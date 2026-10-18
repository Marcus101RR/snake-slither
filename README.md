# Snake Slither

A very basic Snake game in C++ with SDL2. One codebase builds as a desktop app and, via Emscripten, as a WebAssembly page for the browser. The window is 1280x720 on a 32x18 grid.

## Controls

- Arrow keys or WASD: steer
- Space or Enter: restart after game over
- Esc: quit (desktop only)

Eat the red food to grow and score. Hitting a wall or yourself ends the game; the final score shows in the middle of the screen.

## Desktop build

Requires CMake 3.16+, a C++17 compiler, and SDL2 development files.

- Ubuntu/Debian: `sudo apt install cmake g++ libsdl2-dev`
- macOS (Homebrew): `brew install cmake sdl2`
- Windows (vcpkg): `vcpkg install sdl2`, then pass `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake` to the configure step

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
