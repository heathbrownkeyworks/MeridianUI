param(
    [Parameter(Mandatory = $true)][string]$Config,
    [Parameter(Mandatory = $true)][string]$Root,
    [string]$ExpectedPublisher = $env:MERIDIAN_SIGNING_PUBLISHER,
    [string]$SignToolPath = $env:MERIDIAN_SIGNTOOL_PATH
)

$ErrorActionPreference = "Stop"

if ($Config -ne "Release") {
    Write-Host "Skipping Azure artifact signing for $Config."
    exit 0
}

if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
    throw "Release artifact root does not exist: $Root"
}

if ([string]::IsNullOrWhiteSpace($ExpectedPublisher)) {
    throw "Set MERIDIAN_SIGNING_PUBLISHER or pass -ExpectedPublisher."
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

$binaries = @(
    Get-ChildItem -LiteralPath $Root -Recurse -File |
        Where-Object { $_.Extension -in ".dll", ".exe" } |
        Sort-Object FullName
)
if ($binaries.Count -eq 0) {
    throw "No DLL or EXE artifacts were found below: $Root"
}

$publisherPattern = "Issued to:\s+" + [regex]::Escape($ExpectedPublisher)
$signScript = Join-Path $PSScriptRoot "sign.ps1"
$verifyScript = Join-Path $PSScriptRoot "verify_release_artifacts.ps1"

$signedCount = 0
$skippedCount = 0
foreach ($binary in $binaries) {
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $verifyOutput = & $SignToolPath verify /pa /all /v $binary.FullName 2>&1 | Out-String
    $verifyExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference

    if ($verifyExitCode -eq 0 -and $verifyOutput -match $publisherPattern) {
        ++$skippedCount
        Write-Host "Already Azure-signed: $($binary.FullName)"
        continue
    }

    $appendSignature = $verifyExitCode -eq 0
    $signArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $signScript,
        "-File", $binary.FullName,
        "-SignToolPath", $SignToolPath
    )
    if ($appendSignature) {
        $signArguments += "-Append"
        Write-Host "Appending Meridian signature while preserving vendor signature: $($binary.FullName)"
    } else {
        Write-Host "Signing staged binary: $($binary.FullName)"
    }

    & powershell @signArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Azure Artifact Signing failed for: $($binary.FullName)"
    }
    ++$signedCount
}

& $verifyScript -Config $Config -Root $Root -ExpectedPublisher $ExpectedPublisher -SignToolPath $SignToolPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Release signing complete: $signedCount signed, $skippedCount already signed, $($binaries.Count) total."
