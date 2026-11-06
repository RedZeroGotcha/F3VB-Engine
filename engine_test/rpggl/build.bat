@echo off
REM =====================================================================
REM  RPGGL - build (Visual Studio 2022/2026 + CMake)
REM
REM  Uso:   build.bat            -> Release
REM         build.bat debug      -> Debug
REM         build.bat clean      -> apaga build-ninja, build-vs e dist
REM
REM  Detecta sozinho o ambiente:
REM   * "Developer Command Prompt" do VS (cl.exe no PATH): usa o gerador
REM     Ninja.  ATENCAO: Ninja NAO aceita "-A x64" (era o erro da v0.1);
REM     a arquitetura vem do proprio prompt (x64).
REM   * Prompt comum (sem cl.exe): usa o gerador do Visual Studio com -A x64.
REM
REM  Resultado: dist\RPGGL.exe  (runtime do MSVC embutido, /MT)
REM =====================================================================
setlocal EnableExtensions
cd /d "%~dp0"

set "CFG=Release"
if /i "%~1"=="debug" set "CFG=Debug"
if /i "%~1"=="clean" goto :clean

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERRO] cmake nao encontrado. Instale o CMake ou use o Developer Command Prompt do Visual Studio.
  exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 goto :vsgen

REM ---------- Developer Command Prompt: Ninja ----------
if defined VSCMD_ARG_TGT_ARCH if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
  echo [ERRO] Este prompt compila para %VSCMD_ARG_TGT_ARCH%. Abra o "x64 Native Tools Command Prompt" ou "Developer Command Prompt".
  exit /b 1
)
set "BDIR=build-ninja"
set "GEN=Ninja"
where ninja >nul 2>nul
if errorlevel 1 set "GEN=NMake Makefiles"
echo [INFO] Gerador: %GEN%  ^|  Configuracao: %CFG%  ^|  Pasta: %BDIR%
cmake -S . -B %BDIR% -G "%GEN%" -DCMAKE_BUILD_TYPE=%CFG%
if errorlevel 1 goto :erro
cmake --build %BDIR%
if errorlevel 1 goto :erro
set "EXE=%BDIR%\RPGGL.exe"
goto :pronto

REM ---------- Prompt comum: gerador do Visual Studio ----------
:vsgen
set "BDIR=build-vs"
echo [INFO] cl.exe nao esta no PATH: usando o gerador do Visual Studio (-A x64)  ^|  Configuracao: %CFG%
cmake -S . -B %BDIR% -A x64
if errorlevel 1 goto :erro
cmake --build %BDIR% --config %CFG%
if errorlevel 1 goto :erro
set "EXE=%BDIR%\%CFG%\RPGGL.exe"
goto :pronto

:pronto
if not exist dist mkdir dist
copy /y "%EXE%" "dist\RPGGL.exe" >nul
echo.
if not exist "dist\saves" mkdir "dist\saves"
echo ==== OK: dist\RPGGL.exe ====
echo Os saves (.sav) ficam em dist\saves  (pasta criada ao lado do .exe)
where dumpbin >nul 2>nul
if errorlevel 1 goto :fim
echo Dependencias do .exe (NAO deve aparecer VCRUNTIME140 / MSVCP140):
dumpbin /dependents dist\RPGGL.exe | findstr /i ".dll"
:fim
endlocal
exit /b 0

:clean
if exist build rmdir /s /q build
if exist build-ninja rmdir /s /q build-ninja
if exist build-vs rmdir /s /q build-vs
if exist dist rmdir /s /q dist
echo Limpo.
endlocal
exit /b 0

:erro
echo.
echo *** Falha no build ***
echo Dica: se ja existir uma pasta "build" de tentativas antigas, pode apagar (build.bat clean).
endlocal
exit /b 1
