param(
    [string]$CompilerPath = ".\mote_test.exe",
    [string]$ManifestPath = "test\harness\manifest.json",
    [string]$ArtifactsDir = "test\artifacts\harness",
    [switch]$Build,
    [string]$Filter
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Host "FAIL: $Message" -ForegroundColor Red
    exit 1
}

function Ensure-Compiler([string]$Path, [bool]$ShouldBuild) {
    if($ShouldBuild -or -not (Test-Path -LiteralPath $Path)) {
        Write-Host "Building compiler -> $Path"
        & gcc src\main.c -o $Path
        if($LASTEXITCODE -ne 0) {
            Fail "compiler build failed"
        }
    }
}

function Invoke-Compiler([string]$Compiler, [string[]]$CompilerArgs) {
    $stdout = New-TemporaryFile
    $stderr = New-TemporaryFile
    try {
        $process = Start-Process -FilePath $Compiler `
                                 -ArgumentList $CompilerArgs `
                                 -NoNewWindow `
                                 -Wait `
                                 -PassThru `
                                 -RedirectStandardOutput $stdout.FullName `
                                 -RedirectStandardError $stderr.FullName
        $combined = ""
        if(Test-Path -LiteralPath $stdout.FullName) {
            $combined += [System.IO.File]::ReadAllText($stdout.FullName)
        }
        if(Test-Path -LiteralPath $stderr.FullName) {
            $combined += [System.IO.File]::ReadAllText($stderr.FullName)
        }

        return @{
            ExitCode = $process.ExitCode
            Output = $combined
        }
    }
    finally {
        Remove-Item -LiteralPath $stdout.FullName, $stderr.FullName -Force -ErrorAction SilentlyContinue
    }
}

function New-GeneratedTestInput($Case, [string]$ArtifactsRoot) {
    if(-not $Case.generated) {
        return [string]$Case.input
    }

    $generatedDir = Join-Path $ArtifactsRoot "generated"
    New-Item -ItemType Directory -Force -Path $generatedDir | Out-Null
    $path = Join-Path $generatedDir ($Case.name + ".mote")

    switch([string]$Case.generated.kind) {
        "scope_variable_overflow" {
            $count = [int]$Case.generated.count
            $lines = @("Host = struct {")
            $lines += "    boom: fn(self: Self"
            for($i = 0; $i -lt $count; $i++) {
                $lines[-1] += ", p$($i): i32"
            }
            $lines[-1] += ") void {"
            $lines += "    },"
            $lines += "};"
            [System.IO.File]::WriteAllText($path, ($lines -join [Environment]::NewLine))
            return $path
        }
        "scope_type_overflow" {
            $count = [int]$Case.generated.count
            $lines = @()
            for($i = 0; $i -lt $count; $i++) {
                $lines += "T$($i) = struct {"
                $lines += "    value: i32,"
                $lines += "};"
                $lines += ""
            }
            [System.IO.File]::WriteAllText($path, ($lines -join [Environment]::NewLine))
            return $path
        }
        default {
            Fail "unknown generated test kind: $($Case.generated.kind)"
        }
    }
}

if(-not (Test-Path -LiteralPath $ManifestPath)) {
    Fail "manifest not found: $ManifestPath"
}

Ensure-Compiler $CompilerPath $Build.IsPresent

New-Item -ItemType Directory -Force -Path $ArtifactsDir | Out-Null

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$cases = @($manifest.tests)
if($Filter) {
    $cases = @($cases | Where-Object { $_.name -like "*$Filter*" })
}

if($cases.Count -eq 0) {
    Fail "no test cases matched"
}

$passed = 0
$failed = 0

foreach($case in $cases) {
    $inputPath = New-GeneratedTestInput $case $ArtifactsDir
    $compilerArgs = @()
    if($case.mode -eq "llvm") {
        $outputName = [System.IO.Path]::GetFileNameWithoutExtension($inputPath) + ".ll"
        $outputPath = Join-Path $ArtifactsDir $outputName
        $compilerArgs += "-S"
        $compilerArgs += $inputPath
        $compilerArgs += "-o"
        $compilerArgs += $outputPath
    }
    else {
        $compilerArgs += $inputPath
    }

    if($case.args) {
        foreach($arg in $case.args) {
            $compilerArgs += [string]$arg
        }
    }

    $result = Invoke-Compiler $CompilerPath $compilerArgs
    $ok = $true

    if([string]$case.expect -eq "success") {
        if($result.ExitCode -ne 0) {
            $ok = $false
        }
        elseif($case.mode -eq "llvm") {
            $outputName = [System.IO.Path]::GetFileNameWithoutExtension($inputPath) + ".ll"
            $outputPath = Join-Path $ArtifactsDir $outputName
            if(-not (Test-Path -LiteralPath $outputPath)) {
                $ok = $false
            }
        }
    }
    elseif([string]$case.expect -eq "failure") {
        if($result.ExitCode -eq 0) {
            $ok = $false
        }
        foreach($needle in @($case.contains)) {
            if(-not $result.Output.Contains([string]$needle)) {
                $ok = $false
                break
            }
        }
    }
    else {
        Fail "unknown expectation in manifest for $($case.name)"
    }

    if($ok) {
        $passed++
        Write-Host "PASS [$($case.name)]"
    }
    else {
        $failed++
        Write-Host "FAIL [$($case.name)]" -ForegroundColor Red
        Write-Host "  args: $($compilerArgs -join ' ')"
        Write-Host "  exit: $($result.ExitCode)"
        if($result.Output) {
            Write-Host "  output:"
            Write-Host $result.Output.TrimEnd()
        }
    }
}

Write-Host ""
Write-Host "Summary: $passed passed, $failed failed"

if($failed -ne 0) {
    exit 1
}
