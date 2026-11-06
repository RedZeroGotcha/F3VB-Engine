# =====================================================================
#  RPGGL - build de teste 0.2  (RPG pos-apocaliptico em mundo aberto)
#
#  - Runtime do MSVC LINKADO ESTATICAMENTE (/MT): o .exe nao exige o
#    Visual C++ Redistributable.  Depende apenas de DLLs que ja vem no
#    Windows (kernel32, user32, gdi32, opengl32, winmm).
#  - Com MinGW-w64 tambem funciona (libgcc/libstdc++ estaticos).
#  - Threads: usa as primitivas Win32 diretamente (sys.h), sem depender
#    de std::thread / winpthreads.
#
#  Uso recomendado:  build.bat   (detecta Ninja x Visual Studio sozinho)
#  Teste headless (qualquer SO, sem janela):
#     cmake -S . -B build -DRPGGL_BUILD_TESTS=ON && cmake --build build
#     ./build/rpggl_headless
# =====================================================================
cmake_minimum_required(VERSION 3.15)
project(RPGGL CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
option(RPGGL_BUILD_TESTS "Compila o teste headless (mundo, IA, veiculos, saves)" OFF)

# ---- 1) Nucleo do jogo (sem Windows/OpenGL): mundo, regras, saves, menus ----
add_library(rpggl_core STATIC src/world.cpp src/ragdoll.cpp src/game.cpp src/save.cpp src/ui.cpp)
target_include_directories(rpggl_core PUBLIC src)

# ---- 2) Executavel (Windows): janela, OpenGL, HUD, audio ----------------
if(WIN32)
  add_executable(RPGGL WIN32 src/platform.cpp src/render.cpp src/hud.cpp src/audio.cpp)
  target_link_libraries(RPGGL PRIVATE rpggl_core opengl32 gdi32 user32 winmm)
  if(MSVC)
    set_property(TARGET RPGGL rpggl_core PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    foreach(t RPGGL rpggl_core)
      target_compile_options(${t} PRIVATE /W3 /utf-8 $<$<CONFIG:Release>:/O2>)
      target_compile_definitions(${t} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    endforeach()
  else()  # MinGW-w64
    foreach(t RPGGL rpggl_core)
      target_compile_options(${t} PRIVATE -O2 -Wall)
      target_compile_definitions(${t} PRIVATE NOMINMAX)
    endforeach()
    target_link_options(RPGGL PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
else()
  message(STATUS "Nao e Windows: apenas o nucleo e os testes headless sao compilados.")
  find_package(Threads REQUIRED)
  target_link_libraries(rpggl_core PUBLIC Threads::Threads)
endif()

# ---- 3) Testes headless -------------------------------------------------
if(RPGGL_BUILD_TESTS AND NOT WIN32)
  add_executable(rpggl_headless tests/headless.cpp tests/stub_audio.cpp)
  target_link_libraries(rpggl_headless PRIVATE rpggl_core)
endif()
