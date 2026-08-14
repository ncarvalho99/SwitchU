<#
.SYNOPSIS
Publishes the standard 912x512 BC7 package for every SwitchU animated theme.

.DESCRIPTION
Run this from the Proxmox host that owns the theme container.  Each package is
first copied to a temporary host path, pushed into the container under a
".next" name, verified by SHA-256, then atomically renamed to theme.zip.

The script deliberately never handles passwords, disables host-key checking,
or reads settings from another local script.  Use an SSH key loaded in the
agent (or -IdentityFile) and provide the target explicitly.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $PackagesDirectory,
    [Parameter(Mandatory = $true)] [string] $SshTarget,
    [Parameter(Mandatory = $true)] [int] $ContainerId,
    [string] $ContainerThemeDirectory = '/srv/themes/themes',
    [string] $ReindexCommand = '/opt/switchu-themes/reindex.py',
    [ValidateRange(1, 256)] [int] $ExpectedPackageCount = 61,
    [string] $IdentityFile = '',
    [string] $LogPath = ''
)

$ErrorActionPreference = 'Stop'
$PackagesDirectory = (Resolve-Path -LiteralPath $PackagesDirectory).Path
if (-not $LogPath) { $LogPath = Join-Path $PackagesDirectory '..\bc7-catalog-publish.log' }

foreach ($command in 'ssh', 'scp') {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "Required OpenSSH command '$command' was not found in PATH."
    }
}

if ($SshTarget -notmatch '^[^\s:]+@[^\s:]+$') { throw 'SshTarget must be user@host.' }
if ($ContainerId -lt 1) { throw 'ContainerId must be positive.' }
if ($ContainerThemeDirectory -notmatch '^/[^\s]*$') { throw 'ContainerThemeDirectory must be an absolute path.' }
if ($ReindexCommand -match "[\r\n]") { throw 'ReindexCommand must be one line.' }

$sshOptions = @()
if ($IdentityFile) {
    $IdentityFile = (Resolve-Path -LiteralPath $IdentityFile).Path
    $sshOptions += @('-i', $IdentityFile)
}

function Quote-Posix([string] $Value) {
    # A single quote inside a shell single-quoted argument is represented by
    # closing it, emitting a quoted single quote, then reopening it: '"'"'.
    $singleQuote = [string][char]39
    $embeddedQuote = $singleQuote + '"' + $singleQuote + '"' + $singleQuote
    return $singleQuote + $Value.Replace($singleQuote, $embeddedQuote) + $singleQuote
}

function Write-ProgressLog([string] $Text) {
    $line = "$(Get-Date -Format s) $Text"
    $line | Tee-Object -FilePath $LogPath -Append
}
function Invoke-Remote([string] $Command) {
    & ssh @sshOptions $SshTarget $Command
    if ($LASTEXITCODE -ne 0) { throw "Remote command failed: $Command" }
}
function Sha256([string] $Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

$files = Get-ChildItem -LiteralPath $PackagesDirectory -Filter '*.zip' -File | Sort-Object Name
if ($files.Count -ne $ExpectedPackageCount) { throw "Expected $ExpectedPackageCount BC7 packages; found $($files.Count)." }
Write-ProgressLog "Publishing $($files.Count) BC7 packages."

$index = 0
foreach ($file in $files) {
    $index++
    $id = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    if ($id -notmatch '^[a-z0-9][a-z0-9_-]{1,63}$') { throw "Unsafe theme id: $id" }
    $expected = Sha256 $file.FullName
    $hostTemp = "/tmp/switchu-bc7-$id-$PID.zip"
    $containerNext = "$ContainerThemeDirectory/$id/theme.zip.bc7-next"
    $containerFinal = "$ContainerThemeDirectory/$id/theme.zip"
    Write-ProgressLog ("[{0}/{1}] uploading {2} ({3:N1} MB)" -f $index, $files.Count, $id, ($file.Length / 1MB))
    & scp @sshOptions -- $file.FullName "$SshTarget`:$hostTemp"
    if ($LASTEXITCODE -ne 0) { throw "SCP failed for $id" }
    $stage = "pct push $(Quote-Posix $hostTemp) $(Quote-Posix $containerNext) --perms 644 && rm -f $(Quote-Posix $hostTemp) && pct exec $ContainerId -- sha256sum $(Quote-Posix $containerNext)"
    $result = Invoke-Remote $stage
    $actual = (($result | Select-Object -Last 1) -split '\s+')[0].ToLowerInvariant()
    if ($actual -ne $expected) {
        Invoke-Remote "pct exec $ContainerId -- rm -f $(Quote-Posix $containerNext)"
        throw "SHA-256 mismatch for $id"
    }
    Invoke-Remote "pct exec $ContainerId -- mv -f $(Quote-Posix $containerNext) $(Quote-Posix $containerFinal)"
    Write-ProgressLog "[$index/$($files.Count)] published $id sha256=$actual"
}

Write-ProgressLog 'Rebuilding catalog index.'
$null = Invoke-Remote "pct exec $ContainerId -- $(Quote-Posix $ReindexCommand)"
$summary = Invoke-Remote "pct exec $ContainerId -- python3 -c $(Quote-Posix 'import json; d=json.load(open("/srv/themes/index.json")); a=[x for x in d["themes"] if x.get("package")]; print(len(d["themes"]), len(a))')"
Write-ProgressLog "Catalog validation: $summary"
if ($summary -notmatch "^$ExpectedPackageCount\s+$ExpectedPackageCount$") { throw "Unexpected catalog package summary: $summary" }
Write-ProgressLog 'COMPLETE'
