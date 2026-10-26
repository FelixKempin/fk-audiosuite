# Signieren mit Microsoft Trusted Signing - freiwillig. Für alle Windows-Produkte (DAW, Plugins,
# künftige Programme): Eingebunden mit ". <DAW-Repo>\dist\windows\sign.ps1", dann Invoke-FkSign.
#
# Eingerichtet ist es, wenn diese Umgebungsvariablen gesetzt sind (GitHub: Variablen und Secrets
# des Repos, siehe dist/README.md "Signieren"):
#   FK_SIGN_ENDPOINT   z. B. https://weu.codesigning.azure.net
#   FK_SIGN_ACCOUNT    Name des Trusted-Signing-Kontos
#   FK_SIGN_PROFILE    Name des Zertifikatprofils
#   AZURE_TENANT_ID, AZURE_CLIENT_ID, AZURE_CLIENT_SECRET   App-Registrierung mit der Rolle
#                      "Trusted Signing Certificate Profile Signer"
# Fehlt etwas, bleibt alles unsigniert. Schlägt das Signieren fehl (Konto gekündigt, Dienst nicht
# erreichbar), gibt es eine Warnung und das Build läuft unsigniert weiter: ein Release scheitert nie
# am Signieren.

function Test-FkSigning {
    return [bool]($env:FK_SIGN_ENDPOINT -and $env:FK_SIGN_ACCOUNT -and $env:FK_SIGN_PROFILE -and
                   $env:AZURE_TENANT_ID -and $env:AZURE_CLIENT_ID -and $env:AZURE_CLIENT_SECRET)
}

function Write-FkSignWarning([string]$Text) {
    Write-Warning $Text
    if ($env:GITHUB_ACTIONS) { Write-Host "::warning::$Text" }
}

# Signiert Dateien und alle Programme (.exe, .dll, .vst3-Binaries) in Ordnern. Gibt zurück, ob alles
# signiert ist; $false heißt unsigniert, nie Abbruch.
function Invoke-FkSign([string[]]$Paths) {
    $files = @(foreach ($path in $Paths) {
        if (-not $path -or -not (Test-Path $path)) { continue }
        if (Test-Path $path -PathType Container) {
            Get-ChildItem $path -Recurse -File -Include '*.exe', '*.dll', '*.vst3' | ForEach-Object FullName
        } else {
            (Resolve-Path $path).Path
        }
    })
    if (-not $files) { return $false }
    if (-not (Test-FkSigning)) {
        Write-Host "    Signieren: nicht eingerichtet, $($files.Count) Datei(en) unsigniert"
        return $false
    }

    try {
        if (-not (Get-Module -ListAvailable -Name TrustedSigning)) {
            Set-PSRepository -Name PSGallery -InstallationPolicy Trusted
            Install-Module -Name TrustedSigning -Scope CurrentUser -Force -ErrorAction Stop
        }
        Import-Module TrustedSigning -ErrorAction Stop
        $params = @{
            Endpoint               = $env:FK_SIGN_ENDPOINT
            CodeSigningAccountName = $env:FK_SIGN_ACCOUNT
            CertificateProfileName = $env:FK_SIGN_PROFILE
            Files                  = ($files -join ',')
            FileDigest             = 'SHA256'
            TimestampRfc3161       = 'http://timestamp.acs.microsoft.com'
            TimestampDigest        = 'SHA256'
        }
        Invoke-TrustedSigning @params -ErrorAction Stop | Out-Host
    } catch {
        Write-FkSignWarning "Signieren fehlgeschlagen, weiter unsigniert: $($_.Exception.Message)"
        return $false
    }

    $unsigned = @($files | Where-Object { (Get-AuthenticodeSignature $_).Status -ne 'Valid' })
    if ($unsigned) {
        Write-FkSignWarning "Nicht signiert: $(($unsigned | ForEach-Object { Split-Path -Leaf $_ }) -join ', ')"
        return $false
    }
    Write-Host "    Signiert: $($files.Count) Datei(en)"
    return $true
}
