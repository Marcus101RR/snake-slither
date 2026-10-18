# Snake Slither

A very basic Snake game in C++ with SDL2. One codebase builds as a desktop app and, via Emscripten, as a WebAssembly page for the browser. The window is 1280x720 on a 32x18 grid.

## Controls

- Arrow keys or WASD: steer
- Space or Enter: restart after game over
- Esc: quit (desktop only)

Eat the red food to grow and score. Hitting a wall or yourself ends the game; the final score shows in the middle of the screen.

## Desktop build in Visual Studio (Windows)

You don't need to install SDL2 yourself. If CMake can't find it, it downloads and builds SDL2 automatically.

1. In the Visual Studio Installer, make sure the **Desktop development with C++** workload is installed. It includes the CMake tools and Git that the build uses.
2. In Visual Studio, use **File > Open > Folder** and pick the `snake-slither` folder (the one containing `CMakeLists.txt`).
3. Wait for the Output window to say CMake generation finished. The first time takes a minute or two while SDL2 downloads and builds.
4. In the startup item dropdown on the toolbar (next to the green play button), choose **snake.exe**.
5. Press **F5** (or Ctrl+F5 to run without the debugger).

If CMake ever gets into a bad state, use **Project > Delete Cache and Reconfigure**.

## Desktop build from the command line (any OS)

Requires CMake 3.16+, a C++17 compiler and Git. SDL2 is used if installed (for example `sudo apt install libsdl2-dev` or `brew install sdl2`), otherwise it's downloaded automatically.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/snake          # Windows: build\Release\snake.exe
```

## Browser build

Requires the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html). Emscripten provides its own SDL2 port, so nothing else is needed.

```sh
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python3 -m http.server -d build-web 8000
```

Then open http://localhost:8000/snake.html. The page must be served over HTTP; opening the file directly won't load the WebAssembly.

## Layout

- `src/main.cpp`: the whole game (input, update, rendering)
- `web/shell.html`: the HTML page template used by the browser build
- `CMakeLists.txt`: build for both targets
