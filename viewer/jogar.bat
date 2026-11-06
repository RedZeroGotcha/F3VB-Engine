@echo off
rem F3VB v19 - abre o jogo (build\F3VB.exe) com as pastas certas. Rode depois do build.bat.
rem Ajuste as duas linhas abaixo se as suas pastas forem outras (ou defina as variaveis antes de chamar este .bat).
setlocal
cd /d "%~dp0"
if "%F3VB_ASSETS_DIR%"=="" set "F3VB_ASSETS_DIR=F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
if "%F3VB_DEMO_DIR%"=="" set "F3VB_DEMO_DIR=F:\F3VB Engine\Demo\Demo"
set "EXE=build\F3VB.exe"
if not exist "%EXE%" set "EXE=build\Release\F3VB.exe"
if not exist "%EXE%" (
  echo Nao achei build\F3VB.exe. Rode build.bat antes ^(no Developer Command Prompt do Visual Studio^).
  pause
  exit /b 1
)
if not exist "%F3VB_ASSETS_DIR%\CONVERTED\maps" echo AVISO: %F3VB_ASSETS_DIR%\CONVERTED\maps nao existe. Rode: py -3 -m f3vb convert-retail --maps
echo original_assets: %F3VB_ASSETS_DIR%
echo demo (Fonts, English.stf): %F3VB_DEMO_DIR%
start "" "%EXE%" %*
