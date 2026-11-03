# Static checks for VKE: clang-format, clang-tidy and Cppcheck.
#
#   tools/check.ps1            what changed against HEAD (edits and new files), only problems on changed lines
#   tools/check.ps1 -Fix       the same, formatting the changed files first
#   tools/check.ps1 -All       every source file, every problem
#
# Claude Code runs it through two hooks (.claude/settings.json):
#   -Track   after Edit/Write: remembers the C++ file Claude touched
#   -Hook    when Claude stops: checks the touched files' changed lines; exit 2 sends the problems back to
#            Claude, and the list is cleared once they're clean
#
# clang-tidy and Cppcheck read build/compile_commands.json, so the build tree has to be configured and built
# (the reflection step generates headers the sources include). Exit code 0 means clean.
param(
    [switch]$All,
    [switch]$Fix,
    [switch]$Hook,
    [switch]$Track,
    [string]$Base = 'HEAD',
    [string]$BuildDir = 'build',
    [int]$Jobs = [Environment]::ProcessorCount
)

# Native tools write to stderr; under 'Stop' Windows PowerShell would turn that into errors.
$ErrorActionPreference = 'Continue'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $Root

$sourcePattern = '^(Engine|Editor|runtime|tools)/.*\.(h|hpp|cpp)$'
$cacheDir = Join-Path $Root "$BuildDir\.check"
$touchedFile = Join-Path $cacheDir 'touched.txt'

$payload = $null
if ($Hook -or $Track) {
    try { $payload = [Console]::In.ReadToEnd() | ConvertFrom-Json } catch {}
}

if ($Track) {
    $path = if ($payload) { "$($payload.tool_input.file_path)" } else { '' }
    if ($path) {
        $full = [IO.Path]::GetFullPath($path)
        if ($full.StartsWith("$Root\", [StringComparison]::OrdinalIgnoreCase)) {
            $relative = $full.Substring($Root.Length + 1).Replace('\', '/')
            if ($relative -match $sourcePattern) {
                New-Item -ItemType Directory -Force $cacheDir | Out-Null
                Add-Content -Path $touchedFile -Value $relative
            }
        }
    }
    exit 0
}

$stopHookActive = $Hook -and $payload -and [bool]$payload.stop_hook_active

function Find-Tool([string]$name, [string[]]$candidates) {
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) { return $candidate }
    }
    $command = Get-Command $name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    return $null
}

$vsLlvm = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -prerelease -property installationPath
    if ($vs) { $vsLlvm = Join-Path $vs 'VC\Tools\Llvm\x64\bin' }
}
$llvmDirs = @("$env:ProgramFiles\LLVM\bin", $vsLlvm) | Where-Object { $_ }
$clangFormat = Find-Tool 'clang-format' ($llvmDirs | ForEach-Object { Join-Path $_ 'clang-format.exe' })
$clangTidy = Find-Tool 'clang-tidy' ($llvmDirs | ForEach-Object { Join-Path $_ 'clang-tidy.exe' })
$cppcheck = Find-Tool 'cppcheck' @("$env:ProgramFiles\Cppcheck\cppcheck.exe")

$compileDb = Join-Path $Root "$BuildDir\compile_commands.json"
$notes = New-Object System.Collections.Generic.List[string]
if (-not $clangFormat) { $notes.Add('clang-format not found: install LLVM (winget install LLVM.LLVM).') }
if (-not $clangTidy) { $notes.Add('clang-tidy not found: install LLVM (winget install LLVM.LLVM).') }
if (-not $cppcheck) { $notes.Add('cppcheck not found: winget install Cppcheck.Cppcheck.') }
if (-not (Test-Path $compileDb)) {
    $notes.Add("$BuildDir/compile_commands.json not found: configure and build first; clang-tidy and Cppcheck skipped.")
    $clangTidy = $null
    $cppcheck = $null
}

