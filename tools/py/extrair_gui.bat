@echo off
rem Abre a interface do extrator F3VB (precisa do Python do Windows)
cd /d "%~dp0"
where py >nul 2>nul && (py -3 f3vb_extractor_gui.pyw) || (python f3vb_extractor_gui.pyw)
if errorlevel 1 pause
