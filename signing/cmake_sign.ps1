# CMake post-build hook: signs the given file via sign.ps1, but only for
# Release builds so debug iteration needs no Azure login or network.
param(
    [Parameter(Mandatory = $true)][string]$Config,
    [Parameter(Mandatory = $true)][string]$File
)

if ($Config -ne 'Release')
{
    exit 0
}

& "$PSScriptRoot\sign.ps1" $File
exit $LASTEXITCODE
