<#
.SYNOPSIS
Builds SwitchU locally on Windows through the pinned Docker environment.

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build-local.ps1 -Mode release -Variant sysmodule
#>
[CmdletBinding()]
param(
    [ValidateSet('release', 'debug')] [string] $Mode = 'release',
    [ValidateSet('sysmodule', 'homebrew')] [string] $Variant = 'sysmodule'
)

$ErrorActionPreference = 'Stop'
$dockerRoot = 'C:\Program Files\Docker\Docker\resources\bin'
$docker = Join-Path $dockerRoot 'docker.exe'

if (-not (Test-Path -LiteralPath $docker)) {
    throw "Docker Desktop was not found at $dockerRoot"
}

# Docker Desktop's credential helpers live beside docker.exe. A new Windows
# installation may omit this directory from PATH, which breaks anonymous pulls.
$env:PATH = "$dockerRoot;$env:PATH"

& $docker image inspect switchu-build 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "A imagem local 'switchu-build' nao existe. Execute .\tools\build-image.ps1 -PullBase primeiro."
}

$repo = Split-Path -Parent $PSScriptRoot
$logsDirectory = Join-Path $repo '.logs'
New-Item -ItemType Directory -Force -Path $logsDirectory | Out-Null
$log = Join-Path $logsDirectory 'build-local.log'

if (& $docker ps -q --filter ancestor=switchu-build) {
    throw 'Ja existe um build SwitchU rodando. Espere ele terminar antes de iniciar outro.'
}

Write-Host "build $Mode/$Variant -> $log" -ForegroundColor Cyan
$started = Get-Date

# xmake installs packages below /root/.xmake. Keep that state in a named volume:
# its project cache survives on the bind mount, so discarding package state at
# every --rm run would make later builds incorrectly think dependencies exist.
$ErrorActionPreference = 'Continue'
& $docker run --rm -v "${repo}:/src" -v switchu-xmake:/root/.xmake `
    switchu-build bash /src/tools/build-inside.sh $Mode $Variant |
    Tee-Object -FilePath $log
$rc = $LASTEXITCODE
$ErrorActionPreference = 'Stop'

$seconds = [int]((Get-Date) - $started).TotalSeconds
$minutes = [int]($seconds / 60)
$elapsed = if ($minutes -gt 0) { "${minutes}m $($seconds % 60)s" } else { "${seconds}s" }

if ($rc -eq 0) {
    Write-Host "ok em $elapsed" -ForegroundColor Green
} else {
    Write-Host "falhou (exit $rc) em $elapsed -- log completo em $log" -ForegroundColor Red
}
exit $rc
