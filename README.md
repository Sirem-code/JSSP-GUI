# JSSP GUI Solver 

This app is made in C++ and uses OpenGL for GUI. The data folder contains `15 JSSP instances`, you may add your own by copy and pasting JSSP to a txt file and then placing it in the data folder.
You can use the following website for `JSSP instances`: https://people.brunel.ac.uk/~mastjjb/jeb/orlib/files/jobshop1.txt

# Features

- Greedy solving algorithm
- Parametrized GA solver (you can now swap between Greedy and GA using the selector box)
- Full fledged Gantt chart for solution visualization implemented with OpenGL and imgui
- Solution export (txt format)
- File parser for JSSP
- Characteristics tab for any given JSSP
- Unit tests with catch2

# SETUP - Visual Studio 2026

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

# Unit tests

1. Open test.slnx, right-click project node and open __Properties__

2. __C/C++ -> General__
   - Add `Sirem-code\JSSP-GUI\tests\catch2` and `Sirem-code\JSSP-GUI\JSSP` in `Additional Include Directories`
   - Build tests

# Notes

About the GA: 

Start with small values (e.g. `50 population size`, `50 generations`, `0.10 mutation rate` and `2 tournament size`) to avoid crashes. This solver is `very resource intensive` but can provide `drastically better results` in comparison to the classic greedy implementation. 
