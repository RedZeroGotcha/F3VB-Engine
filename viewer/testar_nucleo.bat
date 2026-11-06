@echo off
rem Testa o nucleo do visualizador (leitor, animacao, skinning) comparando com o Python.
rem Precisa de Python e de ter compilado antes com build.bat
setlocal
cd /d "%~dp0"
if exist build\test_core.exe ( set "T=build\test_core.exe" ) else ( set "T=build\Release\test_core.exe" )
if not exist %T% ( echo test_core.exe nao encontrado. Rode build.bat primeiro. & goto :erro )
py -3 tests\make_fixture.py build\fixture || goto :erro
%T% build\fixture || goto :erro
if exist build\test_v12.exe ( set "T2=build\test_v12.exe" ) else ( set "T2=build\Release\test_v12.exe" )
if not exist %T2% ( echo test_v12.exe nao encontrado. Rode build.bat primeiro. & goto :erro )
%T2% build\fixture || goto :erro
if exist build\test_f3map.exe ( set "T3=build\test_f3map.exe" ) else ( set "T3=build\Release\test_f3map.exe" )
if not exist %T3% ( echo test_f3map.exe nao encontrado. Rode build.bat primeiro. & goto :erro )
py -3 tests\make_map_fixture.py build\mapfx || goto :erro
%T3% build\mapfx || goto :erro
if exist build\test_script.exe ( set "T4=build\test_script.exe" ) else ( set "T4=build\Release\test_script.exe" )
if not exist %T4% ( echo test_script.exe nao encontrado. Rode build.bat primeiro. & goto :erro )
%T4% || goto :erro
if exist build\test_ui.exe ( set "T5=build\test_ui.exe" ) else ( set "T5=build\Release\test_ui.exe" )
if not exist %T5% ( echo test_ui.exe nao encontrado. Rode build.bat primeiro. & goto :erro )
%T5% || goto :erro
echo OK
pause
exit /b 0
:erro
echo FALHOU
pause
exit /b 1
