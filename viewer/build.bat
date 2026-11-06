@echo off
rem Compila o F3VB Viewer (64 bits). Rode no "Developer Command Prompt for VS" (cmd), nao no PowerShell.
rem Tenta primeiro Ninja (sem -A x64: Ninja nao aceita plataforma); se falhar, tenta o gerador do Visual Studio.
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake nao encontrado. Abra o "Developer Command Prompt for VS" ou instale com:  winget install Kitware.CMake
  pause
  exit /b 1
)
where cl >nul 2>nul
if errorlevel 1 (
  echo O compilador cl.exe nao esta no PATH. Abra pelo "Developer Command Prompt for VS" ^(nao pelo PowerShell comum^).
  pause
  exit /b 1
)
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" (
  if not "%VSCMD_ARG_TGT_ARCH%"=="" echo AVISO: o prompt esta em "%VSCMD_ARG_TGT_ARCH%". Para 64 bits use o "x64 Native Tools Command Prompt" ou rode: vcvars64.bat
)

rem Ignora um CMAKE_GENERATOR herdado do ambiente e comeca de uma pasta limpa
set CMAKE_GENERATOR=
set CMAKE_GENERATOR_PLATFORM=
if exist build rmdir /s /q build

echo.
echo === Tentativa 1: Ninja ===
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :tenta_vs
cmake --build build
if errorlevel 1 goto :falhou
set "EXE=build\F3VBViewer.exe"
goto :pronto

:tenta_vs
echo.
echo === Tentativa 2: Visual Studio 18 2026 ===
if exist build rmdir /s /q build
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
if errorlevel 1 goto :falhou
cmake --build build --config Release
if errorlevel 1 goto :falhou
set "EXE=build\Release\F3VBViewer.exe"
goto :pronto

:falhou
echo.
echo Falha ao compilar. Copie a mensagem inteira acima e envie.
pause
exit /b 1

:pronto
echo %EXE%> build\exe_path.txt
call :video32 "%EXE%"
echo.
echo Pronto: %EXE%
echo   F3VB Engine v21: o JOGO e  build\F3VB.exe  (na 1a vez escolha a pasta da demo; F3VB.exe --autoteste confere tudo sozinho)
echo        conferencia sem janela:  build\F3VBCheck.exe --demo "pasta da demo" --maps --models --tudo
echo   visualizadores: build\F3VBMapViewer.exe (mapas) e build\F3VBViewer.exe (modelos .f3m)
pause
exit /b 0

rem v20: ajudante de video de 32 BITS (F3VBVideo32.exe). Os videos de abertura (.mve) da demo sao Bink 1 e quem decodifica e a
rem binkw32.dll da PROPRIA demo, que e de 32 bits; por isso o ajudante e compilado a parte, com o compilador x86, e fica ao lado do F3VB.exe.
rem Sem ele o jogo funciona normalmente, so pula os videos.
:video32
setlocal
for %%D in ("%~dp1.") do set "OUTDIR=%%~fD"
set "VCVARS=%VSINSTALLDIR%VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
  echo AVISO: vcvarsall.bat nao achado ^(VSINSTALLDIR="%VSINSTALLDIR%"^): F3VBVideo32.exe nao compilado; os videos de abertura serao pulados.
  endlocal & exit /b 0
)
if not exist build\video32 mkdir build\video32
cmd /c ""%VCVARS%" x86 >nul && cl /nologo /O2 /EHsc /MT /W3 /D_CRT_SECURE_NO_WARNINGS src\video_helper32.cpp /Fo:build\video32\ /Fe:"%OUTDIR%\F3VBVideo32.exe" user32.lib"
if errorlevel 1 (
  echo AVISO: falha ao compilar o F3VBVideo32.exe ^(32 bits^): os videos de abertura serao pulados.
) else (
  echo Ajudante de video: %OUTDIR%\F3VBVideo32.exe
)
endlocal & exit /b 0
