cmake_minimum_required(VERSION 3.20)
project(f3vb LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(f3vb_formats INTERFACE)
target_include_directories(f3vb_formats INTERFACE src)

# VM Pawn/Small classica (AMX file_version 6) para os scripts da demo
add_library(f3vb_amx STATIC src/amx/amx_vm.cpp)
target_include_directories(f3vb_amx PUBLIC src)

add_library(f3vb_host STATIC src/host/game_host.cpp)
target_link_libraries(f3vb_host PUBLIC f3vb_amx)

add_executable(dialog_dump src/host/dialog_dump.cpp)
target_link_libraries(dialog_dump PRIVATE f3vb_host)

add_executable(amx_run src/amx/amx_run.cpp)
target_link_libraries(amx_run PRIVATE f3vb_amx)

enable_testing()
add_executable(test_chunk tests_cpp/test_chunk.cpp)
target_link_libraries(test_chunk PRIVATE f3vb_formats)
add_test(NAME chunk COMMAND test_chunk)

add_executable(test_archive tests_cpp/test_archive.cpp)
target_link_libraries(test_archive PRIVATE f3vb_formats)
add_test(NAME archive COMMAND test_archive)

add_executable(test_amx tests_cpp/test_amx.cpp)
target_link_libraries(test_amx PRIVATE f3vb_amx)
add_test(NAME amx COMMAND test_amx)

add_executable(test_host tests_cpp/test_host.cpp)
target_link_libraries(test_host PRIVATE f3vb_host)
add_test(NAME host COMMAND test_host)

# Fuzz opcional (use -fsanitize=address,undefined):  fuzz_amx ARQ.amx [iteracoes]
add_executable(fuzz_amx tests_cpp/fuzz_amx.cpp)
target_link_libraries(fuzz_amx PRIVATE f3vb_amx)
