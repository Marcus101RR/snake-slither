cmake_minimum_required(VERSION 3.16)
project(SnakeSlither C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Desktop: use an installed SDL2 if there is one, otherwise download and build it.
option(SNAKE_FETCH_SDL2 "Always download SDL2 instead of using an installed copy" OFF)

add_executable(snake src/main.cpp src/stb_image_impl.cpp)
target_include_directories(snake PRIVATE third_party)
if(WIN32)
  # Red apple icon for snake.exe in Explorer and the taskbar.
  target_sources(snake PRIVATE res/snake.rc)
endif()

if(EMSCRIPTEN)
  # Emscripten ships its own SDL2 port; output snake.html/.js/.wasm.
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  target_compile_options(snake PRIVATE -sUSE_SDL=2)
  target_link_options(snake PRIVATE -sUSE_SDL=2 --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
    --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets)
  # Relink when an asset changes so snake.data stays current.
  file(GLOB SNAKE_ASSETS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/*)
  set_property(TARGET snake APPEND PROPERTY LINK_DEPENDS ${SNAKE_ASSETS})
else()
  if(NOT SNAKE_FETCH_SDL2)
    find_package(SDL2 QUIET)
  endif()

  if(SDL2_FOUND)
    message(STATUS "Using installed SDL2")
    if(TARGET SDL2::SDL2)
      if(TARGET SDL2::SDL2main)
        target_link_libraries(snake PRIVATE SDL2::SDL2main)
      endif()
      target_link_libraries(snake PRIVATE SDL2::SDL2)
    else()
      target_include_directories(snake PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(snake PRIVATE ${SDL2_LIBRARIES})
    endif()
  else()
    message(STATUS "SDL2 not found, downloading it (first configure takes a minute)")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-2.32.10
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL2)
    if(TARGET SDL2::SDL2main)
      target_link_libraries(snake PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(snake PRIVATE SDL2::SDL2-static)
  endif()

  # Copy assets next to the executable so the game finds them wherever it runs.
  add_custom_command(TARGET snake POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:snake>/assets)

  if(MSVC)
    set_target_properties(snake PROPERTIES WIN32_EXECUTABLE ON)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT snake)
  endif()
endif()
