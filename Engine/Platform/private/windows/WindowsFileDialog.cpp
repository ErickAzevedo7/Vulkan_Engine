#include "platform/FileDialog.h"
#include "platform/PlatformGlobals.h"
#include "platform/Window.h"

#include <core/Log.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace VKE::Platform::FileDialog {

    namespace {
        std::wstring ToWide(const std::string& text) {
            if (text.empty()) return {};
            int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring wide(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
            return wide;
        }

        HWND GetOwnerWindow() {
            if (!VKE::Window) return nullptr;
            return glfwGetWin32Window(static_cast<GLFWwindow*>(VKE::Window->GetNativeHandle()));
        }

        // Keeps COM initialized on this thread while a dialog is open.
        class ComScope {
        public:
            ComScope() {
                HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
                // Already initialized in another mode still lets the dialog run.
                m_Uninitialize = SUCCEEDED(result);
            }
            ~ComScope() {
                if (m_Uninitialize) CoUninitialize();
            }

        private:
            bool m_Uninitialize = false;
        };

        template<typename T>
        struct ComPtr {
            T* Ptr = nullptr;
            ~ComPtr() {
                if (Ptr) Ptr->Release();
            }
            T** operator&() { return &Ptr; }
            T* operator->() const { return Ptr; }
        };

        void SetStartFolder(IFileDialog& dialog, const std::filesystem::path& folder) {
            if (folder.empty()) return;
            // The shell only parses backslashes.
            std::filesystem::path native = std::filesystem::absolute(folder).make_preferred();
            ComPtr<IShellItem> item;
            if (SUCCEEDED(SHCreateItemFromParsingName(native.c_str(), nullptr, IID_PPV_ARGS(&item)))) {
                dialog.SetFolder(item.Ptr);
            }
        }

        // `text` holds the converted names and patterns, which must outlive the dialog.
        void SetFilters(IFileDialog& dialog, const std::vector<Filter>& filters, std::vector<std::wstring>& text) {
            if (filters.empty()) return;
            text.clear();
            for (const Filter& filter : filters) {
                text.push_back(ToWide(filter.Name));
                text.push_back(ToWide(filter.Pattern));
            }
            std::vector<COMDLG_FILTERSPEC> specs;
            for (size_t i = 0; i < filters.size(); ++i) {
                specs.push_back({text[i * 2].c_str(), text[i * 2 + 1].c_str()});
            }
            dialog.SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
        }

        // Runs `dialog` and returns the chosen path. HRESULT_FROM_WIN32(ERROR_CANCELLED) is the
        // user closing it.
        std::optional<std::filesystem::path> Show(IFileDialog& dialog, const char* what) {
            HRESULT result = dialog.Show(GetOwnerWindow());
            if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return std::nullopt;
            if (FAILED(result)) {
                VKE_CORE_ERROR("FileDialog::{}: the dialog failed (0x{:08x})", what, static_cast<unsigned>(result));
                return std::nullopt;
            }

            ComPtr<IShellItem> item;
            if (FAILED(dialog.GetResult(&item))) return std::nullopt;

            PWSTR path = nullptr;
            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                VKE_CORE_ERROR("FileDialog::{}: the choice is not a file system path", what);
                return std::nullopt;
            }
            std::filesystem::path chosen(path);
            CoTaskMemFree(path);
            return chosen;
        }

        // The extension of the first pattern of the first filter, without "*.", or empty.
        std::wstring DefaultExtension(const std::vector<Filter>& filters) {
            if (filters.empty()) return {};
            std::string pattern = filters.front().Pattern.substr(0, filters.front().Pattern.find(';'));
            if (pattern.rfind("*.", 0) != 0 || pattern.find('*', 2) != std::string::npos) return {};
            return ToWide(pattern.substr(2));
        }
    } // namespace

    std::optional<std::filesystem::path> OpenFile(const std::vector<Filter>& filters,
                                                  const std::filesystem::path& folder) {
        ComScope com;
        ComPtr<IFileOpenDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            VKE_CORE_ERROR("FileDialog::OpenFile: could not create the dialog");
            return std::nullopt;
        }
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
        std::vector<std::wstring> filterText;
        SetFilters(*dialog.Ptr, filters, filterText);
        SetStartFolder(*dialog.Ptr, folder);
        return Show(*dialog.Ptr, "OpenFile");
    }

    std::optional<std::filesystem::path> SaveFile(const std::vector<Filter>& filters, const std::string& defaultName,
                                                  const std::filesystem::path& folder) {
        ComScope com;
        ComPtr<IFileSaveDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            VKE_CORE_ERROR("FileDialog::SaveFile: could not create the dialog");
            return std::nullopt;
        }
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
        std::vector<std::wstring> filterText;
        SetFilters(*dialog.Ptr, filters, filterText);
        std::wstring extension = DefaultExtension(filters);
        if (!extension.empty()) dialog->SetDefaultExtension(extension.c_str());
        if (!defaultName.empty()) dialog->SetFileName(ToWide(defaultName).c_str());
        SetStartFolder(*dialog.Ptr, folder);
        return Show(*dialog.Ptr, "SaveFile");
    }

    std::optional<std::filesystem::path> PickFolder(const std::filesystem::path& folder) {
        ComScope com;
        ComPtr<IFileOpenDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            VKE_CORE_ERROR("FileDialog::PickFolder: could not create the dialog");
            return std::nullopt;
        }
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS);
        SetStartFolder(*dialog.Ptr, folder);
        return Show(*dialog.Ptr, "PickFolder");
    }

} // namespace VKE::Platform::FileDialog
