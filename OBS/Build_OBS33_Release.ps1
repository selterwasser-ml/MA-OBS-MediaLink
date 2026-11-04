param(
  [string]$DepsRoot = 'C:\OBS-Development\obs-plugintemplate-33\.deps',
  [string]$Generator = 'Visual Studio 18 2026'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build_x64'
$prefixParts = @(
  (Join-Path $DepsRoot 'cmake'),
  (Join-Path $DepsRoot 'obs-deps-2026-10-23-x64'),
  (Join-Path $DepsRoot 'obs-deps-qt6-2026-10-23-x64')
)
foreach ($path in $prefixParts) { if (-not (Test-Path $path)) { throw "Abhaengigkeit fehlt: $path" } }
$prefix = $prefixParts -join ';'
Write-Host 'MA OBS MediaLink v0.45 - OBS 33 Quellenauswahl BETA build' -ForegroundColor Cyan
& cmake -S $root -B $build -G $Generator -A x64 "-DCMAKE_PREFIX_PATH=$prefix"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure fehlgeschlagen.' }
& cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen.' }
$dll = Join-Path $build 'Release\obs-sacn-crossfade.dll'
if (-not (Test-Path $dll)) { throw 'Release-DLL fehlt.' }
$release = Join-Path $root 'Release'
$bin = Join-Path $release 'obs-sacn-crossfade\bin\64bit'
$data = Join-Path $release 'obs-sacn-crossfade\data'
New-Item -ItemType Directory -Force -Path $bin,$data | Out-Null
Copy-Item $dll (Join-Path $bin 'obs-sacn-crossfade.dll') -Force
Copy-Item (Join-Path $root 'data\crossfade.effect') (Join-Path $data 'crossfade.effect') -Force
Write-Host "Release-Ordner erstellt: $release" -ForegroundColor Green
Write-Host 'NICHT automatisch installiert! OBS vorher beenden und bestehende DLL sichern.' -ForegroundColor Yellow
