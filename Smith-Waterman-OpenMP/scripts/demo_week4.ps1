[CmdletBinding()]
param(
    [string]$ExecutablePath = '',
    [string]$SampleDirectory = '',
    [string]$BenchmarkDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'native_process.ps1')

try {
    $projectRoot = Split-Path -Parent $PSScriptRoot
    if (-not $ExecutablePath) { $ExecutablePath = Join-Path $projectRoot 'build-release/sw_cli.exe' }
    if (-not $SampleDirectory) { $SampleDirectory = Join-Path $projectRoot 'data/samples' }
    if (-not $BenchmarkDirectory) { $BenchmarkDirectory = Join-Path $projectRoot 'results/week3_validation' }
    if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
        throw "Release executable not found: $ExecutablePath`nBuild first:`ncmake -S `"$projectRoot`" -B `"$projectRoot/build-release`" -G Ninja -DCMAKE_BUILD_TYPE=Release`ncmake --build `"$projectRoot/build-release`""
    }
    $exe = (Resolve-Path -LiteralPath $ExecutablePath).Path
    $query = Join-Path $SampleDirectory 'queries.fasta'
    $references = Join-Path $SampleDirectory 'references.fasta'
    foreach ($file in @($query, $references)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Sample FASTA not found: $file" }
    }
    $query = (Resolve-Path -LiteralPath $query).Path
    $references = (Resolve-Path -LiteralPath $references).Path
    $runtime = @{ OMP_DYNAMIC = 'FALSE' }

    function Invoke-DemoCli {
        param([string]$Label, [string[]]$CliArguments)
        Write-Host "`n--- $Label ---"
        $result = Invoke-SwProcess -FilePath $exe -Arguments $CliArguments -Environment $runtime
        if ($result.ExitCode -ne 0) { throw "$Label failed (exit $($result.ExitCode)): $($result.Stderr)" }
        Write-Host $result.Stdout.TrimEnd()
        if ($result.Stderr) { Write-Host $result.Stderr.TrimEnd() }
        return $result
    }

    $help = Invoke-SwProcess -FilePath $exe -Arguments @('--help') -Environment $runtime
    if ($help.ExitCode -ne 0 -or $help.Stdout -notmatch 'Max supported batch threads: (\d+)') {
        throw 'Cannot determine the supported thread count from CLI help.'
    }
    $threads = [Math]::Min(4, [int]$Matches[1])
    if ($threads -lt 1) { throw 'The CLI reported an invalid thread limit.' }
    Write-Host "Executable: $exe"
    Write-Host "Parallel request: $threads thread(s); runtime statistics below report the actual team."

    $score = Invoke-DemoCli 'Serial alignment score' @('align', '--seq1', 'ACGT', '--seq2', 'ACGT')
    if ($score.Stdout.Trim() -ne 'Score: 8') { throw 'Known alignment score must be 8.' }
    $trace = Invoke-DemoCli 'Traceback with a gap' @('align', '--seq1', 'AAAACCCC', '--seq2', 'AAAAGCCCC', '--alignment')
    if ($trace.Stdout -notmatch 'Score: 14' -or $trace.Stdout -notmatch 'AAAA-CCCC') {
        throw 'Known traceback/score did not match.'
    }
    Write-Host "`n--- Sample FASTA (read by search below) ---"
    Write-Host ([IO.File]::ReadAllText($query).TrimEnd())
    Write-Host ([IO.File]::ReadAllText($references).TrimEnd())
    $search = @('search', '--query', $query, '--query-id', 'query_2', '--references', $references)
    $serial = Invoke-DemoCli 'Serial FASTA search' ($search + @('--mode', 'serial', '--stats'))
    foreach ($schedule in @('default', 'static', 'dynamic')) {
        $cliArguments = $search + @('--mode', 'parallel', '--threads', [string]$threads, '--stats')
        if ($schedule -ne 'default') { $cliArguments += @('--schedule', $schedule) }
        $parallel = Invoke-DemoCli "Parallel FASTA search ($schedule scheduling)" $cliArguments
        if ($parallel.Stdout -cne $serial.Stdout) { throw "Score/order mismatch with $schedule scheduling." }
        if ($parallel.Stderr -notmatch 'Actual threads: (\d+)' -or [int]$Matches[1] -lt 1) {
            throw 'Missing actual thread count.'
        }
    }
    Write-Host "`nPASS: serial/default/static/dynamic outputs match, including FASTA order."
    if (Test-Path -LiteralPath (Join-Path $BenchmarkDirectory 'summary_benchmark.csv') -PathType Leaf) {
        Write-Host "Existing benchmark results (not rerun): $([IO.Path]::GetFullPath($BenchmarkDirectory))"
    }
    else { Write-Host "Existing benchmark results are not included here; project location: $BenchmarkDirectory" }
    Write-Host 'Demo complete. No FASTA or benchmark results were written.'
    exit 0
}
catch {
    [Console]::Error.WriteLine("Demo failed: $($_.Exception.Message)")
    exit 1
}
