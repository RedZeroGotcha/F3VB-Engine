cmake_minimum_required(VERSION 3.15)
project(F3VBViewer CXX)
if(WIN32)
  enable_language(RC)   # v21: icone e versao do F3VB.exe (res/f3vb.rc)
endif()

set(CMAKE_CXX_STANDARD 20)      # v17: a VM AMX (../src/amx) usa std::span
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mesmo estilo do Futebol3D: C++20 desde o v17 (era C++14), Win32 + OpenGL 1.x, sem bibliotecas externas, runtime estatico (/MT)
if(MSVC)
  add_compile_options(/W3 /EHsc /O2)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)   # fopen/localtime etc. sao usados de forma segura aqui
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Nucleo portavel (leitor F3M, cena, animacao, skinning, analise e log): sem Windows nem OpenGL
# v20: + leitura DIRETA da demo original (vfs: resource.rht + .grp) e montagem de mapas em memoria (tre + map_build: .8tre, .map, .rle)
# v20 (modelos 3D direto da demo): gr2 (Granny 2, porte C++ do blendergranny, MIT), b3d, model_build (.b3d+.gr2 -> cena), proto (.CRT/.ARM/.USE...),
# actor (personagem montado: corpo + partes + arma + animacao parada)
# v21: os fontes sao UTF-8; sem /utf-8 o MSVC le como cp1252 e textos com acento saem quebrados (titulo "tÃ©cnica" no v20)
if(MSVC)
  add_compile_options(/utf-8)
endif()

add_library(f3v_core STATIC src/f3m.cpp src/scene.cpp src/analyze.cpp src/log.cpp src/f3map.cpp src/f3tex.cpp src/vfs.cpp src/tre.cpp src/map_build.cpp
            src/gr2.cpp src/b3d.cpp src/model_build.cpp src/proto.cpp src/actor.cpp src/image.cpp src/ground.cpp)   # image: TGA/BMP/DDS (texturas dos modelos)
target_include_directories(f3v_core PUBLIC src)

# v17: VM AMX (Pawn/Small classico), camada de natives, .stf e sessao de dialogo (mesmas fontes do projeto principal, em ../src)
add_library(f3v_script STATIC src/stf.cpp src/script_world.cpp ../src/amx/amx_vm.cpp ../src/host/game_host.cpp)
target_include_directories(f3v_script PUBLIC src)
target_link_libraries(f3v_script PUBLIC f3v_core)

# v19: interface do jogo original, parte PORTAVEL (sem Windows nem OpenGL): leitor .int, skins .ini, TGA/BMP, TrueType proprio, layout,
# recursos, backend de CPU (PNG) e as regras da criacao de personagem. Testavel no Linux com g++ (tools/py/tests e tests/test_ui.cpp).
add_library(f3v_ui STATIC src/gui_int.cpp src/skin.cpp src/ttf.cpp src/gui.cpp src/ui_assets.cpp src/ui_raster.cpp src/chargen.cpp src/audio_mix.cpp)
target_include_directories(f3v_ui PUBLIC src)
target_link_libraries(f3v_ui PUBLIC f3v_script f3v_core)

# v19: desenha qualquer tela .int em PNG sem janela:  F3VBUiPreview Mainmenu [--hover NewGameBtn]  |  F3VBUiPreview --all --out pasta
add_executable(F3VBUiPreview src/uipreview_main.cpp)
target_link_libraries(F3VBUiPreview PRIVATE f3v_ui)

# v19: testes do nucleo da interface (dados sinteticos: TGA/BMP, .int, skin, fonte TrueType minima, layout, criacao de personagem)
add_executable(test_ui tests/test_ui.cpp)
target_link_libraries(test_ui PRIVATE f3v_ui)

