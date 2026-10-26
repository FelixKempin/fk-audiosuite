# Windows-Gegenstück zu fetch-plugins.sh: holt die mitgelieferten Lite-Plugins als fertige
# Release-Pakete (dist\bundled-plugins.lock), prüft die SHA-256 und legt die VST3-Bundles nach
# dist\out\plugins\. package-windows.ps1 packt sie in Installer und Zip.
# Eigene Bundles statt der Pakete: nach dist\out\plugins\ legen und release-windows.ps1 -KeepPlugins.
#
# Aufruf (PowerShell im Projektordner):  .\dist\fetch-plugins.ps1
. (Join-Path $PSScriptRoot 'windows\lib.ps1')

$lock = Join-Path $PSScriptRoot 'bundled-plugins.lock'
$pluginsOut = Join-Path $FkOut 'plugins'
if (Test-Path $pluginsOut) { Remove-Item -Recurse -Force $pluginsOut }
New-Item -ItemType Directory -Force -Path $pluginsOut | Out-Null

$lines = Get-Content $lock | Where-Object { $_ -notmatch '^\s*#' -and $_.Trim() }
$base = ($lines | Where-Object { $_ -match '^base\s' } | ForEach-Object { ($_ -split '\s+')[1] }) | Select-Object -First 1
if (-not $base) { throw "Keine base-Zeile in $lock." }

$work = Join-Path ([IO.Path]::GetTempPath()) ("fk-plugins-" + [Guid]::NewGuid())
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $found = $false
    foreach ($line in $lines) {
        $platform, $path, $sha = $line -split '\s+'
        if ($platform -ne 'windows') { continue }
        $found = $true
        $file = Split-Path -Leaf $path
        Write-Host "==> Plugin $file"
        $download = Join-Path $work $file
        Invoke-WebRequest -UseBasicParsing -Uri "$base/$path" -OutFile $download
        if ((Get-FileHash -Algorithm SHA256 $download).Hash.ToLower() -ne $sha.ToLower()) {
            throw "Prüfsumme von $file falsch."
        }
        $unpacked = Join-Path $work "$file.d"
        Expand-Archive -Path $download -DestinationPath $unpacked
        # Nur die Bundle-Ordner; die Datei gleichen Namens in Contents\x86_64-win ist kein Ordner.
        $bundles = Get-ChildItem -Recurse -Directory -Filter '*.vst3' $unpacked
        if (-not $bundles) { throw "Kein VST3-Bundle in $file." }
        foreach ($bundle in $bundles) {
            Copy-Item -Recurse $bundle.FullName $pluginsOut
            Write-Host "    $($bundle.Name)"
        }
    }
    if (-not $found) { Write-Warning "Keine Windows-Plugins in $lock, Auslieferung ohne Lite-Plugins." }
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
