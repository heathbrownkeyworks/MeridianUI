param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$ScratchRoot
)
$ErrorActionPreference='Stop'
$SourceRoot=(Resolve-Path -LiteralPath $SourceRoot).Path
$null=New-Item -ItemType Directory -Path $ScratchRoot -Force
$ScratchRoot=(Resolve-Path -LiteralPath $ScratchRoot).Path
$runnerPath=Join-Path $ScratchRoot 'RunBuildCase.ps1'
@'
param([string]$SourceRoot,[string]$ScratchRoot,[string]$CaseName,
      [int]$ConfigureExit=0,[int]$BuildExit=0,[int]$TestExit=0,
      [switch]$BuildTests,[switch]$SkipTests,[switch]$Fresh)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $ScratchRoot
$env:VS_DEV_SHELL_PATH=Join-Path $ScratchRoot 'FakeVsDevShell.ps1'
$global:caseLog=Join-Path $ScratchRoot ($CaseName+'.jsonl')
$global:configureResult=$ConfigureExit
$global:buildResult=$BuildExit
$global:testResult=$TestExit
$global:nativeExit=Join-Path $ScratchRoot 'NativeExit.cmd'
$global:cmakeCalls=0
function cl {}
function cmake {
    $flatArgs=@($args | ForEach-Object { $_ })
    $global:cmakeCalls++
    @{tool='cmake';arguments=$flatArgs} | ConvertTo-Json -Compress | Add-Content -LiteralPath $global:caseLog
    $env:MERIDIAN_TEST_NATIVE_EXIT=if($global:cmakeCalls -eq 1){$global:configureResult}else{$global:buildResult}
    & $global:nativeExit
    $global:LASTEXITCODE=$LASTEXITCODE
}
function ctest {
    @{tool='ctest';arguments=@($args)} | ConvertTo-Json -Compress | Add-Content -LiteralPath $global:caseLog
    $env:MERIDIAN_TEST_NATIVE_EXIT=$global:testResult
    & $global:nativeExit
    $global:LASTEXITCODE=$LASTEXITCODE
}
$options=@{threads=1}
if($BuildTests){$options.buildTests=$true}
if($SkipTests){$options.skipTests=$true}
if($Fresh){$options.fresh=$true}
& (Join-Path $SourceRoot 'BuildRelease.ps1') @options
exit $LASTEXITCODE
'@ | Set-Content -LiteralPath $runnerPath
'param([string]$Arch)' | Set-Content -LiteralPath (Join-Path $ScratchRoot 'FakeVsDevShell.ps1')
'@exit /b %MERIDIAN_TEST_NATIVE_EXIT%' | Set-Content -LiteralPath (Join-Path $ScratchRoot 'NativeExit.cmd')

$cases=@(
    @{name='default';options=@();testing='OFF';cmake=2;ctest=0;exit=0},
    @{name='tests';options=@('-BuildTests');testing='ON';cmake=2;ctest=1;exit=0},
    @{name='skip';options=@('-SkipTests');testing='OFF';cmake=2;ctest=0;exit=0},
    @{name='compile-only';options=@('-BuildTests','-SkipTests');testing='ON';cmake=2;ctest=0;exit=0},
    @{name='configure-failure';options=@('-BuildTests','-ConfigureExit','17');testing='ON';cmake=1;ctest=0;exit=17},
    @{name='build-failure';options=@('-BuildTests','-BuildExit','18');testing='ON';cmake=2;ctest=0;exit=18},
    @{name='test-failure';options=@('-BuildTests','-TestExit','8');testing='ON';cmake=2;ctest=1;exit=8},
    @{name='fresh';options=@('-Fresh');testing='OFF';cmake=2;ctest=0;exit=0}
)
foreach($case in $cases){
    $caseLog=Join-Path $ScratchRoot ($case.name+'.jsonl')
    Set-Content -LiteralPath $caseLog -Value ''
    $options=$case.options
    $output=& powershell -NoProfile -ExecutionPolicy Bypass -File $runnerPath -SourceRoot $SourceRoot -ScratchRoot $ScratchRoot -CaseName $case.name @options 2>&1
    $actualExit=$LASTEXITCODE
    $output | Set-Content -LiteralPath (Join-Path $ScratchRoot ($case.name+'.log'))
    if($actualExit -ne $case.exit){throw "$($case.name): expected exit $($case.exit), got $actualExit"}
    $calls=@(Get-Content -LiteralPath $caseLog | Where-Object {$_ -match '\S'} | ForEach-Object {$_ | ConvertFrom-Json})
    if(@($calls | Where-Object tool -eq 'cmake').Count -ne $case.cmake){throw "$($case.name): incorrect build call count"}
    if(@($calls | Where-Object tool -eq 'ctest').Count -ne $case.ctest){throw "$($case.name): incorrect test call count"}
    $configure=$calls[0].arguments
    if($configure -notcontains "-DBUILD_TESTING=$($case.testing)"){throw "$($case.name): wrong test compilation policy"}
    if($configure -notcontains $SourceRoot){throw "$($case.name): source path depends on working directory"}
    if(($case.name -eq 'fresh') -ne ($configure -contains '--fresh')){throw "$($case.name): incorrect fresh configure policy"}
    foreach($testCall in @($calls | Where-Object tool -eq 'ctest')){
        if($testCall.arguments -notcontains '--no-tests=error'){throw "$($case.name): empty test suite would pass"}
        if($testCall.arguments -notcontains (Join-Path $SourceRoot 'build/release')){throw "$($case.name): tests use the wrong build directory"}
    }
    Write-Host "PASS: $($case.name)"
}
Write-Host 'All build-script regression cases passed.'