# v20: confere a leitura direta da demo (todos os recursos; comparacao byte a byte com a copia extraida) e monta todos os mapas em C++
#   F3VBCheck --demo "<pasta da demo>" [--assets "<original_assets>"] [--maps [--f3map "<original_assets>\CONVERTED\maps"]]
add_executable(F3VBCheck src/check_main.cpp)
target_link_libraries(F3VBCheck PRIVATE f3v_script f3v_core)   # v21: --tudo roda os scripts e dialogos na VM
# v20: testes sinteticos da Vfs (.rht/.grp falsos), do montador de mapas e do mixer de som
add_executable(test_v20 tests/test_v20.cpp)
target_link_libraries(test_v20 PRIVATE f3v_ui)
# v21: criacao de personagem contra os 3 prints do original
add_executable(test_v21 tests/test_v21.cpp)
target_link_libraries(test_v21 PRIVATE f3v_ui)

# v17: roda um script .amx sem janela e imprime o dialogo / levantamento (depuracao):  F3VBScript ARQ.amx [--stf T.stf] [--choices 1,2] [--survey]
add_executable(F3VBScript src/script_main.cpp)
target_link_libraries(F3VBScript PRIVATE f3v_script)

if(WIN32)
  add_executable(F3VBViewer WIN32 src/viewer_main.cpp)
  target_link_libraries(F3VBViewer PRIVATE f3v_core opengl32 user32 gdi32 comdlg32 shell32)
  # v15: segundo executavel: escolher um mapa (.f3map) e ve-lo em 3D (geometria .8tre + planta .rle + entidades, navegacao, gatilhos...)
  add_executable(F3VBMapViewer WIN32 src/mapviewer_main.cpp)
  target_link_libraries(F3VBMapViewer PRIVATE f3v_script f3v_core opengl32 user32 gdi32 comdlg32)
  # O JOGO (v19; v20: le a demo original direto, seletor de pasta, mixer de som, camera do sys.ini, MSAA/anisotropico)
  # shell32/ole32/uuid: seletor de pastas do Windows (IFileOpenDialog); winmm: som (waveOut). Tudo parte do Windows.
  # v20: + actor_gl (desenho dos personagens) e video_player (videos de abertura, decodificados pelo F3VBVideo32.exe)
  # v21: + res/f3vb.rc (icone com a logo do F3VB Engine e informacoes de versao)
  add_executable(F3VB WIN32 src/game_main.cpp src/map_render.cpp src/gui_gl.cpp src/audio.cpp src/win_platform.cpp src/actor_gl.cpp src/video_player.cpp res/f3vb.rc)
  target_link_libraries(F3VB PRIVATE f3v_ui f3v_script f3v_core opengl32 user32 gdi32 winmm shell32 ole32 uuid)
  # v20: o ajudante de video (src/video_helper32.cpp) e de 32 BITS (a binkw32.dll da demo e de 32 bits): o build.bat compila ele a parte,
  # com o compilador x86 (vcvarsall x86). Aqui nao entra, porque esta configuracao e de 64 bits.
endif()

# Teste do nucleo (roda em qualquer sistema). Precisa do fixture gerado por tests/make_fixture.py:
#   python tests/make_fixture.py build/fixture && build/test_core build/fixture
add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE f3v_core)

# v12: log, analise de modelo, escala malha x esqueleto, mesh_scale (aceita a pasta do fixture para o teste de mutacoes)
add_executable(test_v12 tests/test_v12.cpp)
target_link_libraries(test_v12 PRIVATE f3v_core)

# Ferramenta de linha de comando: gera o MESMO relatorio do log do visualizador, sem abrir janela
#   f3v_analyze modelo.f3m [animacoes.f3m ...] [--scale=X] [--out=relatorio.txt]
add_executable(f3v_analyze src/analyze_main.cpp)
target_link_libraries(f3v_analyze PRIVATE f3v_core)

# v15: leitor do .f3map (fixture: python tests/make_map_fixture.py build/mapfx && build/test_f3map build/mapfx)
add_executable(test_f3map tests/test_f3map.cpp)
target_link_libraries(test_f3map PRIVATE f3v_core)

# v17: scripts + dialogo (sem dados do jogo: o proprio teste monta scripts AMX sinteticos)   build/test_script
add_executable(test_script tests/test_script.cpp)
target_link_libraries(test_script PRIVATE f3v_script)
