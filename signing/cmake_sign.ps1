# CMake post-build hook: signs the given file, but only for
# Release builds so debug iteration needs no signing setup or network.
# When LOCAL_SIGNING is set, signs with the local self-signed certificate
# (sign_local.ps1); otherwise signs with Azure Artifact Signing (sign.ps1).
param(
    [Parameter(Mandatory = $true)][string]$Config,
    [Parameter(Mandatory = $true)][string]$File,
    [switch]$LocalSigning
)

if ($Config -ne 'Release')
{
    exit 0
}

if ($LocalSigning)
{
    & "$PSScriptRoot\sign_local.ps1" -File $File
}
else
{
    & "$PSScriptRoot\sign.ps1" -File $File
}
exit $LASTEXITCODE
