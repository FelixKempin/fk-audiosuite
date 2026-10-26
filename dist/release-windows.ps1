# Die ganze Windows-Auslieferung in einem Lauf, Gegenstück zu release-linux.sh: Voraussetzungen
# prüfen (fehlende Werkzeuge bietet es per winget an), Lite-Plugins holen, DAW bauen und testen,
# Installer und Zip packen. Ergebnis in dist\out\.
#
# Gestartet über dist\windows\Release-Windows.cmd (Doppelklick) oder:
#   powershell -ExecutionPolicy Bypass -File .\dist\release-windows.ps1 [-SkipTests] [-KeepPlugins]
param(
    [switch]$SkipTests,
    # Die VST3-Bundles in dist\out\plugins\ nehmen, wie sie sind, statt sie laut bundled-plugins.lock zu laden.
    [switch]$KeepPlugins
)
. (Join-Path $PSScriptRoot 'windows\lib.ps1')

$started = Get-Date
Write-Host "==> FK Audio Suite $FkVersion ($FkChannel), Stand $FkGitSha" -ForegroundColor Cyan

function Update-FkPath {
    $env:PATH = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
                [Environment]::GetEnvironmentVariable('Path', 'User')
}

# Fragt einmal nach und installiert über winget (in Windows 10/11 enthalten).
function Install-FkTool([string]$Name, [string]$WingetId, [string[]]$Extra = @()) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "$Name fehlt, und winget ist nicht da. Bitte von Hand installieren (dist/README.md)."
    }
    $answer = Read-Host "$Name fehlt. Jetzt über winget installieren? [J/n]"
    if ($answer -and $answer -notmatch '^[jJyY]') { throw "$Name fehlt (dist/README.md)." }
    winget install --id $WingetId -e --accept-package-agreements --accept-source-agreements @Extra
    if ($LASTEXITCODE -ne 0) { throw "Installation von $Name fehlgeschlagen." }
    Update-FkPath
}

Write-Host '==> Voraussetzungen'
try { $null = Use-FkCmake } catch {
    # Build Tools reichen (ohne IDE); die Arbeitslast "C++-Buildtools" bringt MSVC, SDK und CMake mit.
    Install-FkTool 'Visual Studio 2022 (C++-Buildtools)' 'Microsoft.VisualStudio.2022.BuildTools' @(
        '--override', '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended')
    $null = Use-FkCmake
}
Write-Host "    CMake:       $((Get-Command cmake).Source)"

$iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { Install-FkTool 'Inno Setup 6 (Installer)' 'JRSoftware.InnoSetup' }

Write-Host "    Plugins:     $(if ($KeepPlugins) { 'vorhandene in dist\out\plugins' } else { 'Release-Pakete (bundled-plugins.lock)' })"

if (-not $KeepPlugins) { & (Join-Path $PSScriptRoot 'fetch-plugins.ps1') }
& (Join-Path $PSScriptRoot 'package-windows.ps1') -SkipTests:$SkipTests

$minutes = [int]((Get-Date) - $started).TotalMinutes
Write-Host "==> Fertig nach $minutes min. Pakete in $FkOut" -ForegroundColor Green
