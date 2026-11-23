[CmdletBinding()]
param(
    [string]$VcVars64 = ''
)

$ErrorActionPreference = 'Stop'
$component = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$buildRoot = Join-Path $PSScriptRoot 'build'

if ($VcVars64) {
    $VcVars64 = (Resolve-Path $VcVars64).Path
} elseif (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        foreach ($version in @('18', '2022', '2019')) {
            foreach ($edition in @('Community', 'BuildTools', 'Professional', 'Enterprise')) {
                $candidate = Join-Path $base "Microsoft Visual Studio/$version/$edition/VC/Auxiliary/Build/vcvars64.bat"
                if (Test-Path -LiteralPath $candidate) {
                    $VcVars64 = (Resolve-Path $candidate).Path
                    break
                }
            }
            if ($VcVars64) { break }
        }
        if ($VcVars64) { break }
    }
    if (-not $VcVars64) {
        throw 'MSVC not found. Run in a developer shell or pass -VcVars64 <path-to-vcvars64.bat>.'
    }
}

foreach ($mode in @('legacy', 'string-aware')) {
    $modeBuild = Join-Path $buildRoot $mode
    New-Item -ItemType Directory -Force -Path $modeBuild | Out-Null
    $batchPath = Join-Path $modeBuild 'compile.cmd'
    $define = if ($mode -eq 'string-aware') { '/DCONFIG_DATAFORMAT_JSON_STRING_AWARE=1' } else { '' }
    $initialize = if ($VcVars64) { "call `"$VcVars64`"`r`nif errorlevel 1 exit /b %errorlevel%" } else { '' }
    $batch = @"
@echo off
$initialize
cl.exe /nologo /std:c++17 /EHsc /W4 /Od $define /I"$PSScriptRoot/stubs" /I"$component/include" "$component/CJsonReadStream.cpp" "$PSScriptRoot/json_stream_tests.cpp" /Fe:json_stream_tests.exe
exit /b %errorlevel%
"@
    [System.IO.File]::WriteAllText($batchPath, $batch.Replace("`r`n", "`n").Replace("`n", "`r`n"), [System.Text.Encoding]::ASCII)
    Push-Location $modeBuild
    try {
        Write-Host "=== Build $mode ==="
        & $env:ComSpec /d /c "`"$batchPath`""
        if ($LASTEXITCODE -ne 0) { throw "MSVC failed for $mode (exit $LASTEXITCODE)." }
        Write-Host "=== Run $mode ==="
        & (Join-Path $modeBuild 'json_stream_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw "Tests failed for $mode (exit $LASTEXITCODE)." }
    } finally {
        Pop-Location
    }
}

Write-Host 'Both JSON stream modes passed. Build outputs are isolated under components/dataformat/test/host/build/.'
