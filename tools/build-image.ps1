[CmdletBinding()]
param(
    [switch] $PullBase
)

$ErrorActionPreference = 'Stop'
$dockerRoot = 'C:\Program Files\Docker\Docker\resources\bin'
$docker = Join-Path $dockerRoot 'docker.exe'

if (-not (Test-Path -LiteralPath $docker)) {
    throw "Docker Desktop was not found at $docker"
}

# Docker Desktop installs its credential helper beside docker.exe but does not
# always add this directory to PATH after a fresh Windows install.  The helper
# is required even for anonymous pulls from Docker Hub.
$env:PATH = "$dockerRoot;$env:PATH"

if ($PullBase) {
    & $docker pull 'devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282'
    if ($LASTEXITCODE -ne 0) { throw 'Could not pull the pinned devkitPro base image.' }
}

$repo = Split-Path -Parent $PSScriptRoot
& $docker build --tag switchu-build --file (Join-Path $PSScriptRoot 'Dockerfile.build') $repo
if ($LASTEXITCODE -ne 0) { throw 'SwitchU build image failed.' }

& $docker image inspect switchu-build --format 'switchu-build {{.Id}} {{.Created}}'
