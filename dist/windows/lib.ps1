# Gemeinsame Helfer der Windows-dist-Skripte (bauen und packen). Wird mit ". $PSScriptRoot\windows\lib.ps1" geladen.
$ErrorActionPreference = 'Stop'

$FkRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ((Split-Path -Leaf $PSScriptRoot) -eq 'windows') { $FkRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }
Set-Location $FkRoot

# Einstellungen aus dist\dist.env (Schlüssel=Wert, nicht im Git); Umgebungsvariablen haben Vorrang.
# Bekannt sind FK_RELEASE_CHANNEL und FK_UPDATE_URL.
$envFile = Join-Path $FkRoot 'dist\dist.env'
if (Test-Path $envFile) {
    foreach ($line in Get-Content $envFile) {
        if ($line -match '^\s*#' -or $line -notmatch '=') { continue }
        $key, $value = $line -split '=', 2
        if (-not [Environment]::GetEnvironmentVariable($key.Trim())) {
            [Environment]::SetEnvironmentVariable($key.Trim(), $value.Trim())
        }
    }
}

$FkChannel = if ($env:FK_RELEASE_CHANNEL) { $env:FK_RELEASE_CHANNEL } else { 'beta' }

# Version: einzige Quelle ist project(VERSION) in CMakeLists.txt.
$FkVersion = (Select-String -Path (Join-Path $FkRoot 'CMakeLists.txt') -Pattern '^\s+VERSION ([0-9.]+)' |
              Select-Object -First 1).Matches[0].Groups[1].Value

# Git-Stand: git, sonst .fk-git-sha (Kopie ohne git).
$FkGitSha = 'unbekannt'
try { $FkGitSha = (git rev-parse --short HEAD 2>$null) } catch {}
if (-not $FkGitSha -and (Test-Path (Join-Path $FkRoot '.fk-git-sha'))) { $FkGitSha = (Get-Content (Join-Path $FkRoot '.fk-git-sha')).Trim() }
if (-not $FkGitSha) { $FkGitSha = 'unbekannt' }

$FkPlatform     = 'windows-x64'
$FkOut          = Join-Path $FkRoot 'dist\out'
$FkArtifactBase = "FK-Audio-Suite-$FkVersion-$FkPlatform"
# Woher die App Update-Hinweise holt (fest im Build), wie fk_update_url in dist/lib.sh.
$FkUpdateUrl = if ($env:FK_UPDATE_URL) { $env:FK_UPDATE_URL } else { 'https://licensing.fk-onlineservices.de/v1/releases/?product=fk-audio-suite' }

# UTF-8 ohne BOM schreiben: Windows PowerShell 5 setzt bei -Encoding utf8 ein BOM, das JSON-Leser
# (auch der Update-Hinweis der App) nicht mögen.
function Write-Utf8NoBom([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding($false)))
}

# Sorgt dafür, dass cmake aufrufbar ist. Visual Studio bringt CMake mit, legt es aber nur in der
# "Developer PowerShell" in den PATH; hier wird es über vswhere gefunden. Liefert den VS-Ordner.
function Use-FkCmake {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = $null
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue) -and $vs) {
        $vsCmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
        if (Test-Path (Join-Path $vsCmake 'cmake.exe')) { $env:PATH = "$vsCmake;$env:PATH" }
    }
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw 'CMake nicht gefunden. Visual Studio 2022 mit "Desktopentwicklung mit C++" installieren (dist/README.md §3.2).'
    }
    return $vs
}
