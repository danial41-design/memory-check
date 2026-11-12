# Memory Room

**Memory Room: A 2D Lights-Out Memory Puzzle Game Using OpenGL**

Daniyal Amangeldi, group 2412

The player sees a room for six seconds and memorizes its walls, three keys, and exit. Then the room becomes dark. Collect all keys and reach the exit using memory. Two short flashes reveal the room for 0.5 seconds each.

## Build and run on Windows

1. Open `MemoryRoom.sln` in **Visual Studio**.
2. Select **Debug | x64** and build the solution.
3. Run with **Ctrl+F5** or **F5**.

You can also run `build.bat`, then `run.bat`. The project uses C++17, OpenGL 3.3 Core, GLFW, GLAD, and GLSL 330. The required headers and GLFW library are included in `Libraries/`. No command-line arguments are needed.

| Key | Action |
| --- | --- |
| Enter | Start or play again after winning |
| W/A/S/D or arrow keys | Move |
| Space | Use a short flash |
| R | Restart |
| Esc | Quit |

The dark phase hides the room geometry while keeping its collision map active. The flashes temporarily draw the room again. See `README_RU.txt` for a detailed explanation in Russian and `PROJECT_DESCRIPTION_EN.txt` for an English overview.
