[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure {
    param([string]$Message)
    $script:failures.Add($Message)
}

function Read-Normalized {
    param([string]$Path)
    return ((Get-Content -Raw -LiteralPath $Path) -replace "`r`n", "`n").TrimEnd("`n")
}

$required = @(
    "AGENTS.md",
    "CLAUDE.md",
    ".opencode/AGENTS.md",
    ".antigravity/rules.md",
    "docs/README.md",
    "docs/chapters/01_architecture.md",
    "docs/chapters/02_project_structure.md",
    "docs/chapters/03_changelog.md",
    ".agents/skills/switchu-protocol/SKILL.md",
    ".claude/skills/switchu-protocol/SKILL.md"
)

foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $relative) -PathType Leaf)) {
        Add-Failure "Missing required file: $relative"
    }
}

$expectedWrappers = @{
    "CLAUDE.md" = "@AGENTS.md"
    ".opencode/AGENTS.md" = "Read and follow the repository-root [AGENTS.md](../AGENTS.md)."
    ".antigravity/rules.md" = "Read and follow the repository-root [AGENTS.md](../AGENTS.md)."
}

foreach ($relative in $expectedWrappers.Keys) {
    $path = Join-Path $repoRoot $relative
    if ((Test-Path -LiteralPath $path) -and
        (Read-Normalized $path) -cne $expectedWrappers[$relative]) {
        Add-Failure "Wrapper contains content beyond the root-AGENTS pointer: $relative"
    }
}

$indexPath = Join-Path $repoRoot "docs/README.md"
if (Test-Path -LiteralPath $indexPath) {
    $indexText = Get-Content -Raw -LiteralPath $indexPath
    $links = [regex]::Matches($indexText, '\[[^\]]+\]\(([^)]+)\)')
    foreach ($match in $links) {
        $target = $match.Groups[1].Value.Split('#')[0]
        if ($target -and $target -notmatch '^(?:https?://|mailto:)') {
            $resolved = Join-Path (Split-Path -Parent $indexPath) $target
            if (-not (Test-Path -LiteralPath $resolved)) {
                Add-Failure "Indexed path does not exist: $target"
            }
        }
    }
}

$chapterDir = Join-Path $repoRoot "docs/chapters"
if (Test-Path -LiteralPath $chapterDir) {
    foreach ($chapter in Get-ChildItem -LiteralPath $chapterDir -Filter "*.md" -File) {
        $lineCount = @(Get-Content -LiteralPath $chapter.FullName).Count
        if ($lineCount -ge 200) {
            Add-Failure "Chapter must remain below 200 lines: $($chapter.Name) has $lineCount"
        }
    }
}

$skillPath = Join-Path $repoRoot ".agents/skills/switchu-protocol/SKILL.md"
if (Test-Path -LiteralPath $skillPath) {
    $skillText = Get-Content -Raw -LiteralPath $skillPath
    if ($skillText -notmatch '(?s)^---\s+name:\s*switchu-protocol\s+description:.+?\s+---') {
        Add-Failure "Canonical skill frontmatter is missing or malformed"
    }
    if ($skillText -match '(?i)(?:[A-Z]:[\\/]|/Users/|/home/|\\Users\\)') {
        Add-Failure "Canonical skill contains a personal or absolute workstation path"
    }
}

$pointerPath = Join-Path $repoRoot ".claude/skills/switchu-protocol/SKILL.md"
if (Test-Path -LiteralPath $pointerPath) {
    $expectedPointer = @'
---
name: switchu-protocol
description: Route work in the SwitchU repository or explicit SwitchU project requests to the canonical project skill. Do not use for unrelated repositories or generic Nintendo Switch topics.
---

Read and follow `../../../.agents/skills/switchu-protocol/SKILL.md`. That file
is canonical; this Claude Code discovery pointer adds no independent rules.
'@
    $expectedPointer = ($expectedPointer -replace "`r`n", "`n").TrimEnd("`n")
    if ((Read-Normalized $pointerPath) -cne $expectedPointer) {
        Add-Failure "Claude skill pointer does not resolve exclusively to the canonical skill"
    }
}

Push-Location $repoRoot
try {
    & git check-ignore -q -- ".agents/locks/protocol-validation-probe.lock"
    if ($LASTEXITCODE -ne 0) {
        Add-Failure "Runtime lock files are not ignored"
    }

    & git check-ignore -q -- "prompt_handoff_switchu.md"
    if ($LASTEXITCODE -ne 0) {
        Add-Failure "Dynamic handoff is not ignored"
    }

    foreach ($trackedProtocolPath in @(
        "AGENTS.md",
        "CLAUDE.md",
        ".agents/skills/switchu-protocol/SKILL.md",
        ".claude/skills/switchu-protocol/SKILL.md",
        ".opencode/AGENTS.md",
        ".antigravity/rules.md"
    )) {
        & git check-ignore -q --no-index -- $trackedProtocolPath
        if ($LASTEXITCODE -eq 0) {
            Add-Failure "Trackable protocol file is ignored: $trackedProtocolPath"
        }
    }
}
finally {
    Pop-Location
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        [Console]::Error.WriteLine("FAIL: $failure")
    }
    throw "Agent protocol validation failed with $($failures.Count) error(s)."
}

Write-Output "PASS: protocol paths, chapter limits, wrappers, canonical skill, and ignore boundaries are valid."
