[CmdletBinding()]
param(
  [string]$DepsRoot = (Join-Path $PSScriptRoot '.deps'),
  [string]$Generator = 'Visual Studio 18 2026',
  [switch]$UseExistingDeps
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build_x64'
$DepsRoot = [IO.Path]::GetFullPath($DepsRoot)
$spec = Get-Content -LiteralPath (Join-Path $root 'dependencies.json') -Raw | ConvertFrom-Json
if ($UseExistingDeps) {
  # Compatibility with an SDK already installed by the OBS plugin template.
  $sdk = $DepsRoot
} else {
  & (Join-Path $root 'Prepare_Dependencies.ps1') -DepsRoot $DepsRoot -Generator $Generator
  $sdk = Join-Path $DepsRoot 'sdk'
}
$prefixParts = @(
  (Join-Path $sdk 'cmake'),
  (Join-Path $DepsRoot $spec.archives[1].directory),
  (Join-Path $DepsRoot $spec.archives[2].directory)
)
foreach ($path in $prefixParts) { if (-not (Test-Path $path)) { throw "Abhaengigkeit fehlt: $path" } }
$prefix = ($prefixParts -join ';').Replace('\', '/')
Write-Host 'MA OBS MediaLink v0.45 - Windows x64 / OBS 33 build' -ForegroundColor Cyan
& cmake --fresh -S $root -B $build -G $Generator -A x64 "-DCMAKE_PREFIX_PATH=$prefix"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure fehlgeschlagen.' }
& cmake --build $build --config Release --parallel 1
if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen.' }
$release = Join-Path $root 'Release'
& cmake --install $build --config Release --prefix $release
if ($LASTEXITCODE -ne 0) { throw 'Paketbereitstellung fehlgeschlagen.' }
foreach ($relative in @('obs-sacn-crossfade/bin/64bit/obs-sacn-crossfade.dll', 'obs-sacn-crossfade/data/crossfade.effect')) {
  if (-not (Test-Path -LiteralPath (Join-Path $release $relative))) { throw "Release-Datei fehlt: $relative" }
}
Write-Host "Release-Ordner erstellt: $release" -ForegroundColor Green
Write-Host 'Nicht installiert. Die vorhandene OBS-Installation wurde nicht veraendert.'
