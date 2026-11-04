[CmdletBinding()]
param(
  [string]$DepsRoot = (Join-Path $PSScriptRoot '.deps'),
  [string]$Generator = 'Visual Studio 18 2026'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$spec = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'dependencies.json') -Raw | ConvertFrom-Json
$DepsRoot = [IO.Path]::GetFullPath($DepsRoot)
$downloads = Join-Path $DepsRoot 'downloads'
New-Item -ItemType Directory -Force -Path $downloads | Out-Null

# Verify cached archives too. An interrupted or changed download must never be used.
foreach ($archive in $spec.archives) {
  $zip = Join-Path $downloads $archive.name
  if (-not (Test-Path -LiteralPath $zip)) {
    Write-Host "Download: $($archive.name)"
    $temporary = "$zip.partial"
    & curl.exe --fail --location --retry 3 --output $temporary $archive.url
    if ($LASTEXITCODE -ne 0) { throw "Download fehlgeschlagen: $($archive.url)" }
    if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $archive.sha256) {
      throw "SHA256 ungleich: $temporary"
    }
    Move-Item -LiteralPath $temporary -Destination $zip -Force
  }
  if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $archive.sha256) {
    throw "SHA256 ungleich: $zip. Datei entfernen und erneut starten."
  }
  $destination = Join-Path $DepsRoot $archive.directory
  $marker = Join-Path $destination '.medialink-archive-sha256'
  if (Test-Path -LiteralPath $destination) {
    if (-not (Test-Path -LiteralPath $marker) -or
        (Get-Content -LiteralPath $marker -Raw).Trim() -ne $archive.sha256) {
      throw "Unbekannter Abhaengigkeitsordner: $destination. Einen neuen -DepsRoot verwenden."
    }
  } else {
    if ($archive.containsRootDirectory) { $extractTo = $DepsRoot }
    else { $extractTo = $destination }
    New-Item -ItemType Directory -Force -Path $extractTo | Out-Null
    Push-Location $extractTo
    try {
      & cmake -E tar xf $zip
      if ($LASTEXITCODE -ne 0) { throw "Entpacken fehlgeschlagen: $zip" }
    } finally { Pop-Location }
    if (-not (Test-Path -LiteralPath $destination)) { throw "Archivordner fehlt: $destination" }
    Set-Content -LiteralPath $marker -Value $archive.sha256 -Encoding ascii
  }
}

$prebuilt = Join-Path $DepsRoot $spec.archives[1].directory
$qt = Join-Path $DepsRoot $spec.archives[2].directory
$obsSource = Join-Path $DepsRoot $spec.archives[0].directory
# OBS configures 32-bit capture helpers even for a Development-only x64 SDK.
# Seed its dependency markers from our verified archives, avoiding implicit downloads.
$obsDeps = Join-Path $obsSource '.deps'
foreach ($index in @(1, 2, 3)) {
  Copy-Item -LiteralPath (Join-Path $downloads $spec.archives[$index].name) -Destination $obsDeps -Force
}
foreach ($entry in @(@('prebuilt_x64', 1), @('qt6_x64', 2), @('prebuilt_x86', 3))) {
  Set-Content -LiteralPath (Join-Path $obsDeps ".dependency_$($entry[0]).sha256") `
    -Value $spec.archives[$entry[1]].sha256 -Encoding ascii -NoNewline
}
$sdkBuild = Join-Path $DepsRoot 'build_obs33_x64'
$sdk = Join-Path $DepsRoot 'sdk'
$prefix = "$prebuilt;$qt".Replace('\', '/')
Write-Host "OBS $($spec.obsVersion): libobs und Frontend-API bauen"
& cmake --fresh -S $obsSource -B $sdkBuild -G $Generator -A x64 `
  "-DCMAKE_PREFIX_PATH=$prefix" "-DOBS_VERSION_OVERRIDE=$($spec.obsVersion)" `
  -DENABLE_CORE_MODULES=OFF -DENABLE_FRONTEND=OFF -DENABLE_BROWSER=OFF -DENABLE_SCRIPTING=OFF
if ($LASTEXITCODE -ne 0) { throw 'OBS SDK configure fehlgeschlagen.' }
& cmake --build $sdkBuild --target obs-frontend-api --config Release --parallel 1
if ($LASTEXITCODE -ne 0) { throw 'OBS SDK build fehlgeschlagen.' }
& cmake --install $sdkBuild --component Development --config Release --prefix $sdk
if ($LASTEXITCODE -ne 0) { throw 'OBS SDK install fehlgeschlagen.' }
Write-Host "OBS SDK bereit: $sdk" -ForegroundColor Green
