$ErrorActionPreference = 'Stop'
$base = $PSScriptRoot
$release = Join-Path $base 'Release\obs-sacn-crossfade'
$dll = Join-Path $release 'bin\64bit\obs-sacn-crossfade.dll'
$shader = Join-Path $release 'data\crossfade.effect'
$plugin = 'C:\ProgramData\obs-studio\plugins\obs-sacn-crossfade'
$targetDll = Join-Path $plugin 'bin\64bit\obs-sacn-crossfade.dll'
$targetShader = Join-Path $plugin 'data\crossfade.effect'
if (-not (Test-Path $dll)) { throw "Release-DLL fehlt. Zuerst Build ausfuehren: $dll" }
if (-not (Test-Path $shader)) { throw "Shader fehlt: $shader" }
if (-not (Test-Path $targetDll)) { throw "Bisheriges Plugin fehlt: $targetDll" }
if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw 'OBS ist noch geoeffnet. Bitte OBS schliessen.' }
$backupRoot = 'C:\OBS-Development\releases'
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
$backup = Join-Path $backupRoot ('MA_OBS_MediaLink_Pre045_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
Copy-Item -LiteralPath $plugin -Destination $backup -Recurse -ErrorAction Stop
Copy-Item -LiteralPath $dll -Destination $targetDll -Force -ErrorAction Stop
Copy-Item -LiteralPath $shader -Destination $targetShader -Force -ErrorAction Stop
$sourceHash = (Get-FileHash $dll -Algorithm SHA256).Hash
$targetHash = (Get-FileHash $targetDll -Algorithm SHA256).Hash
if ($sourceHash -ne $targetHash) { throw 'DLL-Hash ungleich; Installation nicht bestaetigt' }
Write-Host "OK - v0.45 DLL installiert. SHA256: $targetHash" -ForegroundColor Green
Write-Host "Backup: $backup"
