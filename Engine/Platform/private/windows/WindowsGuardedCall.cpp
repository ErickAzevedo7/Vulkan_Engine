#include "platform/GuardedCall.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <malloc.h>

#include <cstdio>

namespace VKE::Platform {

    namespace {
        constexpr DWORD kCppException = 0xE06D7363;

        struct FaultRecord {
            DWORD Code = 0;
            void* Address = nullptr;
            ULONG_PTR Access = 0;
            ULONG_PTR Target = 0;
        };

        // Runs on the stack that faulted, which may have no room left: only copies.
        int CaptureFault(EXCEPTION_POINTERS* pointers, FaultRecord& record) {
            const EXCEPTION_RECORD& exception = *pointers->ExceptionRecord;
            record.Code = exception.ExceptionCode;
            record.Address = exception.ExceptionAddress;
            if (exception.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && exception.NumberParameters >= 2) {
                record.Access = exception.ExceptionInformation[0];
                record.Target = exception.ExceptionInformation[1];
            }
            return EXCEPTION_EXECUTE_HANDLER;
        }

        // On its own: a function with __try can't hold objects with destructors.
        bool RunGuarded(void (*call)(void*), void* data, FaultRecord& record) {
            __try {
                call(data);
                return true;
            } __except (CaptureFault(GetExceptionInformation(), record)) {
                return false;
            }
        }

        std::string DescribeCode(const FaultRecord& record) {
            char text[96];
            switch (record.Code) {
            case EXCEPTION_ACCESS_VIOLATION: {
                const char* access = record.Access == 0 ? "reading" : record.Access == 1 ? "writing" : "running";
                if (record.Target < 0x10000) return std::string(access) + " through a null pointer";
                std::snprintf(text, sizeof(text), "%s through a bad pointer (0x%llx)", access,
                              static_cast<unsigned long long>(record.Target));
                return text;
            }
            case EXCEPTION_INT_DIVIDE_BY_ZERO: return "a whole number divided by zero";
            case EXCEPTION_STACK_OVERFLOW: return "a stack overflow, usually a function calling itself forever";
            case EXCEPTION_ILLEGAL_INSTRUCTION: return "an invalid instruction";
            case kCppException: return "a C++ exception nobody caught";
            default: break;
            }
            std::snprintf(text, sizeof(text), "a system fault (0x%08lX)", record.Code);
            return text;
        }

        // "Game-1a2b.dll+0x1234", or nothing if it's not in a module.
        std::string DescribeAddress(void* address) {
            HMODULE module = nullptr;
            constexpr DWORD kFlags =
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
            if (!GetModuleHandleExW(kFlags, static_cast<LPCWSTR>(address), &module)) {
                return {};
            }
            char path[MAX_PATH] = {};
            GetModuleFileNameA(module, path, MAX_PATH);
            const char* name = path;
            for (const char* c = path; *c; ++c) {
                if (*c == '\\' || *c == '/') name = c + 1;
            }
            char text[MAX_PATH + 32];
            auto offset = static_cast<unsigned long long>(static_cast<char*>(address) - reinterpret_cast<char*>(module));
            std::snprintf(text, sizeof(text), " at %s+0x%llx", name, offset);
            return text;
        }
    } // namespace

    bool GuardedCall(void (*call)(void* data), void* data, std::string& outFault) {
        FaultRecord record;
        if (RunGuarded(call, data, record)) return true;

        // The guard page of the stack was used up: without it the next overflow ends the process.
        if (record.Code == EXCEPTION_STACK_OVERFLOW) _resetstkoflw();
        outFault = DescribeCode(record) + DescribeAddress(record.Address);
        return false;
    }

} // namespace VKE::Platform
