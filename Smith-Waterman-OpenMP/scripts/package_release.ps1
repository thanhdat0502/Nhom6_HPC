[CmdletBinding()]
param(
    [string]$BuildDirectory = '',
    [string]$Destination = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'native_process.ps1')

try {
    $projectRoot = Split-Path -Parent $PSScriptRoot
    if (-not $BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'build-release' }
    $cachePath = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        throw "CMakeCache.txt not found. Configure/build Release first: cmake -S `"$projectRoot`" -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release"
    }
    $cache = [IO.File]::ReadAllText($cachePath)
    if ($cache -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$') {
        throw 'Packaging requires a single-configuration Release build, not Debug.'
    }
    if ($cache -notmatch '(?m)^CMAKE_CXX_COMPILER:(?:FILEPATH|STRING|UNINITIALIZED)=(.+)\r?$') {
        throw 'Cannot determine the compiler used by this build.'
    }
    $compiler = $Matches[1].Trim()
    $runtimeBin = Split-Path -Parent $compiler
    $prefix = Split-Path -Parent $runtimeBin
    $objdump = Join-Path $runtimeBin 'objdump.exe'
    if (-not (Test-Path -LiteralPath $objdump -PathType Leaf)) {
        throw 'Matching GNU objdump was not found next to the compiler. DLL dependencies cannot be verified.'
    }
    $executables = @('sw_cli.exe', 'sw_benchmark.exe')
    foreach ($name in $executables) {
        if (-not (Test-Path -LiteralPath (Join-Path $BuildDirectory $name) -PathType Leaf)) {
            throw "Release executable not found: $name. Run cmake --build `"$BuildDirectory`" first."
        }
    }
    $format = Invoke-SwProcess $objdump @('-f', (Join-Path $BuildDirectory 'sw_cli.exe'))
    if ($format.ExitCode -ne 0 -or $format.Stdout -notmatch 'file format pei-x86-64') {
        throw 'This packager supports the Windows x86-64 GCC/UCRT build only.'
    }
    if (-not $Destination) {
        $suffix = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
        $Destination = Join-Path $projectRoot "dist/Smith-Waterman-OpenMP-Release-$suffix"
    }
    $Destination = [IO.Path]::GetFullPath($Destination)
    $zipPath = $Destination + '.zip'
    if ((Test-Path -LiteralPath $Destination) -or (Test-Path -LiteralPath $zipPath)) {
        throw "Refusing to overwrite an existing package or ZIP: $Destination"
    }

    # Resolve imports recursively from the exact compiler prefix, never arbitrary PATH DLLs.
    # Windows API sets and OS DLLs stay supplied by Windows, not copied into the ZIP.
    $runtimeLicenses = @{
        'libgcc_s_seh-1.dll' = 'gcc'; 'libstdc++-6.dll' = 'gcc'
        'libgomp-1.dll' = 'gcc'; 'libwinpthread-1.dll' = 'winpthreads'
    }
    $pending = New-Object 'System.Collections.Generic.Queue[string]'
    foreach ($name in $executables) { $pending.Enqueue((Join-Path $BuildDirectory $name)) }
    $seen = @{}
    $dlls = @{}
    $imports = @()
    while ($pending.Count -gt 0) {
        $file = $pending.Dequeue()
        $name = Split-Path -Leaf $file
        if ($seen.ContainsKey($name)) { continue }
        $seen[$name] = $true
        $inspection = Invoke-SwProcess $objdump @('-p', $file)
        if ($inspection.ExitCode -ne 0) { throw "Cannot inspect DLL imports: $file" }
        $dependencies = @([regex]::Matches($inspection.Stdout, '(?m)^\s*DLL Name:\s*(\S+)\s*$') |
            ForEach-Object { $_.Groups[1].Value })
        if ($dependencies.Count -eq 0) { throw "No PE imports found; refusing an unverified package: $file" }
        $imports += [PSCustomObject]@{ File = $name; Imports = $dependencies }
        foreach ($dependency in $dependencies) {
            if ($runtimeLicenses.ContainsKey($dependency)) {
                $path = Join-Path $runtimeBin $dependency
                if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing runtime: $path" }
                if (-not $dlls.ContainsKey($dependency)) { $dlls[$dependency] = $path; $pending.Enqueue($path) }
            }
            elseif ($dependency -match '^(?:api-ms-win-|ext-ms-win-)' -or
                    (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32/$dependency") -PathType Leaf)) {
                # OS dependency: recorded in the manifest; requires compatible Windows/UCRT.
            }
            else { throw "Unresolved/unapproved DLL dependency: $dependency (imported by $name). No package created." }
        }
    }
    $licenseDirectories = @('gcc', 'winpthreads', 'crt')
    foreach ($directory in $licenseDirectories) {
        if (-not (Test-Path -LiteralPath (Join-Path $prefix "share/licenses/$directory") -PathType Container)) {
            throw "Runtime license directory missing: $directory. Refusing to distribute runtimes without their notices."
        }
    }

    # Only a new destination is created. Failures leave it for diagnosis, never delete user files.
    New-Item -ItemType Directory -Path $Destination | Out-Null
    $bin = Join-Path $Destination 'bin'
    New-Item -ItemType Directory -Path $bin | Out-Null
    foreach ($name in $executables) { Copy-Item -LiteralPath (Join-Path $BuildDirectory $name) -Destination $bin }
    foreach ($name in $dlls.Keys) { Copy-Item -LiteralPath $dlls[$name] -Destination $bin }
    foreach ($directory in $licenseDirectories) {
        $target = Join-Path $Destination "licenses/$directory"
        New-Item -ItemType Directory -Path $target | Out-Null
        Get-ChildItem -LiteralPath (Join-Path $prefix "share/licenses/$directory") -File |
            ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $target }
    }
    $samples = Join-Path $Destination 'data/samples'
    New-Item -ItemType Directory -Path $samples | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $projectRoot 'data/samples') -Filter '*.fasta' -File |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $samples }
    $scripts = Join-Path $Destination 'scripts'
    New-Item -ItemType Directory -Path $scripts | Out-Null
    foreach ($name in @('demo_week4.ps1', 'native_process.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $scripts
    }
    # Source handoff: an explicit text/data allowlist; no .git, cache, binaries or results.
    $source = Join-Path $Destination 'source'
    New-Item -ItemType Directory -Path $source | Out-Null
    foreach ($name in @('CMakeLists.txt', 'README.md', '.gitignore', '.gitattributes')) {
        Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination $source
    }
    $extensions = @('.cpp', '.h', '.cmake', '.ps1', '.py', '.md', '.fasta')
    foreach ($directory in @('src', 'include', 'tests', 'scripts', 'docs', 'data')) {
        Get-ChildItem -LiteralPath (Join-Path $projectRoot $directory) -File -Recurse | ForEach-Object {
            if ($_.Extension -in $extensions -and $_.FullName -notmatch '[\\/]__pycache__[\\/]') {
                if ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing source symlink: $($_.FullName)" }
                $relative = $_.FullName.Substring($projectRoot.Length + 1)
                $target = Join-Path $source $relative
                New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
                Copy-Item -LiteralPath $_.FullName -Destination $target
            }
        }
    }
    $instructions = @'
# Smith-Waterman OpenMP - Windows x86-64 Release

Requires a compatible Windows 10/11 x64 system with the Windows Universal CRT.
Keep the bundled GNU runtime DLLs beside both executables in bin/. They were
resolved recursively from this build's GCC prefix; see package_manifest.json.
This is not a claim of compatibility with every Windows version or CPU.
No toolchain installation is needed for the included binary demo on the tested host.

From this extracted directory in Windows PowerShell:

    .\bin\sw_cli.exe --help
    .\bin\sw_cli.exe align --seq1 ACGT --seq2 ACGT
    powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\demo_week4.ps1 -ExecutablePath .\bin\sw_cli.exe

ExecutionPolicy Bypass applies only to the child PowerShell process; it does not
change machine or user policy. Omit it if your policy already permits scripts.
FASTA samples are in data/samples/. The demo compares serial/static/dynamic
outputs and does not write data or rerun the main benchmark.

Optional tiny Release benchmark (choose a NEW output directory):

    .\bin\sw_benchmark.exe --suite smoke --threads 1 --warmup 2 --runs 7 --output smoke-new

Source code, tests, scripts and technical notes are included in source/.
Building source requires GCC/UCRT64, OpenMP, CMake and Ninja; plotting requires
Python/Matplotlib. These development tools are not included in this package.
From source/: cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
Then: cmake --build build-release; ctest --test-dir build-release --output-on-failure

The large historical results/ tree is intentionally not in this delivery ZIP;
consult the original repository for week 3 CSV/PNG and their provenance.
GNU GCC runtime and MinGW-w64 notices are preserved in licenses/. GCC runtime
libraries use the GCC Runtime Library Exception; winpthreads has its own COPYING.
Installed toolchain sources: https://www.msys2.org/ and https://gcc.gnu.org/.
'@
    [IO.File]::WriteAllText((Join-Path $Destination 'README_RUN.md'), $instructions + "`n", (New-Object Text.UTF8Encoding($false)))

    # Verify copied binaries with PATH stripped of all GCC/MSYS2/project directories.
    $cleanEnvironment = @{
        PATH = "$env:SystemRoot\System32;$env:SystemRoot"
        OMP_DYNAMIC = 'FALSE'; OMP_WAIT_POLICY = 'PASSIVE'
        OMP_THREAD_LIMIT = $null; OMP_NUM_THREADS = $null
        OMP_PROC_BIND = $null; OMP_PLACES = $null
    }
    $cli = Join-Path $bin 'sw_cli.exe'
    $query = Join-Path $samples 'queries.fasta'
    $references = Join-Path $samples 'references.fasta'
    $align = Invoke-SwProcess $cli @('align', '--seq1', 'ACGT', '--seq2', 'ACGT') $Destination $cleanEnvironment
    if ($align.ExitCode -ne 0 -or $align.Stdout.Trim() -ne 'Score: 8' -or $align.Stderr) { throw 'Packaged align failed.' }
    $base = @('search', '--query', $query, '--query-id', 'query_2', '--references', $references)
    $serial = Invoke-SwProcess $cli ($base + @('--mode', 'serial')) $Destination $cleanEnvironment
    $help = Invoke-SwProcess $cli @('--help') $Destination $cleanEnvironment
    if ($serial.ExitCode -ne 0 -or $help.ExitCode -ne 0 -or $help.Stdout -notmatch 'Max supported batch threads: (\d+)') {
        throw 'Packaged search/help failed.'
    }
    $threads = [Math]::Min(4, [int]$Matches[1])
    $teams = @()
    foreach ($schedule in @('static', 'dynamic')) {
        $parallel = Invoke-SwProcess $cli ($base + @('--mode', 'parallel', '--threads', [string]$threads, '--schedule', $schedule, '--stats')) $Destination $cleanEnvironment
        if ($parallel.ExitCode -ne 0 -or $parallel.Stdout -cne $serial.Stdout -or $parallel.Stderr -notmatch 'Actual threads: (\d+)') {
            throw "Packaged $schedule search failed or scores/order differ."
        }
        $teams += [PSCustomObject]@{ Schedule = $schedule; Requested = $threads; Actual = [int]$Matches[1] }
    }
    $demo = Invoke-SwProcess (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $scripts 'demo_week4.ps1'), '-ExecutablePath', $cli
    ) $Destination $cleanEnvironment
    if ($demo.ExitCode -ne 0 -or $demo.Stdout -notmatch 'Demo complete') { throw "Packaged demo failed: $($demo.Stderr)" }
    $benchHelp = Invoke-SwProcess (Join-Path $bin 'sw_benchmark.exe') @('--help') $Destination $cleanEnvironment
    if ($benchHelp.ExitCode -ne 0 -or $benchHelp.Stdout -notmatch 'warmup') { throw 'Packaged benchmark executable failed.' }
    $inventory = @(Get-ChildItem -LiteralPath $Destination -File -Recurse | Sort-Object FullName | ForEach-Object {
        [PSCustomObject]@{ Path = $_.FullName.Substring($Destination.Length + 1).Replace('\', '/'); Bytes = $_.Length
            SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    $manifest = [PSCustomObject]@{
        Status = 'verified_on_build_host'; CreatedUtc = [DateTime]::UtcNow.ToString('o')
        BuildType = 'Release'; Architecture = 'Windows x86-64'; Compiler = $compiler
        RuntimeDirectory = $runtimeBin; RuntimeDlls = @($dlls.Keys | Sort-Object)
        Imports = $imports; CleanPathSmokePassed = $true; ParallelTeams = $teams
        CompatibilityLimit = 'Tested on the build host with a minimal Windows PATH; no separate-machine certification. OS API sets/UCRT supplied by Windows.'
        Files = $inventory
    }
    [IO.File]::WriteAllText((Join-Path $Destination 'package_manifest.json'), ($manifest | ConvertTo-Json -Depth 8) + "`n", (New-Object Text.UTF8Encoding($false)))
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($Destination, $zipPath)
    Write-Host "Package verified: $Destination"
    Write-Host "ZIP: $zipPath"
    Write-Host "ZIP SHA256: $((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant())"
    exit 0
}
catch {
    [Console]::Error.WriteLine("Packaging failed: $($_.Exception.Message)")
    [Console]::Error.WriteLine('No existing package was overwritten. A newly created incomplete directory, if any, is retained for diagnosis.')
    exit 1
}
