param(
    [string]$BuildDir = "build-windows",
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [switch]$Install
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,

        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

$repo = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repo

Invoke-Checked "cmake" @("-S", ".", "-B", $BuildDir, "-G", $Generator, "-A", "x64", "-DCMAKE_BUILD_TYPE=$Config")
Invoke-Checked "cmake" @("--build", $BuildDir, "--config", $Config, "--target", "eq_dsp_tests")
Invoke-Checked "ctest" @("--test-dir", $BuildDir, "-C", $Config, "--output-on-failure")
Invoke-Checked "cmake" @("--build", $BuildDir, "--config", $Config, "--target", "AIAgentParametricEQ_VST3")

$vst3 = Get-ChildItem -Path $BuildDir -Filter "AI Agent Parametric EQ.vst3" -Recurse | Select-Object -First 1
if (-not $vst3) {
    throw "VST3 bundle was not found under $BuildDir"
}

Write-Host "Built VST3:" $vst3.FullName

if ($Install) {
    $targetDir = "C:\Program Files\Common Files\VST3"
    $target = Join-Path $targetDir $vst3.Name

    if (-not (Test-Path $targetDir)) {
        New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
    }

    if (Test-Path $target) {
        Remove-Item -Recurse -Force $target
    }

    Copy-Item -Recurse -Force $vst3.FullName $target
    Write-Host "Installed VST3:" $target
    Write-Host "Open FL Studio Plugin Manager and run Find installed plugins."
}
