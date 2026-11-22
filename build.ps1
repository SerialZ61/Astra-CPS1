param([string]$DevkitPro = 'C:\devkitPro')
$ErrorActionPreference = 'Stop'
$bash = Join-Path $DevkitPro 'msys2\usr\bin\bash.exe'
if (!(Test-Path -LiteralPath $bash)) { throw "Shell devkitPro introuvable : $bash" }
$env:ASTRA_CPS1_PROJECT = $PSScriptRoot
& $bash -lc 'cd "$(cygpath -u "$ASTRA_CPS1_PROJECT")" && make -j6'
exit $LASTEXITCODE
