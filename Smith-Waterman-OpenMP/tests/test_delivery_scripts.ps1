param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot,
    [Parameter(Mandatory = $true)][string]$ExecutablePath,
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$BuildType
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $ProjectRoot 'scripts/native_process.ps1')
$passed = 0

function Check {
    param([string]$Label, [bool]$Condition)
    if (-not $Condition) { throw "FAIL $Label" }
    $script:passed++
    Write-Host "PASS $Label"
}

try {
    $work = Join-Path $BuildDirectory ('delivery tests ' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work | Out-Null
    $powershell = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    $demoArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $ProjectRoot 'scripts/demo_week4.ps1'))
    $packageArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $ProjectRoot 'scripts/package_release.ps1'))
    $samples = Join-Path $ProjectRoot 'data/samples'
    $cli = Invoke-SwProcess $ExecutablePath @('align', '--seq1', '', '--seq2', 'ACGT') $work
    Check 'native_empty_argument' ($cli.ExitCode -eq 0 -and $cli.Stdout.Trim() -eq 'Score: 0' -and -not $cli.Stderr)
    $cli = Invoke-SwProcess $ExecutablePath @('align', '--seq1', 'A" C\', '--seq2', 'ACGT') $work
    Check 'native_quoted_argument' ($cli.ExitCode -eq 1 -and -not $cli.Stdout -and $cli.Stderr -match '^Error: Invalid DNA')

    $demo = Invoke-SwProcess $powershell ($demoArgs + @('-ExecutablePath', $ExecutablePath)) $work
    Check 'demo_from_other_working_directory' ($demo.ExitCode -eq 0 -and $demo.Stdout -match 'Demo complete' -and -not $demo.Stderr)
    $demo = Invoke-SwProcess $powershell ($demoArgs + @('-ExecutablePath', (Join-Path $work 'missing.exe'))) $work
    Check 'missing_executable_gives_build_hint' ($demo.ExitCode -eq 1 -and $demo.Stderr -match 'Build first:' -and $demo.Stdout -notmatch 'Demo complete')
    $demo = Invoke-SwProcess $powershell ($demoArgs + @('-ExecutablePath', $ExecutablePath, '-SampleDirectory', $work)) $work
    Check 'missing_FASTA_is_not_success' ($demo.ExitCode -eq 1 -and $demo.Stderr -match 'Sample FASTA not found' -and $demo.Stdout -notmatch 'Demo complete')
    $demo = Invoke-SwProcess $powershell ($demoArgs + @('-ExecutablePath', $ExecutablePath)) $work @{ OMP_THREAD_LIMIT = '1' }
    Check 'demo_respects_one_thread_limit' ($demo.ExitCode -eq 0 -and $demo.Stdout -match 'Parallel request: 1 thread' -and $demo.Stdout -notmatch 'Actual threads: [2-9]')

    $missingOutput = Join-Path $work 'must not be created'
    $package = Invoke-SwProcess $powershell ($packageArgs + @('-BuildDirectory', (Join-Path $work 'missing build'), '-Destination', $missingOutput)) $work
    Check 'package_requires_build_cache' ($package.ExitCode -eq 1 -and $package.Stderr -match 'CMakeCache.txt not found' -and -not (Test-Path -LiteralPath $missingOutput))
    $fakeDebug = Join-Path $work 'debug cache'
    New-Item -ItemType Directory -Path $fakeDebug | Out-Null
    [IO.File]::WriteAllText((Join-Path $fakeDebug 'CMakeCache.txt'), "CMAKE_BUILD_TYPE:STRING=Debug`n")
    $package = Invoke-SwProcess $powershell ($packageArgs + @('-BuildDirectory', $fakeDebug, '-Destination', $missingOutput)) $work
    Check 'package_rejects_debug' ($package.ExitCode -eq 1 -and $package.Stderr -match 'requires.*Release' -and -not (Test-Path -LiteralPath $missingOutput))

    if ($BuildType -eq 'Release') {
        $destination = Join-Path $work 'release handoff'
        $package = Invoke-SwProcess $powershell ($packageArgs + @('-BuildDirectory', $BuildDirectory, '-Destination', $destination)) $work @{} 60
        if ($package.ExitCode -ne 0) { throw "Package creation failed: $($package.Stdout) $($package.Stderr)" }
        $manifestPath = Join-Path $destination 'package_manifest.json'
        $manifest = [IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
        Check 'release_package_and_recursive_runtime_manifest' ($manifest.Status -eq 'verified_on_build_host' -and $manifest.CleanPathSmokePassed -and $manifest.RuntimeDlls.Count -eq 4)
        $before = (Get-FileHash -LiteralPath $manifestPath).Hash
        $package = Invoke-SwProcess $powershell ($packageArgs + @('-BuildDirectory', $BuildDirectory, '-Destination', $destination)) $work
        Check 'package_refuses_existing_directory' ($package.ExitCode -eq 1 -and $package.Stderr -match 'Refusing to overwrite' -and (Get-FileHash -LiteralPath $manifestPath).Hash -eq $before)
        $zipOnly = Join-Path $work 'zip exists'
        [IO.File]::WriteAllText(($zipOnly + '.zip'), 'existing zip sentinel')
        $package = Invoke-SwProcess $powershell ($packageArgs + @('-BuildDirectory', $BuildDirectory, '-Destination', $zipOnly)) $work
        Check 'package_refuses_existing_zip' ($package.ExitCode -eq 1 -and $package.Stderr -match 'Refusing to overwrite' -and [IO.File]::ReadAllText($zipOnly + '.zip') -eq 'existing zip sentinel' -and -not (Test-Path -LiteralPath $zipOnly))

        $clean = @{ PATH = "$env:SystemRoot\System32;$env:SystemRoot"; OMP_DYNAMIC = 'FALSE'; OMP_THREAD_LIMIT = $null; OMP_NUM_THREADS = $null }
        # Extract outside the repository and toolchain, with spaces in the path.
        $isolated = Join-Path ([IO.Path]::GetTempPath()) ('sw week4 extracted ' + [Guid]::NewGuid().ToString('N'))
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [IO.Compression.ZipFile]::ExtractToDirectory(($destination + '.zip'), $isolated)
        $demo = Invoke-SwProcess $powershell @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $isolated 'scripts/demo_week4.ps1'), '-ExecutablePath', (Join-Path $isolated 'bin/sw_cli.exe')) $isolated $clean
        Check 'extracted_zip_demo_with_windows_only_PATH' ($demo.ExitCode -eq 0 -and $demo.Stdout -match 'Demo complete' -and -not $demo.Stderr)
        $smokeDir = Join-Path $isolated 'benchmark smoke'
        $bench = Invoke-SwProcess (Join-Path $isolated 'bin/sw_benchmark.exe') @('--suite', 'smoke', '--threads', '1', '--warmup', '2', '--runs', '7', '--output', $smokeDir) $isolated $clean
        if ($bench.ExitCode -ne 0) { throw "Packaged benchmark failed: $($bench.Stderr)" }
        $rows = @(Import-Csv -LiteralPath (Join-Path $smokeDir 'raw_benchmark.csv'))
        $summary = @(Import-Csv -LiteralPath (Join-Path $smokeDir 'summary_benchmark.csv'))
        $meta = [IO.File]::ReadAllText((Join-Path $smokeDir 'run_metadata.json')) | ConvertFrom-Json
        Check 'extracted_release_benchmark_smoke' ($meta.status -eq 'complete' -and $meta.build_type -eq 'Release' -and $rows.Count -eq 54 -and $summary.Count -eq 6 -and @($rows | Where-Object { $_.verified -ne '1' -or $_.actual_threads -ne '1' }).Count -eq 0)
        $inventoryCorrect = $true
        foreach ($file in $manifest.Files) {
            $path = Join-Path $isolated $file.Path
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ne $file.SHA256) { $inventoryCorrect = $false }
        }
        Check 'extracted_inventory_hashes' $inventoryCorrect
        $unwanted = @(Get-ChildItem -LiteralPath $isolated -Recurse -Force | Where-Object { $_.Name -in @('.git', 'CMakeCache.txt', '__pycache__') })
        Check 'source_handoff_without_cache_or_toolchain' ($unwanted.Count -eq 0 -and (Test-Path -LiteralPath (Join-Path $isolated 'source/src/smith_waterman_serial.cpp')) -and (Test-Path -LiteralPath (Join-Path $isolated 'source/tests/test_parallel_exceptions.cpp')))
        Write-Host "Extracted validation directory: $isolated"
    }
    Write-Host "Passed: $passed delivery script cases; Failed: 0"
    exit 0
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
