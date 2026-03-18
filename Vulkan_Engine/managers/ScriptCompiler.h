#pragma once
#include <functional>
#include <string>
#include <thread>
#include <atomic>

/// ScriptCompiler
///
/// Compiles a single script header into its own .dll using cl.exe.
/// Compilation runs on a background thread so the editor stays responsive.
/// 
/// Usage:
///   ScriptCompiler::compileAsync("path/to/MyScript.h",
///       [](bool ok, const std::string& dllPath) {
///           if (ok) ScriptPluginLoader::loadPlugin(dllPath);
///       });
class ScriptCompiler {
public:
    /// Returns true while a compile is in progress.
    static bool isCompiling();

    /// Returns the name of the script currently being compiled (for UI).
    static std::string compilingScriptName();

    /// Asynchronously compiles a header into a per-script DLL.
    /// 'callback' is called on the MAIN thread on the next call to tick().
    using Callback = std::function<void(bool success, const std::string& dllPath)>;
    static void compileAsync(const std::string& headerPath, Callback callback);

    /// Must be called every frame from the main loop to fire pending callbacks.
    static void tick();

    /// Ensures any background compilation threads are joined.
    static void shutdown();

    /// Generates/updates ScriptCompilerConfig.json in the projects/ folder.
    /// Called once on engine startup to detect cl.exe and engine paths.
    static void setupConfig(const std::string& projectsDir,
                            const std::string& engineRoot,
                            const std::string& engineLibPath,
                            const std::string& glmInclude);

private:
    static bool loadConfig(const std::string& projectsDir);
    static bool compileSync(const std::string& headerPath, std::string& outDllPath);

    // Config (loaded from JSON)
    static std::string s_clExe;
    static std::string s_vcvarsall;
    static std::string s_engineRoot;
    static std::string s_engineLib;
    static std::string s_glmInclude;
    static std::string s_outputDir;

    // State
    static std::atomic<bool>  s_compiling;
    static std::string        s_compilingName;
    static std::thread        s_thread;

    // Pending result to deliver on main thread
    struct PendingResult { bool ok; std::string dllPath; Callback cb; };
    static std::atomic<bool>  s_resultReady;
    static PendingResult      s_pendingResult;
};
