#include "ScriptCompiler.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include "ScriptPluginLoader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

// Tiny JSON helpers (no external dependency needed for this simple config)
namespace {
// Reads a key:"value" pair from a simple flat JSON string
std::string jsonGet(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + search.size()) + 1;
    auto end = json.find('"', pos);
    return json.substr(pos, end - pos);
}
std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}
} // namespace

// ---------------------------------------------------------------------------
// Statics
// ---------------------------------------------------------------------------
std::string          ScriptCompiler::s_clExe;
std::string          ScriptCompiler::s_vcvarsall;
std::string          ScriptCompiler::s_engineRoot;
std::string          ScriptCompiler::s_engineLib;
std::string          ScriptCompiler::s_glmInclude;
std::string          ScriptCompiler::s_outputDir;
std::atomic<bool>    ScriptCompiler::s_compiling{false};
std::string          ScriptCompiler::s_compilingName;
std::thread          ScriptCompiler::s_thread;
std::atomic<bool>    ScriptCompiler::s_resultReady{false};
ScriptCompiler::PendingResult ScriptCompiler::s_pendingResult;

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
void ScriptCompiler::setupConfig(const std::string& projectsDir,
                                  const std::string& engineRoot,
                                  const std::string& engineLibPath,
                                  const std::string& glmInclude) {
    namespace fs = std::filesystem;

    // Try to auto-detect cl.exe and vcvarsall.bat via vswhere
    std::string clExe;
    std::string vcvarsall;
    {
        const char* vswhereLocations[] = {
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\Installer\\vswhere.exe",
        };
        for (const char* vswhere : vswhereLocations) {
            if (!fs::exists(vswhere)) continue;

            // Find cl.exe
            {
                std::string cmd = std::string("\"") + vswhere +
                    "\" -latest -requires Microsoft.VisualCpp.Tools.HostX64.TargetX64"
                    " -find VC\\Tools\\MSVC\\**\\bin\\HostX64\\x64\\cl.exe 2>NUL";
                FILE* pipe = _popen(cmd.c_str(), "r");
                if (pipe) {
                    char buf[512] = {};
                    if (fgets(buf, sizeof(buf), pipe)) {
                        clExe = buf;
                        while (!clExe.empty() && (clExe.back() == '\n' || clExe.back() == '\r'))
                            clExe.pop_back();
                    }
                    _pclose(pipe);
                }
            }

            // Derive vcvarsall.bat from the VS installation path (always at a known relative location)
            {
                std::string cmd = std::string("\"") + vswhere +
                    "\" -latest -property installationPath 2>NUL";
                FILE* pipe = _popen(cmd.c_str(), "r");
                if (pipe) {
                    char buf[512] = {};
                    if (fgets(buf, sizeof(buf), pipe)) {
                        vcvarsall = buf;
                        while (!vcvarsall.empty() && (vcvarsall.back() == '\n' || vcvarsall.back() == '\r'))
                            vcvarsall.pop_back();
                        // vcvarsall.bat is always at <InstallDir>\VC\Auxiliary\Build\vcvarsall.bat
                        vcvarsall += "\\VC\\Auxiliary\\Build\\vcvarsall.bat";
                        if (!fs::exists(vcvarsall)) vcvarsall.clear();
                    }
                    _pclose(pipe);
                }
            }

            if (!clExe.empty()) break;
        }
    }

    if (clExe.empty()) {
        std::cerr << "[ScriptCompiler] WARNING: could not auto-detect cl.exe. "
                     "Edit ScriptCompilerConfig.json manually.\n";
        clExe = "<cl.exe not found — fill in manually>";
    }

    // Write config
    std::string outputDir = (fs::path(projectsDir) / "__compiled").string();
    fs::create_directories(outputDir);

    std::string configPath = (fs::path(projectsDir) / "ScriptCompilerConfig.json").string();
    std::ofstream f(configPath);
    f << "{\n"
      << "  \"cl_exe\":      \"" << jsonEscape(clExe)          << "\",\n"
      << "  \"vcvarsall\":    \"" << jsonEscape(vcvarsall)      << "\",\n"
      << "  \"engine_root\": \"" << jsonEscape(engineRoot)      << "\",\n"
      << "  \"engine_lib\":  \"" << jsonEscape(engineLibPath)   << "\",\n"
      << "  \"glm_include\": \"" << jsonEscape(glmInclude)      << "\",\n"
      << "  \"output_dir\":  \"" << jsonEscape(outputDir)       << "\"\n"
      << "}\n";

    // Also load into memory
    s_clExe       = clExe;
    s_vcvarsall   = vcvarsall;
    s_engineRoot  = engineRoot;
    s_engineLib   = engineLibPath;
    s_glmInclude  = glmInclude;
    s_outputDir   = outputDir;

    std::cout << "[ScriptCompiler] Config written to: " << configPath << "\n";
}

bool ScriptCompiler::loadConfig(const std::string& projectsDir) {
    std::string configPath = (std::filesystem::path(projectsDir) / "ScriptCompilerConfig.json").string();
    std::ifstream f(configPath);
    if (!f) return false;
    std::string json((std::istreambuf_iterator<char>(f)), {});
    s_clExe      = jsonGet(json, "cl_exe");
    s_vcvarsall   = jsonGet(json, "vcvarsall");
    s_engineRoot = jsonGet(json, "engine_root");
    s_engineLib  = jsonGet(json, "engine_lib");
    s_glmInclude = jsonGet(json, "glm_include");
    s_outputDir  = jsonGet(json, "output_dir");
    return !s_clExe.empty();
}

