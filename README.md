# Vulkan Engine

This repository contains a Vulkan-based rendering engine with ImGui/ImGuizmo integration.  

---

## 1. Prerequisites

Install the following:

1. **Visual Studio 2022+ / 2026**

2. **Vulkan SDK**
   - Download and install from:  
     https://vulkan.lunarg.com/sdk/home
   - During install, let it:
     - Set `VULKAN_SDK` environment variable
     - Add Vulkan binaries to `PATH`

3. **vcpkg**
   - Clone vcpkg somewhere on your machine:
     ```bash
     git clone https://github.com/microsoft/vcpkg.git
     ```
   - Bootstrap:
     ```bash
     .\vcpkg\bootstrap-vcpkg.bat
     ```
   - Optionally set `VCPKG_ROOT`:
     ```powershell
     setx VCPKG_ROOT "C:\path\to\vcpkg"
     ```

The repo already contains `vcpkg-configuration.json` and `vcpkg.json`, so dependencies will be restored automatically by Visual Studio/vcpkg.

---

## 2. Get the Source

- Clone this repository:
    ```bash
     git clone https://github.com/ErickAzevedo7/Vulkan_Engine.git
     cd Vulkan_Engine
     ```
---

## 3. Install Dependencies (vcpkg)

From the repo root:
    ```bash
     .\path\to\vcpkg\vcpkg integrate install
     ```

Visual Studio will use `vcpkg-configuration.json` and `vcpkg.json` to install required libraries on first build (e.g. imgui, imguizmo).

No manual package list is needed.

---

## 4. Build and Run (Visual Studio)

1. Open `Vulkan_Engine` in **Visual Studio**:
   - `File` → `Open` → `Project/Solution...`
   - Select `Vulkan_Engine.sln` (if present), or open the folder if using CMake.

2. Select configuration:
   - `x64-Debug` or `x64-Release`.

3. Build:
   - `Build` → `Build Solution`.

4. Run:
   - Set the main executable as the startup project.
   - Press `F5` to run with debugger or `Ctrl+F5` to run without.

---