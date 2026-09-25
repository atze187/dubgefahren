param(
    [Parameter(Position = 0)]
    [ValidateSet('configure', 'build', 'test', 'install', 'validate', 'all')]
    [string]$Command = 'all',
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$buildDir = Join-Path $root 'build'

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $c = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path $c) { return $c }
    }
    throw 'cmake nicht gefunden (weder im PATH noch in Visual Studio).'
}

$cmake = Find-CMake
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'

function Invoke-Checked([string]$what, [scriptblock]$block) {
    & $block
    if ($LASTEXITCODE -ne 0) { throw "$what fehlgeschlagen (Exit $LASTEXITCODE)" }
}

function Do-Configure { Invoke-Checked 'configure' { & $cmake -S $root -B $buildDir -A x64 } }
function Do-Build {
    if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) { Do-Configure }
    Invoke-Checked 'build' { & $cmake --build $buildDir --config $Config --parallel }
}
function Do-Test { Invoke-Checked 'test' { & $ctest --test-dir $buildDir -C $Config --output-on-failure } }
function Do-Install { Invoke-Checked 'install' { & $cmake --install $buildDir --config $Config } }

function Do-Validate {
    $bundle = Join-Path $buildDir "plugin\Dubgefahren_artefacts\$Config\VST3\Dubgefahren.vst3"
    if (-not (Test-Path $bundle)) { Do-Build }
    $vDir = Join-Path $buildDir 'vst3validator'
    Invoke-Checked 'validator configure' { & $cmake -S (Join-Path $root 'tools\vst3validator') -B $vDir -A x64 }
    Invoke-Checked 'validator build' { & $cmake --build $vDir --config Release --target validator --parallel }
    $validator = Get-ChildItem -Path $vDir -Recurse -Filter 'validator.exe' | Select-Object -First 1
    if (-not $validator) { throw 'validator.exe nicht gefunden' }
    Invoke-Checked 'VST3 validator' { & $validator.FullName $bundle }

    $pluginval = Join-Path $root 'tools\bin\pluginval.exe'
    if (Test-Path $pluginval) {
        Invoke-Checked 'pluginval' { & $pluginval --strictness-level 5 --validate-in-process --validate $bundle }
    } else {
        Write-Host "pluginval nicht gefunden ($pluginval) – übersprungen. Siehe README."
    }
}

switch ($Command) {
    'configure' { Do-Configure }
    'build'     { Do-Build }
    'test'      { Do-Build; Do-Test }
    'install'   { Do-Build; Do-Install }
    'validate'  { Do-Validate }
    'all'       { Do-Configure; Do-Build; Do-Test }
}
