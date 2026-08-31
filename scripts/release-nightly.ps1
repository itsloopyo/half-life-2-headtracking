[CmdletBinding()]
param([switch]$AllowDirty)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

$versionPath = Join-Path $ProjectRoot 'src\version.h'
$versionRaw = Get-Content $versionPath -Raw
if ($versionRaw -notmatch 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"') {
    throw "Could not read HEADTRACKING_VERSION_STRING from $versionPath"
}
$version = $Matches[1]

Publish-NightlyBuild `
    -ModId 'half-life-2' `
    -ModName 'HalfLife2HeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -AllowDirty:$AllowDirty
