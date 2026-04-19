# Agent Guide for Vulkan_Engine

This file gives coding agents a fast, project-specific playbook for making safe, useful changes.

## Project Snapshot

- Language/toolchain: C++17, Visual Studio solution/projects (`.sln`/`.vcxproj`), Vulkan.
- Main app project: `Vulkan_Engine/Vulkan_Engine.vcxproj`.
- Dependency manager: vcpkg manifest mode (`Vulkan_Engine/vcpkg.json`).
- Typical platform/config: `x64` + `Debug` or `Release`.

## Repository Layout

- `Vulkan_Engine/` - main engine/editor source (rendering, ECS-style components, managers, UI).
- `GameScripts/` - gameplay script DLL project loaded by engine.
- `projects/` - output location for scripts, assets, and other project files.
- `Vulkan_Engine/shaders/` - GLSL shaders and `compile.bat` for SPIR-V generation.
- `README.md` - local setup/prerequisites overview.

## Build and Run

Prefer Visual Studio for local iteration, but CLI builds are possible.

### Prerequisites

1. Visual Studio 2022+ with C++ workload.
2. Vulkan SDK installed and `VULKAN_SDK` available.
3. vcpkg installed (see `README.md`).

### CLI Build (PowerShell)

From repo root:

```powershell
msbuild .\Vulkan_Engine.sln /m /p:Configuration=Debug /p:Platform=x64
```

Release build:

```powershell
msbuild .\Vulkan_Engine.sln /m /p:Configuration=Release /p:Platform=x64
```

### Shader Compilation

If shaders change, run:

```powershell
cd .\Vulkan_Engine\shaders
.\compile.bat
```

## Script System Notes (GameScripts)

- `GameScripts/autoreg.ps1` auto-generates `GameScripts/registerScripts.cpp` during pre-build.
- Do not hand-edit `GameScripts/registerScripts.cpp` unless you intentionally disable regeneration.
- Script DLL output is configured to `projects\GameScripts.dll`.

## Coding Conventions

- Formatting is defined in `.clang-format` (tabs, width 4, column limit 120, LLVM-based).
- Keep code C++17-compatible unless project files are updated accordingly.
- Follow existing include and folder patterns in nearby files.
- Avoid introducing new third-party dependencies unless necessary; prefer existing vcpkg setup.

## Validation Checklist for Changes

After code edits, agents should:

1. Build `Debug|x64` successfully.
2. Build `Release|x64` if touching low-level/rendering/build configuration code.
3. Recompile shaders if shader source changed.
4. Confirm `GameScripts` still builds if touching script-facing APIs.

## Safety and Scope

- Keep changes focused; avoid unrelated refactors.
- Do not modify generated, binary, or cache artifacts unless the task requires it.
- Prefer editing source/config files over committing logs or build output.
