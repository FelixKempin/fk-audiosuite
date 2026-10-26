@echo off
rem Doppelklick-Start der Windows-Auslieferung (dist\release-windows.ps1).
rem Liegt die Datei neben einem Ordner FKAudioSuite (Kopie ohne git), startet sie dort, sonst im Repo.
rem Optionen werden durchgereicht, z. B. -SkipTests.
setlocal
set "SCRIPT=%~dp0FKAudioSuite\dist\release-windows.ps1"
if not exist "%SCRIPT%" set "SCRIPT=%~dp0..\release-windows.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
echo.
if errorlevel 1 (echo Die Auslieferung ist fehlgeschlagen, siehe oben.) else (echo Fertig.)
pause