# Repo-relative, forward slashes, lower case: the build tree spells Engine as engine.
function Get-Key([string]$path) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($path)) { $path } else { Join-Path $Root $path }))
    if ($full.StartsWith($Root, [StringComparison]::OrdinalIgnoreCase)) { $full = $full.Substring($Root.Length).TrimStart('\', '/') }
    return $full.Replace('\', '/').ToLowerInvariant()
}

function Invoke-Git { git -c core.safecrlf=false @args 2>$null }

# Keys back to the paths as git spells them, for the report.
$displayPaths = @{}
foreach ($file in (Invoke-Git ls-files)) { $displayPaths[(Get-Key $file)] = $file }

# Files to check, and for each one the changed line ranges (null = the whole file).
$targets = @{}
if ($All) {
    foreach ($file in (Invoke-Git ls-files | Where-Object { $_ -match $sourcePattern })) { $targets[$file] = $null }
} else {
    $changed = @(Invoke-Git diff --name-only --diff-filter=ACMR $Base) + @(Invoke-Git ls-files --others --exclude-standard)
    if ($Hook) {
        # Only what Claude edited: the user's own uncommitted work is theirs to check.
        $touched = if (Test-Path $touchedFile) { @(Get-Content $touchedFile | ForEach-Object { Get-Key $_ }) } else { @() }
        $changed = $changed | Where-Object { $touched -contains (Get-Key $_) }
    }
    foreach ($file in ($changed | Where-Object { $_ -match $sourcePattern } | Sort-Object -Unique)) {
        if (-not (Test-Path $file)) { continue }
        $ranges = New-Object System.Collections.Generic.List[int[]]
        $tracked = Invoke-Git ls-files -- $file
        if (-not $tracked) {
            $ranges = $null
        } else {
            foreach ($line in (Invoke-Git diff -U0 $Base -- $file)) {
                if ($line -match '^@@ -\S+ \+(\d+)(?:,(\d+))? @@') {
                    $start = [int]$Matches[1]
                    $count = if ($Matches[2]) { [int]$Matches[2] } else { 1 }
                    if ($count -gt 0) { $ranges.Add(@($start, ($start + $count - 1))) }
                }
            }
        }
        if ($null -eq $ranges -or $ranges.Count -gt 0) { $targets[$file] = $ranges }
    }
}

if ($targets.Count -eq 0) {
    if ($Hook -and (Test-Path $touchedFile)) { Remove-Item $touchedFile }
    if (-not $Hook) {
        foreach ($note in $notes) { Write-Host "note: $note" }
        Write-Host 'check: no changed C++ sources.'
    }
    exit 0
}

$rangesByKey = @{}
foreach ($file in $targets.Keys) { $rangesByKey[(Get-Key $file)] = $targets[$file] }

function Test-Changed([string]$key, [int]$line) {
    if (-not $rangesByKey.ContainsKey($key)) { return $false }
    $ranges = $rangesByKey[$key]
    if ($null -eq $ranges) { return $true }
    foreach ($range in $ranges) {
        if ($line -ge $range[0] -and $line -le $range[1]) { return $true }
    }
    return $false
}

# A run that already passed with these exact contents and configs isn't repeated.
New-Item -ItemType Directory -Force $cacheDir | Out-Null
$hasher = [Security.Cryptography.SHA256]::Create()
$stamp = New-Object Text.StringBuilder
[void]$stamp.Append("all=$All base=$(Invoke-Git rev-parse $Base)`n")
$inputs = @('.clang-format', '.clang-tidy', 'tools/check.ps1', 'tools/cppcheck-suppressions.txt') +
    @(Invoke-Git ls-files '*.clang-tidy') + @($targets.Keys | Sort-Object)
foreach ($file in ($inputs | Sort-Object -Unique)) {
    if (Test-Path $file) {
        $hash = [BitConverter]::ToString($hasher.ComputeHash([IO.File]::ReadAllBytes((Join-Path $Root $file))))
        [void]$stamp.Append("$file=$hash`n")
    }
}
$runKey = [BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($stamp.ToString()))).Replace('-', '')
$passFile = Join-Path $cacheDir "pass-$runKey"
if (-not $Fix -and $notes.Count -eq 0 -and (Test-Path $passFile)) {
    if ($Hook) { Remove-Item $touchedFile -ErrorAction SilentlyContinue }
    Write-Host "check: $($targets.Count) file(s) unchanged since the last clean run."
    exit 0
}

$findings = New-Object System.Collections.Generic.List[string]
$seen = @{}
function Add-Finding([string]$tool, [string]$path, [int]$line, [int]$column, [string]$message, [bool]$always = $false) {
    $key = Get-Key $path
    if (-not $All -and -not $always -and -not (Test-Changed $key $line)) { return }
    if ($All -and $key -notmatch '^(engine|editor|runtime|tools)/') { return }
    $shown = if ($displayPaths.ContainsKey($key)) { $displayPaths[$key] } else { $key }
    $text = "${shown}:${line}:${column}: [$tool] $message"
    if (-not $seen.ContainsKey($text)) {
        $seen[$text] = $true
        $findings.Add($text)
    }
}

$diagnostic = '^(?<path>.+?):(?<line>\d+):(?<col>\d+): (?<kind>warning|error): (?<msg>.+)$'

# clang-format: whole files, the code base is kept formatted.
if ($clangFormat) {
    foreach ($file in $targets.Keys) {
        if ($Fix) { & $clangFormat -i $file }
        $output = & $clangFormat --dry-run $file 2>&1 | ForEach-Object { "$_" }
        $reported = @{}
        foreach ($line in $output) {
            if ($line -match $diagnostic -and -not $reported.ContainsKey($file)) {
                $reported[$file] = $true
                Add-Finding 'clang-format' $Matches.path ([int]$Matches.line) ([int]$Matches.col) `
                    "not formatted (first difference here); run tools/check.ps1 -Fix" $true
            }
        }
    }
}

# Runs a tool once per argument list, $Jobs at a time; returns all output lines.
function Invoke-Parallel([string]$exe, [object[]]$argLists) {
    $tmp = Join-Path $cacheDir 'out'
    New-Item -ItemType Directory -Force $tmp | Out-Null
    $running = New-Object System.Collections.Generic.List[object]
    $outputs = New-Object System.Collections.Generic.List[string]
    $index = 0
    foreach ($argList in $argLists) {
        while ($running.Count -ge $Jobs) {
            $done = $running | Where-Object { $_.Process.HasExited }
            if (-not $done) { Start-Sleep -Milliseconds 100; continue }
            foreach ($job in @($done)) { [void]$running.Remove($job) }
        }
        $index++
        $out = Join-Path $tmp "$index.out"
        $err = Join-Path $tmp "$index.err"
        $quoted = $argList | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }
        $process = Start-Process -FilePath $exe -ArgumentList $quoted -NoNewWindow -PassThru `
            -RedirectStandardOutput $out -RedirectStandardError $err
        $running.Add([pscustomobject]@{ Process = $process })
        $outputs.Add($out)
        $outputs.Add($err)
    }
    foreach ($job in $running) { $job.Process.WaitForExit() }
    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($file in $outputs) {
        if (Test-Path $file) { foreach ($line in (Get-Content $file)) { $lines.Add($line) } }
    }
    Remove-Item -Recurse -Force $tmp
    return $lines
}

# clang-tidy: each changed file through its own compile command (headers get one inferred from a neighbour).
if ($clangTidy) {
    $tidyFiles = if ($All) {
        (Get-Content $compileDb -Raw | ConvertFrom-Json) | ForEach-Object { $_.file } |
            Where-Object { (Get-Key $_) -match '^(engine|editor|runtime|tools)/' } | Sort-Object -Unique
    } else {
        $targets.Keys
    }
    $argLists = @($tidyFiles | ForEach-Object { , @('-p', $BuildDir, '--quiet', $_) })
    foreach ($line in (Invoke-Parallel $clangTidy $argLists)) {
        if ($line -notmatch $diagnostic) { continue }
        # A compile error hides every other finding in the file, so it's reported wherever it is.
        $isError = $Matches.kind -eq 'error'
        Add-Finding 'clang-tidy' $Matches.path ([int]$Matches.line) ([int]$Matches.col) $Matches.msg $isError
    }
}

# Cppcheck: the whole project (its build dir keeps it fast), reported on the changed lines.
if ($cppcheck) {
    $cppcheckDir = Join-Path $cacheDir 'cppcheck'
    New-Item -ItemType Directory -Force $cppcheckDir | Out-Null
    $output = & $cppcheck "--project=$compileDb" --enable=warning,performance,portability --inline-suppr `
        "--suppressions-list=$Root\tools\cppcheck-suppressions.txt" --suppress=missingIncludeSystem `
        --library=windows --std=c++17 "--cppcheck-build-dir=$cppcheckDir" -j $Jobs --quiet `
        '--template={file}:{line}:{column}: warning: {message} [{id}]' 2>&1 | ForEach-Object { "$_" }
    foreach ($line in $output) {
        if ($line -match $diagnostic) {
            Add-Finding 'cppcheck' $Matches.path ([int]$Matches.line) ([int]$Matches.col) $Matches.msg
        }
    }
}

foreach ($note in $notes) { Write-Host "note: $note" }
$scope = if ($All) { 'all sources' } else { "$($targets.Count) changed file(s) against $Base" }
if ($findings.Count -eq 0) {
    if ($notes.Count -eq 0) { New-Item -ItemType File -Force $passFile | Out-Null }
    if ($Hook) { Remove-Item $touchedFile -ErrorAction SilentlyContinue }
    Write-Host "check: clean ($scope)."
    exit 0
}

$report = @("check: $($findings.Count) problem(s) in $scope.") + ($findings | Sort-Object)
if ($Hook) {
    $report += 'Fix these before finishing: format with tools/check.ps1 -Fix; for a false positive, suppress it ' +
        'with a NOLINT/cppcheck-suppress comment that says why.'
    [Console]::Error.WriteLine(($report -join "`n"))
    # Once Claude has already gone back to fix, report without blocking again.
    if ($stopHookActive) { exit 1 }
    exit 2
}
$report | ForEach-Object { Write-Host $_ }
exit 1
