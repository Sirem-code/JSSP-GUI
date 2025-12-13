# JSSP GUI - Visual Studio 2026 Setup

This project uses GLFW (OpenGL) and imgui. If you get include errors, do the following: 

1. Open the project, right-click the project node and open __Properties__ for the active Configuration and Platform.

2. __Linker -> Input__
   - Add `glfw3.lib` to `Additional Dependencies`.

3. __Linker -> General__
   - Add the folder containing the `.lib` (for example `$(ProjectDir)glfw\library`) to `Additional Library Directories`.

4. __VC++ Directories__
   - `Include Directories`: add `$(ProjectDir)glfw\include\GLFW` (or `$(ProjectDir)glfw\include`).
   - `Library Directories`: add `$(ProjectDir)glfw\library`.

5. __C/C++ -> General__
   - Add the `imgui` folder to `Additional Include Directories` (for example `$(ProjectDir)imgui`).

Notes
- Ensure the GLFW library architecture matches your project Platform (x64 vs Win32). Use x64-built `.lib` for x64 builds.
- If using the GLFW DLL, copy `glfw3.dll` to the output folder (for example `$(OutDir)`) or add its location to `PATH`.
- If your GLFW distribution uses a different filename (e.g. `glfw.lib` or `glfw3_static.lib`), add that exact filename instead of `glfw3.lib`.

Optional
- Use `vcpkg` to install a matching GLFW for your toolset: `vcpkg install glfw3:x64-windows` and `vcpkg integrate install`.
