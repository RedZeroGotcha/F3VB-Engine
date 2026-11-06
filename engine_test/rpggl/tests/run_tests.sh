#!/usr/bin/env bash
# Roda os testes headless com sanitizers. Requer g++ (Linux/macOS).
set -e
cd "$(dirname "$0")"
SRC="../src/world.cpp ../src/ragdoll.cpp ../src/game.cpp ../src/save.cpp ../src/ui.cpp"
echo "== ASan + UBSan =="; g++ -std=c++14 -O1 -g -fsanitize=address,undefined -pthread headless.cpp stub_audio.cpp $SRC -o /tmp/rpggl_asan && /tmp/rpggl_asan | tail -3
echo "== ThreadSanitizer =="; g++ -std=c++14 -O1 -g -fsanitize=thread -pthread headless.cpp stub_audio.cpp $SRC -o /tmp/rpggl_tsan && /tmp/rpggl_tsan | tail -2
echo "== Fumaca do render/HUD (stubs de OpenGL) =="; g++ -std=c++14 -O1 -g -fsanitize=address,undefined -pthread -Istubs render_smoke.cpp stub_audio.cpp $SRC ../src/render.cpp ../src/hud.cpp -o /tmp/rpggl_smoke && /tmp/rpggl_smoke | tail -2
