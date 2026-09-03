# Signs one file with Azure Artifact Signing.
# Copy metadata.example.json to the ignored metadata.json file, or set
# MERIDIAN_SIGNING_METADATA to a metadata file outside the repository.
#
# Requirements:
#   - Azure CLI logged in (az login) as the account holding the
#     "Artifact Signing Certificate Profile Signer" role
#   - Artifact Signing Client Tools
#   - Windows SDK signtool on PATH, or MERIDIAN_SIGNTOOL_PATH set
param(
    [Parameter(Mandatory = $true)][string]$File,
    [switch]$Append,
    [string]$MetadataPath = $env:MERIDIAN_SIGNING_METADATA,
    [string]$SignToolPath = $env:MERIDIAN_SIGNTOOL_PATH,
    [string]$DlibPath = $env:MERIDIAN_AZURE_DLIB_PATH
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($MetadataPath)) {
    $MetadataPath = Join-Path $PSScriptRoot "metadata.json"
}
if (-not (Test-Path -LiteralPath $MetadataPath -PathType Leaf)) {
    throw "Signing metadata not found. Copy metadata.example.json to metadata.json or set MERIDIAN_SIGNING_METADATA."
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

if ([string]::IsNullOrWhiteSpace($DlibPath)) {
    $DlibPath = Join-Path $env:LOCALAPPDATA "Microsoft\MicrosoftArtifactSigningClientTools\Azure.CodeSigning.Dlib.dll"
}
if (-not (Test-Path -LiteralPath $DlibPath -PathType Leaf)) {
    throw "Azure Artifact Signing library does not exist: $DlibPath"
}

# /tr timestamping is mandatory: Artifact Signing certs are valid for only
# three days; the RFC3161 timestamp keeps signatures valid after expiry.
$signArguments = @("sign")
if ($Append) {
    # Preserve a valid vendor signature while adding Meridian's signature.
    $signArguments += "/as"
}
$signArguments += @(
    "/v",
    "/fd", "SHA256",
    "/tr", "http://timestamp.acs.microsoft.com",
    "/td", "SHA256",
    "/dlib", $DlibPath,
    "/dmdf", $MetadataPath,
    $File
)

& $SignToolPath @signArguments
exit $LASTEXITCODE
