param(
    [Parameter(Mandatory = $true)][string]$Config,
    [Parameter(Mandatory = $true)][string]$Root,
    [string]$ExpectedPublisher = $env:MERIDIAN_SIGNING_PUBLISHER,
    [string]$SignToolPath = $env:MERIDIAN_SIGNTOOL_PATH
)

$ErrorActionPreference = "Stop"

if ($Config -ne "Release") {
    Write-Host "Skipping release signature verification for $Config."
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
$failures = @()

foreach ($binary in $binaries) {
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $verifyOutput = & $SignToolPath verify /pa /all /v $binary.FullName 2>&1 | Out-String
    $verifyExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    if ($verifyExitCode -ne 0) {
        $failures += "$($binary.FullName): one or more Authenticode signatures are invalid or missing"
        continue
    }
    if ($verifyOutput -notmatch $publisherPattern) {
        $failures += "$($binary.FullName): Meridian Azure signature from '$ExpectedPublisher' is missing"
    }
}

if ($failures.Count -gt 0) {
    throw "Release signature verification failed:`n$($failures -join "`n")"
}

Write-Host "Verified Meridian Azure signatures on all $($binaries.Count) staged DLL/EXE artifacts."
