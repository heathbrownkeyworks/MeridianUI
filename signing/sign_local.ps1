# Signs one file with a local self-signed code-signing certificate (PFX).
# For LOCAL DEVELOPER TESTING ONLY - not for release distribution.
#
# Defaults to signing/local/MyLocalTestCert.pfx. Override with:
#   MERIDIAN_LOCAL_SIGN_PFX            - path to the PFX to sign with
#   MERIDIAN_LOCAL_SIGN_PFX_PASSWORD   - the PFX export password
#   MERIDIAN_SIGNTOOL_PATH             - path to signtool.exe (else PATH)
#
# The certificate should be installed in a trusted store on this machine so
# the signed binaries verify locally (see docs/plans/local_signing.txt).
# Note: the PFX password is passed to signtool on the command line, which is
# acceptable for local development but not for CI or shared machines.
# 
# ---
# 
# Cert creation information:
# 
# PS D:\tmp> $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=MyLocalTestCert" -CertStoreLocation "Cert:\LocalMachine\My"
# PS D:\tmp> Export-Certificate -Cert $cert -FilePath "MyLocalTestCert.cer"
# PS D:\tmp> $password = ConvertTo-SecureString -String "YourSecurePassword123" -AsPlainText -Force
# PS D:\tmp> Export-PfxCertificate -Cert $cert -FilePath "MyLocalTestCert.pfx" -Password $password
# 
# Used certlm.msc to copy from "Personal > Certificates" into "Trusted Root Certification Authorities > Certificates".

"MyLocalTestCert.cer" and "MyLocalTestCert.pfx" copied into "signing\local\".
param(
    [Parameter(Mandatory = $true)][string]$File,
    [switch]$Append,
    [switch]$NoTimestamp,
    [string]$PfxPath = $env:MERIDIAN_LOCAL_SIGN_PFX,
    [string]$Password = "YourSecurePassword123", # $env:MERIDIAN_LOCAL_SIGN_PFX_PASSWORD,
    [string]$SignToolPath = $env:MERIDIAN_SIGNTOOL_PATH,
    [string]$TimestampUrl = "http://timestamp.acs.microsoft.com"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($PfxPath)) {
    $PfxPath = Join-Path $PSScriptRoot "local\MyLocalTestCert.pfx"
}
if (-not (Test-Path -LiteralPath $PfxPath -PathType Leaf)) {
    throw "Local signing PFX not found: $PfxPath. Place it in signing/local or set MERIDIAN_LOCAL_SIGN_PFX."
}

if ([string]::IsNullOrWhiteSpace($Password)) {
    throw "Local PFX password is not set. Set MERIDIAN_LOCAL_SIGN_PFX_PASSWORD or pass -Password."
}

if ([string]::IsNullOrWhiteSpace($SignToolPath)) {
    $signToolCommand = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($null -eq $signToolCommand) {
        throw "signtool.exe was not found. Add it to PATH or set MERIDIAN_SIGNTOOL_PATH."
    }
    $SignToolPath = $signToolCommand.Source
}
if (-not (Test-Path -LiteralPath $SignToolPath -PathType Leaf)) {
    throw "SignTool does not exist: $SignToolPath"
}

# RFC3161 timestamp keeps the signature valid independent of the local cert's
# validity window (self-signed certs may be short-lived). Use -NoTimestamp for
# fully offline signing.
$signArguments = @("sign")
if ($Append) {
    # Preserve a valid vendor signature while adding the local signature.
    $signArguments += "/as"
}
$signArguments += @(
    "/v",
    "/fd", "SHA256",
    "/f", $PfxPath,
    "/p", $Password
)
if (-not $NoTimestamp) {
    $signArguments += @("/tr", $TimestampUrl, "/td", "SHA256")
}
$signArguments += $File

& $SignToolPath @signArguments
exit $LASTEXITCODE
