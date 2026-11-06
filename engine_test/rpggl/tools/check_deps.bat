@echo off
REM Lista as DLLs de que o .exe depende. Esperado (todas ja vem no Windows):
REM   KERNEL32, USER32, GDI32, OPENGL32, WINMM  (e opcionalmente api-ms-win-crt-* se nao for /MT)
REM Se aparecer VCRUNTIME140.dll ou MSVCP140.dll, o runtime NAO esta estatico.
REM Rode no "x64 Native Tools Command Prompt for VS".
dumpbin /dependents "%~1"