// ---------------------------------------------------------------------------
// Compile (synchronous — called from background thread)
// ---------------------------------------------------------------------------
bool ScriptCompiler::compileSync(const std::string& headerPath, std::string& outDllPath) {
    namespace fs = std::filesystem;

    fs::path header(headerPath);
    std::string scriptName = header.stem().string(); // e.g. "PlayerScript"

    // Output DLL path
    outDllPath = (fs::path(s_outputDir) / (scriptName + ".dll")).string();

    // Generate a minimal wrapper .cpp
    std::string wrapperPath = (fs::path(s_outputDir) / (scriptName + "_wrap.cpp")).string();
    {
        std::ofstream w(wrapperPath);
        w << "// Auto-generated wrapper for " << scriptName << "\n";
        w << "#include \"" << headerPath << "\"\n";
        w << "\nextern \"C\" __declspec(dllexport) void registerScripts() {}\n";
    }

    // Build the command
    // We call vcvarsall.bat x64 to set up the environment (standard headers, libs, etc.)
    // then call cl.exe. Use '&&' to run them in sequence in the same shell.
    std::ostringstream cmd;
    cmd << "cmd /C \"";
    if (!s_vcvarsall.empty()) {
        cmd << "call \"" << s_vcvarsall << "\" x64 && ";
    }
    cmd << "\"" << s_clExe << "\""
        << " /nologo /LD /EHsc /std:c++17 /MDd"
        << " /DGLM_ENABLE_EXPERIMENTAL /DGLM_FORCE_DEPTH_ZERO_TO_ONE"
        << " /I\"" << s_engineRoot << "\""
        << " /I\"" << s_glmInclude << "\""
        << " \"" << wrapperPath << "\"";

    // Detect and add peer .cpp file if it exists
    std::string cppPath = header.replace_extension(".cpp").string();
    if (fs::exists(cppPath)) {
        cmd << " \"" << cppPath << "\"";
    }

    cmd << " /Fe\"" << outDllPath << "\""
        << " /link \"" << s_engineLib << "\""
        << " /SUBSYSTEM:WINDOWS"
        << " /OUT:\"" << outDllPath << "\""
        << "\"";

    std::string logPath = (fs::path(s_outputDir) / (scriptName + "_build.log")).string();
    std::string fullCmd = cmd.str() + " > \"" + logPath + "\" 2>&1";

    std::cout << "[ScriptCompiler] Compiling: " << scriptName << "\n";
    int ret = std::system(fullCmd.c_str());

    if (ret != 0) {
        std::cerr << "[ScriptCompiler] Compile FAILED for '" << scriptName
                  << "'. See: " << logPath << "\n";
        return false;
    }

    std::cout << "[ScriptCompiler] Done: " << outDllPath << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
bool ScriptCompiler::isCompiling() {
    return s_compiling.load();
}

std::string ScriptCompiler::compilingScriptName() {
    return s_compilingName;
}

void ScriptCompiler::compileAsync(const std::string& headerPath, Callback callback) {
    if (s_compiling.load()) {
        std::cerr << "[ScriptCompiler] Already compiling — request ignored.\n";
        return;
    }

    namespace fs = std::filesystem;

    // Try loading config lazily
    if (s_clExe.empty()) {
        // Attempt to find the config next to the exe
        char exeBuf[MAX_PATH] = {};
#ifdef _WIN32
        GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
        fs::path projectsDir = fs::path(exeBuf).parent_path() / ".." / ".." / "projects";
        projectsDir = fs::weakly_canonical(projectsDir);
        if (!loadConfig(projectsDir.string())) {
            std::cerr << "[ScriptCompiler] Config not found. Call setupConfig() first.\n";
            return;
        }
#endif
    }

    s_compilingName = fs::path(headerPath).stem().string();
    s_compiling.store(true);
    s_resultReady.store(false);

    if (s_thread.joinable()) s_thread.join();
    s_thread = std::thread([headerPath, callback]() {
        std::string dllPath;
        bool ok = compileSync(headerPath, dllPath);
        s_pendingResult = { ok, dllPath, callback };
        s_resultReady.store(true);
        s_compiling.store(false);
    });
}

void ScriptCompiler::tick() {
    if (s_resultReady.load()) {
        s_resultReady.store(false);
        auto result = s_pendingResult;
        result.cb(result.ok, result.dllPath);
    }
}

void ScriptCompiler::shutdown() {
    if (s_thread.joinable()) {
        s_thread.join();
    }
}

bool ScriptCompiler::loadFromHeader(const std::string& headerPath) {
    if (headerPath.empty()) return false;
    
    namespace fs = std::filesystem;
    fs::path header(headerPath);
    std::string scriptName = header.stem().string();
    
    // If output dir is unknown, we can't load
    if (s_outputDir.empty()) {
        // Try to load config if we have a projectsDir nearby
        // (This happens on cold start before any compile)
        char exeBuf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
        fs::path projectsDir = fs::path(exeBuf).parent_path() / ".." / ".." / "projects";
        projectsDir = fs::weakly_canonical(projectsDir);
        if (!loadConfig(projectsDir.string())) return false;
    }
    
    fs::path dllPath = fs::path(s_outputDir) / (scriptName + ".dll");
    if (fs::exists(dllPath)) {
        return ScriptPluginLoader::loadPlugin(dllPath.string());
    }
    
    return false;
}
