#!/usr/bin/env pwsh
#Requires -Version 5.1
# Thin wrapper - dev-deploy orchestration lives in
# cameraunlock-core/powershell/DevDeploy.psm1.

param(
    [Parameter(Position = 0)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [Parameter(Position = 1)]
    [string]$GivenPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\DevDeploy.psm1') -Force

$buildOutput  = Join-Path $projectRoot "bin\$Configuration"
$vendorLoader = Join-Path $projectRoot 'vendor\ultimate-asi-loader\dinput8.dll'

$result = Invoke-DevDeployASILoader `
    -GameId 'half-life-2' `
    -GameDisplayName 'Half-Life 2' `
    -BuildOutputPath $buildOutput `
    -ModDllName 'HalfLife2HeadTracking.asi' `
    -VendorLoaderDll $vendorLoader `
    -AsiLoaderName 'winmm.dll' `
    -ExeSubDir 'bin' `
    -GivenPath $GivenPath

Write-Host ""
Write-Host "Deployed HalfLife2HeadTracking.asi to: $($result.ExeDir)" -ForegroundColor Green
Write-Host "Controls: End=toggle tracking, PgUp=cycle 6DOF/rotation/position, PgDn=toggle yaw mode." -ForegroundColor Gray
