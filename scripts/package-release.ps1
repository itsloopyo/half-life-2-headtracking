param([string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path "$PSScriptRoot\.."
$binDir   = Join-Path $repoRoot "bin\$Configuration"
$outDir   = Join-Path $repoRoot 'release'

if (-not (Test-Path $binDir)) {
    throw "Build output not found at $binDir. Run: pixi run build-release"
}

$versionHeader = Join-Path $repoRoot 'src\version.h'
$match = (Select-String -Path $versionHeader -Pattern 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"').Matches
if (-not $match) { throw "Could not parse HEADTRACKING_VERSION_STRING from $versionHeader" }
$version = $match[0].Groups[1].Value

$modName = 'HalfLife2HeadTracking'
$modSlug = 'half-life-2-headtracking'
$asi     = 'HalfLife2HeadTracking.asi'

$asiPath = Join-Path $binDir $asi
if (-not (Test-Path $asiPath)) { throw "Missing build output: $asiPath" }

$vendorDll = Join-Path $repoRoot 'vendor\ultimate-asi-loader\dinput8.dll'
if (-not (Test-Path $vendorDll)) {
    throw "Missing vendored loader: $vendorDll. Run: pixi run update-deps"
}

if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

# ---------- Installer ZIP (GitHub Release) ----------
$installerStage = Join-Path $outDir "$modSlug-installer-stage"
if (Test-Path $installerStage) { Remove-Item $installerStage -Recurse -Force }
New-Item -ItemType Directory -Path $installerStage | Out-Null

# Mod payload deployed to <game>\bin by install.cmd. HeadTracking.ini is
# created by the mod on first launch, so it is not shipped here.
$pluginsDir = Join-Path $installerStage 'plugins'
New-Item -ItemType Directory -Path $pluginsDir | Out-Null
Copy-Item $asiPath $pluginsDir

# Vendored Ultimate ASI Loader: install-time source of truth, extracted to
# <game>\bin\winmm.dll by install.cmd.
$vendorStage = Join-Path $installerStage 'vendor\ultimate-asi-loader'
New-Item -ItemType Directory -Path $vendorStage | Out-Null
# dinput8.dll and its LICENSE are both mandatory: the loader is MIT, and its
# license has to travel with the binary. README.md is the vendoring provenance
# note and is nice to have.
foreach ($f in @('dinput8.dll', 'LICENSE')) {
    $src = Join-Path $repoRoot "vendor\ultimate-asi-loader\$f"
    if (-not (Test-Path $src)) { throw "Missing vendored loader file: $src" }
    Copy-Item $src $vendorStage
}
$vendorReadme = Join-Path $repoRoot 'vendor\ultimate-asi-loader\README.md'
if (Test-Path $vendorReadme) { Copy-Item $vendorReadme $vendorStage }

# cmd.exe needs CRLF: an LF-only batch file mis-parses labels and parenthesised
# blocks, and both wrappers are built out of those, so the failure is an
# installer that exits having done nothing. .gitattributes marks these
# eol=crlf, but the packager copies from the WORKING TREE, which is what a file
# written by an LF-defaulting tool ships as. Checked rather than repaired,
# because a wrapper that reached this point with LF endings was edited by
# something that will do it again.
foreach ($script in @('install.cmd', 'uninstall.cmd')) {
    $path = Join-Path $repoRoot "scripts\$script"
    if ([IO.File]::ReadAllText($path) -notmatch "`r`n") {
        throw "scripts\$script has LF line endings. cmd.exe needs CRLF - run: unix2dos scripts/$script"
    }
}

Copy-Item (Join-Path $repoRoot 'scripts\install.cmd')   $installerStage
Copy-Item (Join-Path $repoRoot 'scripts\uninstall.cmd') $installerStage

# Launcher manifest: the launcher reads launcher-manifest.json from the
# release root to route the install (delivery_mode). Version is stamped from
# src/version.h so the manifest stays in lockstep with the built binary.
$manifestPath = Join-Path $repoRoot 'launcher-manifest.json'
if (-not (Test-Path $manifestPath)) { throw "Missing manifest: $manifestPath" }
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$manifest.mod_info.version = $version
# WriteAllText with a BOM-less UTF8 encoder, not Set-Content -Encoding utf8:
# PS 5.1's utf8 emits a BOM, and utf8NoBOM does not exist there. Both consumers
# currently strip a BOM defensively, but the manifest is a contract file and
# should not need the workaround.
$manifestJson = $manifest | ConvertTo-Json -Depth 10
[IO.File]::WriteAllText(
    (Join-Path $installerStage 'launcher-manifest.json'),
    $manifestJson,
    (New-Object System.Text.UTF8Encoding $false))

Import-Module (Join-Path $repoRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force
Copy-SharedBundle -StagingDir $installerStage -NoRefresh

# LICENSE and THIRD-PARTY-NOTICES.md are not optional documentation: MinHook's
# BSD-2-Clause and the vendored loader's MIT both require their notice to
# accompany the binaries in this ZIP. A missing one fails the package rather
# than quietly producing a release that cannot be distributed.
foreach ($doc in @('LICENSE', 'THIRD-PARTY-NOTICES.md')) {
    $src = Join-Path $repoRoot $doc
    if (-not (Test-Path $src)) { throw "Missing required notice file: $src" }
    Copy-Item $src $installerStage
}
foreach ($doc in @('README.md', 'CHANGELOG.md')) {
    $src = Join-Path $repoRoot $doc
    if (Test-Path $src) { Copy-Item $src $installerStage }
}

$installerZip = Join-Path $outDir "$modName-v$version-installer.zip"
if (Test-Path $installerZip) { Remove-Item $installerZip -Force }
Compress-Archive -Path "$installerStage\*" -DestinationPath $installerZip
Remove-Item $installerStage -Recurse -Force
Write-Host "Packaged installer: $installerZip" -ForegroundColor Green

# ---------- Nexus ZIP (extract to game folder) ----------
# Deploy subtree only: the .asi lands in <game>\bin. Nexus users supply their
# own ASI loader (winmm.dll), so no vendored loader is bundled here.
$nexusStage = Join-Path $outDir "$modSlug-nexus-stage"
if (Test-Path $nexusStage) { Remove-Item $nexusStage -Recurse -Force }
$nexusBin = Join-Path $nexusStage 'bin'
New-Item -ItemType Directory -Path $nexusBin | Out-Null
Copy-Item $asiPath $nexusBin

# The .asi statically links MinHook (BSD-2-Clause) and cameraunlock-core (MIT),
# and both require their notice to travel with a binary distribution. This ZIP
# is a distribution in its own right - it is what Nexus hands the user - so the
# notices ship in it, not only in the installer ZIP. They sit at the ZIP root so
# that extracting over the game folder does not scatter them into the engine's
# own directory.
foreach ($doc in @('LICENSE', 'THIRD-PARTY-NOTICES.md', 'README.md')) {
    $src = Join-Path $repoRoot $doc
    if (-not (Test-Path $src)) { throw "Missing required notice file: $src" }
    Copy-Item $src $nexusStage
}

$nexusZip = Join-Path $outDir "$modName-v$version-nexus.zip"
if (Test-Path $nexusZip) { Remove-Item $nexusZip -Force }
Compress-Archive -Path "$nexusStage\*" -DestinationPath $nexusZip
Remove-Item $nexusStage -Recurse -Force
Write-Host "Packaged nexus:     $nexusZip" -ForegroundColor Green
