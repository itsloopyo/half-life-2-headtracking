#!/usr/bin/env pwsh
#Requires -Version 5.1
# ============================================================================
# Bump the vendored Ultimate ASI Loader under vendor/ultimate-asi-loader/.
# ============================================================================
# Usage:    pixi run update-deps
# Frequency: manual. The vendored copy is the install-time source of truth, so
# the dev runs this when they want a fresh upstream bump, reviews the diff and
# commits it. build / package / release never refresh, and neither does CI.
#
# Half-Life 2 is a 32-bit Source Engine game, so the x86 asset
# (Ultimate-ASI-Loader.zip; the x64 build ships as Ultimate-ASI-Loader_x64.zip)
# is the right one. Upstream ships the loader inside a wrapper zip, but
# install.cmd, package-release.ps1 and launcher-manifest.json all consume the
# raw dinput8.dll (copied to <game>\bin\winmm.dll: Source loads tier0.dll from
# bin\ with an altered search path, so a proxy at the game root is never
# consulted, and winmm.dll is imported there - Half-Life 2 has no xinput import at
# all). So the zip is staged in TEMP and only the DLL is vendored.
# ============================================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

$modulePath = Join-Path $projectDir 'cameraunlock-core/powershell/ModLoaderSetup.psm1'
if (-not (Test-Path $modulePath)) {
    throw "ModLoaderSetup.psm1 not found at $modulePath. Run 'git submodule update --init --recursive' to fetch cameraunlock-core."
}
Import-Module $modulePath -Force

$vendorDir = Join-Path $projectDir 'vendor/ultimate-asi-loader'
$vendorDll = Join-Path $vendorDir 'dinput8.dll'
$readmePath = Join-Path $vendorDir 'README.md'
$licensePath = Join-Path $vendorDir 'LICENSE'
if (-not (Test-Path $vendorDir)) {
    New-Item -ItemType Directory -Path $vendorDir -Force | Out-Null
}

$stageDir = Join-Path $env:TEMP ("asi-update-" + [IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
try {
    $meta = Update-VendoredLoader `
        -Name 'ultimate-asi-loader' `
        -OutputDir $stageDir `
        -OutputFileName 'Ultimate-ASI-Loader.zip' `
        -Owner 'ThirteenAG' -Repo 'Ultimate-ASI-Loader' `
        -VersionPrefix 'v9.' `
        -AssetPattern '^Ultimate-ASI-Loader\.zip$' `
        -LicenseUrl 'https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/master/license'

    $stagedDll = Join-Path $stageDir 'dinput8.dll'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($meta.LocalPath)
    try {
        $entry = $archive.Entries | Where-Object { $_.Name -ieq 'dinput8.dll' } | Select-Object -First 1
        if (-not $entry) {
            throw "$($meta.AssetName) has no dinput8.dll (entries: $($archive.Entries.Name -join ', '))"
        }
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $stagedDll, $true)
    } finally {
        $archive.Dispose()
    }

    $dllSha = (Get-FileHash -LiteralPath $stagedDll -Algorithm SHA256).Hash.ToLower()

    # Idempotency: an unchanged upstream must leave the tree clean. Rewriting
    # README.md unconditionally would churn its fetched_at on every run and
    # produce a commit that says nothing.
    $unchanged = (Test-Path $vendorDll) -and (Test-Path $readmePath) -and (Test-Path $licensePath) -and
        ((Get-FileHash -LiteralPath $vendorDll -Algorithm SHA256).Hash.ToLower() -eq $dllSha)

    if ($unchanged) {
        Write-Host "    no change (dinput8.dll sha256=$($dllSha.Substring(0,12))... matches on-disk vendor copy)" -ForegroundColor DarkGray
    } else {
        Copy-Item -LiteralPath $stagedDll -Destination $vendorDll -Force
        Copy-Item -LiteralPath (Join-Path $stageDir 'LICENSE') -Destination $licensePath -Force

        $readme = @(
            '# Ultimate ASI Loader (vendored)',
            '',
            'Bundled copy of Ultimate ASI Loader (x86), the install-time source of truth.',
            'install.cmd copies it straight out of here and never reaches out to the network.',
            'Refresh manually with `pixi run update-deps`, then commit.',
            '',
            '## Snapshot',
            '',
            '- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader',
            "- Tag: ``$($meta.Tag)``",
            "- Commit: ``$($meta.CommitSha)``",
            "- Asset: ``$($meta.AssetName)``",
            "- Asset URL: $($meta.AssetUrl)",
            "- dinput8.dll SHA-256: ``$dllSha``",
            "- Fetched at: $($meta.FetchedAt)",
            '',
            '`dinput8.dll` is extracted from the upstream x86 zip untouched. install.cmd copies it',
            'to <game>\bin\winmm.dll, the proxy slot Half-Life 2 loads ASI plugins through.'
        ) -join "`n"
        # BOM-less UTF8 with LF endings, matching package-release.ps1: PS 5.1's
        # `Set-Content -Encoding utf8` writes a BOM and terminates the file with
        # CRLF, which makes every regenerated README a mixed-ending diff.
        [IO.File]::WriteAllText($readmePath, $readme + "`n",
                                (New-Object System.Text.UTF8Encoding $false))

        Write-Host "  tag=$($meta.Tag) dinput8.dll sha256=$($dllSha.Substring(0,12))..." -ForegroundColor DarkGray
    }
} finally {
    Remove-Item -LiteralPath $stageDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "vendor/ultimate-asi-loader checked against upstream. Review and commit any diff under vendor/." -ForegroundColor Green
