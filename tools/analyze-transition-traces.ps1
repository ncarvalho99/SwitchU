[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string[]]$Path,

    [string]$CsvPath
)

$ErrorActionPreference = 'Stop'

function Get-Percentile {
    param(
        [double[]]$Sorted,
        [double]$Percentile
    )

    if ($Sorted.Count -eq 0) { return 0.0 }
    $index = [Math]::Max(0, [Math]::Ceiling($Percentile * $Sorted.Count) - 1)
    return $Sorted[$index]
}

function Get-Median {
    param([double[]]$Sorted)

    if ($Sorted.Count -eq 0) { return 0.0 }
    $middle = [int][Math]::Floor($Sorted.Count / 2)
    if (($Sorted.Count % 2) -eq 1) { return $Sorted[$middle] }
    return ($Sorted[$middle - 1] + $Sorted[$middle]) / 2.0
}

$files = foreach ($entry in $Path) {
    Get-ChildItem -Path $entry -File -ErrorAction Stop
}
$files = @($files | Sort-Object FullName -Unique)
if ($files.Count -eq 0) {
    throw 'No log files matched.'
}

$samples = [System.Collections.Generic.List[object]]::new()
foreach ($file in $files) {
    foreach ($line in Get-Content -LiteralPath $file.FullName) {
        if ($line -notmatch '\[trace-(?<kind>[a-z-]+)\]\s+(?<payload>.*)$') {
            continue
        }

        $kind = $Matches.kind
        $payload = $Matches.payload
        $fields = @{}
        foreach ($match in [regex]::Matches($payload, '(?<key>[A-Za-z0-9_]+)=(?<value>[^\s]+)')) {
            $fields[$match.Groups['key'].Value] = $match.Groups['value'].Value
        }

        $case = $kind
        if ($fields.ContainsKey('title')) {
            $case += ':' + $fields['title']
        } elseif ($fields.ContainsKey('reason')) {
            $case += ':reason-' + $fields['reason']
        }

        foreach ($key in $fields.Keys) {
            if (-not $key.EndsWith('_us')) { continue }
            [double]$microseconds = 0
            if (-not [double]::TryParse(
                    $fields[$key],
                    [Globalization.NumberStyles]::Integer,
                    [Globalization.CultureInfo]::InvariantCulture,
                    [ref]$microseconds)) {
                continue
            }

            $samples.Add([pscustomobject]@{
                Case = $case
                Metric = $key.Substring(0, $key.Length - 3) + '_ms'
                Milliseconds = $microseconds / 1000.0
                Source = $file.FullName
            })
        }
    }
}

if ($samples.Count -eq 0) {
    throw 'No [trace-*] microsecond metrics found. Test the trace-enabled build first.'
}

$summary = foreach ($group in $samples | Group-Object Case, Metric) {
    [double[]]$values = @($group.Group.Milliseconds | Sort-Object)
    $median = Get-Median -Sorted $values
    [double[]]$deviations = @($values | ForEach-Object { [Math]::Abs($_ - $median) } | Sort-Object)

    [pscustomobject]@{
        Case = $group.Group[0].Case
        Metric = $group.Group[0].Metric
        Samples = $values.Count
        MinMs = [Math]::Round($values[0], 3)
        P50Ms = [Math]::Round($median, 3)
        P90Ms = [Math]::Round((Get-Percentile -Sorted $values -Percentile 0.90), 3)
        P95Ms = [Math]::Round((Get-Percentile -Sorted $values -Percentile 0.95), 3)
        P99Ms = [Math]::Round((Get-Percentile -Sorted $values -Percentile 0.99), 3)
        MaxMs = [Math]::Round($values[-1], 3)
        MeanMs = [Math]::Round((($values | Measure-Object -Average).Average), 3)
        MadMs = [Math]::Round((Get-Median -Sorted $deviations), 3)
    }
}

$summary = @($summary | Sort-Object Case, Metric)
$summary | Format-Table -AutoSize

if ($CsvPath) {
    $summary | Export-Csv -LiteralPath $CsvPath -NoTypeInformation -Encoding UTF8
    Write-Host "CSV: $CsvPath"
}
