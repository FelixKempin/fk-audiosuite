# Baut und verpackt FK Audio Suite für Windows: ein Zip zum Entpacken (läuft ohne Installation) und,
# wenn Inno Setup installiert ist, zusätzlich ein Installer. Dazu Prüfsummen und Manifest.
#
# Aufruf (PowerShell im Projektordner):  .\dist\package-windows.ps1 [-SkipTests]
# Ergebnis:
#   dist\out\FK-Audio-Suite-<Version>-windows-x64.zip
#   dist\out\FK-Audio-Suite-<Version>-windows-x64-setup.exe   (mit Inno Setup)
#   dist\out\release-windows-x64.json
param(
    [switch]$SkipTests
)
. (Join-Path $PSScriptRoot 'windows\lib.ps1')
. (Join-Path $PSScriptRoot 'windows\sign.ps1')

& (Join-Path $PSScriptRoot 'build-windows.ps1') -SkipTests:$SkipTests

$releaseDir = Join-Path $FkRoot 'build\windows-release\src\app\FKAudioSuite_artefacts\Release'
$stage      = Join-Path $FkOut 'stage\FK Audio Suite'
New-Item -ItemType Directory -Force -Path $FkOut | Out-Null
if (Test-Path (Join-Path $FkOut 'stage')) { Remove-Item -Recurse -Force (Join-Path $FkOut 'stage') }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item (Join-Path $releaseDir 'FK Audio Suite.exe') $stage
Copy-Item (Join-Path $FkRoot 'LICENSE') (Join-Path $stage 'LICENSE.txt')
Copy-Item (Join-Path $FkRoot 'docs\THIRD_PARTY.md') $stage

# Mitgelieferte Lite-Plugins (fetch-plugins.ps1). Zip: nach <Programm>\VST3, dort benutzt die App
# sie an Ort und Stelle. Installer: nur nach Common Files\VST3 (dieselbe Stelle wie der Einzel-
# Installer des Plugins, also nie zwei Kopien und nie zwei Einträge in der Plugin-Liste); die Namen
# stehen in bundled-plugins.txt, daran erkennt services::BundledPlugins sie.
$plugins = Join-Path $FkOut 'plugins'
if (Test-Path $plugins) {
    Copy-Item -Recurse $plugins (Join-Path $stage 'VST3')
    (Get-ChildItem $plugins -Directory -Filter '*.vst3').Name | Set-Content -Encoding utf8 (Join-Path $stage 'bundled-plugins.txt')
    Write-Host "==> Mitgeliefert: $((Get-ChildItem $plugins -Directory).Name -join ', ')"
} else {
    Write-Host 'Hinweis: keine Plugins in dist\out\plugins (fetch-plugins.ps1), Paket ohne Lite-Plugins.'
}

# Programm und Plugins signieren, wenn Trusted Signing eingerichtet ist (windows\sign.ps1).
$null = Invoke-FkSign @($stage)

# Zip: oberster Ordner "FK Audio Suite\".
$zip = Join-Path $FkOut "$FkArtifactBase.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path $stage -DestinationPath $zip
$primary = $zip

# Installer, falls Inno Setup da ist (https://jrsoftware.org/isinfo.php).
$iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
if ($iscc) {
    Write-Host '==> Installer bauen (Inno Setup)'
    $bundled = if (Test-Path (Join-Path $stage 'VST3')) { 'True' } else { 'False' }
    & $iscc "/DAppVersion=$FkVersion" "/DSourceDir=$stage" "/DOutputDir=$FkOut" "/DOutputBase=$FkArtifactBase-setup" "/DBundledPlugins=$bundled" `
        (Join-Path $PSScriptRoot 'windows\fk-audio-suite.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Inno Setup fehlgeschlagen' }
    $primary = Join-Path $FkOut "$FkArtifactBase-setup.exe"
    $null = Invoke-FkSign @($primary)
} else {
    Write-Host 'Hinweis: Inno Setup nicht gefunden, nur das Zip wurde gebaut.'
}

foreach ($file in @($zip, $primary) | Select-Object -Unique) {
    $hash = (Get-FileHash -Algorithm SHA256 $file).Hash.ToLower()
    "$hash  $(Split-Path -Leaf $file)" | Set-Content -Encoding ascii "$file.sha256"
}

$primaryHash = (Get-FileHash -Algorithm SHA256 $primary).Hash.ToLower()
$manifest = [ordered]@{
    version  = $FkVersion
    channel  = $FkChannel
    platform = $FkPlatform
    gitSha   = $FkGitSha
    date     = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd')
    file     = (Split-Path -Leaf $primary)
    sha256   = $primaryHash
    size     = (Get-Item $primary).Length
    portable = (Split-Path -Leaf $zip)
} | ConvertTo-Json
Write-Utf8NoBom (Join-Path $FkOut "release-$FkPlatform.json") $manifest

# Manifest fk-release-manifest/1 je Datei (dist/write-manifest.py); nur für echte Reifestufen.
$py = @('python3', 'python') | Where-Object { Get-Command $_ -ErrorAction SilentlyContinue } | Select-Object -First 1
if ($FkChannel -in @('alpha', 'beta', 'stable') -and $py) {
    foreach ($item in @(@($primary, 'installer'), @($zip, 'portable'))) {
        $file, $package = $item
        if (-not (Test-Path $file)) { continue }
        $signed = if ((Get-AuthenticodeSignature $(if ($package -eq 'portable') { Join-Path $stage 'FK Audio Suite.exe' } else { $file })).Status -eq 'Valid') { 'signed' } else { 'none' }
        & $py (Join-Path $PSScriptRoot 'write-manifest.py') --out $FkOut --file (Split-Path -Leaf $file) `
            --product fk-audio-suite --family fk-audio --version $FkVersion --track $FkChannel `
            --os windows --arch x86_64 --package $package --signature $signed --os-min 10
        if ($LASTEXITCODE -ne 0) { throw 'Manifest v1 fehlgeschlagen' }
    }
} elseif ($FkChannel -in @('alpha', 'beta', 'stable')) {
    Write-Host '::warning::Python fehlt: nur das alte Manifest release-*.json geschrieben.'
}

Remove-Item -Recurse -Force (Join-Path $FkOut 'stage')
Write-Host ''
Write-Host "==> Paket:     $primary"
Write-Host "    Portabel:  $zip"
