param([string]$CompilerRoot = (Join-Path $PSScriptRoot '.deps'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$spec = Get-Content (Join-Path $PSScriptRoot 'compiler.json') -Raw | ConvertFrom-Json
$infoPath = Join-Path $repo 'dist\BUILD-INFO.json'
if (-not (Test-Path -LiteralPath $infoPath)) {
    throw 'Zuerst tools/build_packages.py ausfuehren.'
}
$info = Get-Content -LiteralPath $infoPath -Raw | ConvertFrom-Json
$dll = Join-Path $repo 'OBS\Release\obs-sacn-crossfade\bin\64bit\obs-sacn-crossfade.dll'
if ((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ne $info.dllSha256) {
    throw 'DLL und BUILD-INFO.json passen nicht zusammen. Pakete erneut erzeugen.'
}
if ($info.obsPluginVersion -ne '0.46.0' -or $info.obsSdkVersion -ne '33.0.0') {
    throw 'Installer-Version und Paketinformationen passen nicht zusammen.'
}
New-Item -ItemType Directory -Path $CompilerRoot -Force | Out-Null
$CompilerRoot = (Resolve-Path -LiteralPath $CompilerRoot).Path
$download = Join-Path $CompilerRoot ('innosetup-' + $spec.version + '.exe')
if (-not (Test-Path -LiteralPath $download)) {
    $partial = $download + '.partial'
    Invoke-WebRequest -Uri $spec.url -OutFile $partial
    if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ne $spec.sha256) {
        throw 'Pruefsumme des Inno-Setup-Downloads stimmt nicht.'
    }
    Move-Item -LiteralPath $partial -Destination $download -Force
}
if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne $spec.sha256) {
    throw 'Pruefsumme des gespeicherten Inno-Setup-Compilers stimmt nicht.'
}
# Official portable mode: no compiler uninstall entry, shortcuts or associations.
# Extract on every build so the verified archive supplies the compiler files.
$compilerDir = Join-Path $CompilerRoot ('inno-' + $spec.version)
$compilerArgs = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-',
    '/PORTABLE=1', '/CURRENTUSER', ('/DIR="' + $compilerDir + '"'),
    ('/LOG="' + (Join-Path $CompilerRoot 'compiler-prepare.log') + '"'))
$process = Start-Process -FilePath $download -ArgumentList $compilerArgs -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "Compiler-Vorbereitung fehlgeschlagen: $($process.ExitCode)" }
$compiler = Join-Path $compilerDir 'ISCC.exe'
& $compiler ('/DRepoRoot=' + $repo) (Join-Path $PSScriptRoot 'MediaLink.iss')
if ($LASTEXITCODE -ne 0) { throw "Installer-Build fehlgeschlagen: $LASTEXITCODE" }
Write-Host 'Setup-Datei unter dist/ erstellt. Fuer Pruefsummen: python tools/build_packages.py --finalize-installer'
