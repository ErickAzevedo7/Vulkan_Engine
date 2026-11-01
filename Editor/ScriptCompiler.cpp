#include "ScriptCompiler.h"
#include "DevTools.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace VKE::Editor {

    namespace {
        constexpr auto kLookInterval = std::chrono::milliseconds(200);
        constexpr size_t kLinesOfFailure = 12;
        constexpr const char* kStampFile = "VkeBuild.txt";

        bool IsBuildDescription(const std::filesystem::path& file) {
            return file.extension() == ".cmake" || file.filename() == "CMakeLists.txt";
        }

        bool MattersToBuild(const std::filesystem::path& file) {
            std::string extension = file.extension().u8string();
            return extension == ".cpp" || extension == ".h" || extension == ".hpp" || extension == ".inl" ||
                   IsBuildDescription(file);
        }

        bool IsSource(const std::filesystem::path& file) {
            return file.extension() == ".cpp" || file.extension() == ".h";
        }

        std::string ReadFile(const std::filesystem::path& file) {
            std::ifstream stream(file, std::ios::binary);
            std::ostringstream text;
            text << stream.rdbuf();
            return text.str();
        }

        std::vector<std::string> SplitLines(const std::string& text) {
            std::vector<std::string> lines;
            std::istringstream stream(text);
            std::string line;
            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) lines.push_back(line);
            }
            return lines;
        }

        // The variables `set` listed, in UTF-16, as "NAME=value" in UTF-8.
        std::vector<std::string> ParseEnvironment(const std::string& output) {
            std::wstring wide(output.size() / sizeof(wchar_t), L'\0');
            std::memcpy(wide.data(), output.data(), wide.size() * sizeof(wchar_t));

            std::vector<std::string> variables;
            std::wistringstream stream(wide);
            std::wstring line;
            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == L'\r') line.pop_back();
                if (line.find(L'=') != std::wstring::npos) variables.push_back(std::filesystem::path(line).u8string());
            }
            return variables;
        }

        std::string FindVariable(const std::vector<std::string>& environment, const char* name) {
            size_t length = std::strlen(name);
            for (const std::string& variable : environment) {
                if (variable.size() <= length || variable[length] != '=') continue;
                bool same = std::equal(name, name + length, variable.begin(), [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
                if (same) return variable.substr(length + 1);
            }
            return {};
        }

        // How serious a line of the tools' output is: they all read "<where>: <kind> <what>".
        std::optional<LogLevel> FindLevel(const std::string& line) {
            const std::pair<const char*, LogLevel> kinds[] = {{": fatal error", LogLevel::Error},
                                                              {": error", LogLevel::Error},
                                                              {": warning", LogLevel::Warning},
                                                              {": note", LogLevel::Info}};
            for (const auto& [kind, level] : kinds) {
                size_t found = line.find(kind);
                if (found == std::string::npos) continue;
                char next = line[std::min(found + std::strlen(kind), line.size() - 1)];
                if (next == ' ' || next == ':') return level;
            }
            return std::nullopt;
        }
    } // namespace

    ScriptCompiler::ScriptCompiler(const Project::Project& project, std::filesystem::path sdkFolder)
        : m_ScriptsFolder(project.GetScriptsFolder()),
          m_BuildFolder(project.GetGameBuildFolder()),
          m_Library(project.GetGameLibrary()),
          m_SdkFolder(std::move(sdkFolder)) {
    }

    ScriptCompiler::~ScriptCompiler() {
        Stop();
    }

    void ScriptCompiler::Start() {
        m_Thread = std::thread([this] { Watch(); });
    }

    void ScriptCompiler::Stop() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Stopping = true;
            if (m_Running) m_Running->Stop();
        }
        m_Wake.notify_all();
        if (m_Thread.joinable()) m_Thread.join();
    }

    std::optional<ScriptCompiler::Result> ScriptCompiler::TakeResult() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        std::optional<Result> result;
        result.swap(m_Result);
        return result;
    }

    bool ScriptCompiler::Tick() {
        std::unique_lock<std::mutex> lock(m_Mutex);
        return !m_Wake.wait_for(lock, kLookInterval, [this] { return m_Stopping; });
    }

    void ScriptCompiler::Watch() {
        Files built;
        bool first = true;
        while (Tick()) {
            Files files = ScanFiles();
            if (!first && files == built) continue;

            // Files often change in a burst, as on "save all": this waits for it to end.
            if (!Tick()) return;
            if (ScanFiles() != files) continue;

            first = false;
            built = std::move(files);
            std::error_code ec;
            auto isSource = [](const auto& file) { return IsSource(file.first); };
            bool anySource = std::any_of(built.begin(), built.end(), isSource);
            if (anySource || std::filesystem::exists(m_Library, ec)) Build(built);
        }
    }

    ScriptCompiler::Files ScriptCompiler::ScanFiles() const {
        Files files;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(m_ScriptsFolder, ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (!MattersToBuild(it->path())) continue;
            std::error_code ignored;
            files[it->path()] = {it->last_write_time(ignored), it->file_size(ignored)};
        }
        return files;
    }

    void ScriptCompiler::Build(const Files& files) {
        m_State = State::Building;
        auto start = std::chrono::steady_clock::now();
        std::error_code ec;
        auto before = std::filesystem::last_write_time(m_Library, ec);

        Result result;
        if (FindTools(result) && Configure(files, result)) {
            Platform::Process::Child::Desc desc;
            desc.Executable = m_Cmake;
            desc.Arguments = {"--build", m_BuildFolder.u8string()};
            desc.Environment = m_Environment;
            std::string output;
            result.Succeeded = Run(desc, output);
            ReadOutput(output, !result.Succeeded, result);
        }

        result.LibraryChanged = result.Succeeded && std::filesystem::last_write_time(m_Library, ec) != before;
        result.Seconds = std::chrono::duration<f32>(std::chrono::steady_clock::now() - start).count();
        bool succeeded = result.Succeeded;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            // The editor may not have asked for the last one, and it must still load that DLL.
            if (m_Result) result.LibraryChanged |= m_Result->LibraryChanged;
            m_Result = std::move(result);
        }
        m_State = succeeded ? State::Built : State::Failed;
    }

    bool ScriptCompiler::FindTools(Result& result) {
        if (!m_Environment.empty()) return true;

        m_Studio = FindVisualStudio();
        if (m_Studio.empty()) {
            result.Messages.push_back({LogLevel::Error, "There's no C++ compiler on this computer. Install Visual "
                                                        "Studio with the 'Desktop development with C++' workload."});
            return false;
        }

        // Visual Studio's batch file gives the compiler its paths. This keeps them as they are
        // after it runs, in English so messages read the same anywhere, and in UTF-16 to be exact.
        const char* shell = std::getenv("ComSpec");
        std::filesystem::path setup = m_Studio / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
        Platform::Process::Child::Desc desc;
        desc.Executable = shell ? shell : "cmd.exe";
        desc.Arguments = {"/u", "/c", "set", "VSLANG=1033&&", "call", setup.u8string(), ">nul", "2>&1", "&&", "set"};
        std::string output;
        if (!Run(desc, output)) {
            result.Messages.push_back({LogLevel::Error, "Visual Studio's compiler can't be set up: '" +
                                                            setup.u8string() + "' failed."});
            return false;
        }

        std::vector<std::string> environment = ParseEnvironment(output);
        std::string path = FindVariable(environment, "PATH");
        m_Cmake = FindOnPath(path, "cmake.exe");
        m_Ninja = FindOnPath(path, "ninja.exe");
        if (m_Cmake.empty() || m_Ninja.empty()) {
            result.Messages.push_back({LogLevel::Error, "CMake and Ninja aren't there. Add 'C++ CMake tools for "
                                                        "Windows' in the Visual Studio Installer."});
            return false;
        }
        m_Environment = std::move(environment);
        return true;
    }

    bool ScriptCompiler::Configure(const Files& files, Result& result) {
        // A build folder is tied to where the project, the SDK and the compiler were when it was made.
        std::string place = m_ScriptsFolder.u8string() + "\n" + m_SdkFolder.u8string() + "\n" + m_Studio.u8string();
        // A build left to redo this by itself would print what's wrong where it can't be read.
        std::string stamp = place;
        for (const auto& [file, written] : files) {
            if (IsBuildDescription(file)) stamp += "\n" + std::to_string(written.first.time_since_epoch().count());
        }

        std::error_code ec;
        std::string saved = ReadFile(m_BuildFolder / kStampFile);
        if (saved == stamp && std::filesystem::exists(m_BuildFolder / "build.ninja", ec)) return true;
        if (saved.rfind(place, 0) != 0) std::filesystem::remove_all(m_BuildFolder, ec);

        Platform::Process::Child::Desc desc;
        desc.Executable = m_Cmake;
        desc.Arguments = {"-S", m_ScriptsFolder.u8string(), "-B", m_BuildFolder.u8string(), "-G", "Ninja",
                          "-DCMAKE_MAKE_PROGRAM=" + m_Ninja.generic_u8string(), "-DCMAKE_CXX_COMPILER=cl",
                          "-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW",
                          "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded",
                          "-DVKE_SDK_DIR=" + m_SdkFolder.generic_u8string()};
        desc.Environment = m_Environment;
        std::string output;
        if (!Run(desc, output)) {
            ReadOutput(output, true, result);
            return false;
        }
        std::ofstream(m_BuildFolder / kStampFile, std::ios::binary) << stamp;
        return true;
    }

    bool ScriptCompiler::Run(const Platform::Process::Child::Desc& desc, std::string& output) {
        Iunique<Platform::Process::Child> child = Platform::Process::Child::Start(desc);
        if (!child) return false;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Stopping) return false;
            m_Running = child.get();
        }
        while (child->Read(output)) {
        }
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Running = nullptr;
        return child->GetExitCode() == 0;
    }

    void ScriptCompiler::ReadOutput(const std::string& output, bool failed, Result& result) const {
        std::vector<std::string> lines = SplitLines(Platform::Process::ConsoleTextToUtf8(output));
        bool anyError = false;
        for (const std::string& line : lines) {
            std::optional<LogLevel> level = FindLevel(line);
            if (!level) continue;
            anyError |= *level == LogLevel::Error;
            result.Messages.push_back({*level, ShortenPath(line)});
        }
        if (!failed || anyError) return;
        lines.erase(std::remove_if(lines.begin(), lines.end(),
                                   [](const std::string& line) { return line.rfind("-- ", 0) == 0; }),
                    lines.end());
        size_t first = lines.size() > kLinesOfFailure ? lines.size() - kLinesOfFailure : 0;
        for (size_t i = first; i < lines.size(); ++i) {
            result.Messages.push_back({LogLevel::Error, lines[i]});
        }
    }

    std::string ScriptCompiler::ShortenPath(const std::string& text) const {
        std::filesystem::path folder = m_ScriptsFolder;
        folder.make_preferred();
        std::string prefix = folder.u8string() + static_cast<char>(std::filesystem::path::preferred_separator);
        auto sameCharacter = [](char a, char b) {
            if (a == '/') a = '\\';
            if (b == '/') b = '\\';
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        };
        bool inside = text.size() > prefix.size() && std::equal(prefix.begin(), prefix.end(), text.begin(), sameCharacter);
        return inside ? text.substr(prefix.size()) : text;
    }

} // namespace VKE::Editor
