$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$tcc = Join-Path $root 'tools\tcc\tcc\tcc.exe'
$src = Join-Path $root 'src'
$build = Join-Path $root 'build'
$dist = Join-Path $root 'dist'
$assets = Join-Path $root 'assets'
$demo = Join-Path $root 'demo'


if (!(Test-Path $tcc)) { throw "TinyCC not found: $tcc" }
New-Item -ItemType Directory -Force -Path $build, $dist, $assets, $demo | Out-Null

Write-Host '== compile core =='
$objs = @()
foreach ($name in @('params', 'dsp', 'plugin', 'editor')) {
    $obj = Join-Path $build "$name.o"
    & $tcc -m64 -c -O2 (Join-Path $src "$name.c") -o $obj
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $name" }
    $objs += $obj
}

Write-Host '== link VST2 DLL =='
& $tcc -m64 -shared -O2 @objs -lgdi32 -luser32 -lkernel32 -lmsimg32 -o (Join-Path $dist 'KickForge.dll')
if ($LASTEXITCODE -ne 0) { throw 'link failed' }

Write-Host '== build validation + preview + demo tools =='
& $tcc -m64 -O2 (Join-Path $src 'vst_test.c') -luser32 -lkernel32 -o (Join-Path $build 'vst_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'vst_test compile failed' }
& $tcc -m64 -O2 (Join-Path $src 'preview.c') (Join-Path $src 'params.c') (Join-Path $src 'dsp.c') (Join-Path $src 'plugin.c') (Join-Path $src 'editor.c') -lgdi32 -luser32 -lkernel32 -lmsimg32 -o (Join-Path $build 'preview.exe')
if ($LASTEXITCODE -ne 0) { throw 'preview compile failed' }
& $tcc -m64 -O2 (Join-Path $src 'demo.c') (Join-Path $src 'params.c') (Join-Path $src 'dsp.c') (Join-Path $src 'plugin.c') (Join-Path $src 'editor.c') -lgdi32 -luser32 -lkernel32 -lmsimg32 -o (Join-Path $build 'demo.exe')
if ($LASTEXITCODE -ne 0) { throw 'demo compile failed' }
& $tcc -m64 -O2 (Join-Path $src 'stress_test.c') (Join-Path $src 'params.c') (Join-Path $src 'dsp.c') (Join-Path $src 'plugin.c') (Join-Path $src 'editor.c') -lgdi32 -luser32 -lkernel32 -lmsimg32 -o (Join-Path $build 'stress_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'stress_test compile failed' }
& $tcc -m64 -O2 (Join-Path $src 'ui_capture.c') -lgdi32 -luser32 -lkernel32 -o (Join-Path $build 'ui_capture.exe')
if ($LASTEXITCODE -ne 0) { throw 'ui_capture compile failed' }
& $tcc -m64 -O2 (Join-Path $src 'unload_test.c') -luser32 -lkernel32 -o (Join-Path $build 'unload_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'unload_test compile failed' }
& $tcc -m64 -O2 (Join-Path $src 'settings_test.c') (Join-Path $src 'params.c') (Join-Path $src 'dsp.c') (Join-Path $src 'plugin.c') (Join-Path $src 'editor.c') -lgdi32 -luser32 -lkernel32 -lmsimg32 -o (Join-Path $build 'settings_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'settings_test compile failed' }

$parent = $root
Push-Location $parent
try {
    $env:KF_SETTINGS_FILE = (Join-Path $build 'test_settings.ini')
    Remove-Item $env:KF_SETTINGS_FILE -Force -ErrorAction SilentlyContinue

    Write-Host '== host validation =='
    & (Join-Path $build 'vst_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'VST host validation failed' }

    Write-Host '== lifecycle / scale stress =='
    & (Join-Path $build 'stress_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'UI lifecycle stress test failed' }

    Write-Host '== unload / reload lifecycle =='
    & (Join-Path $build 'unload_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'DLL unload lifecycle test failed' }

    Write-Host '== UI settings persistence =='
    & (Join-Path $build 'settings_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'UI settings persistence test failed' }

    Write-Host '== render UI preview =='
    $env:KF_SETTINGS_FILE = (Join-Path $build 'preview_settings.ini')
    Remove-Item $env:KF_SETTINGS_FILE -Force -ErrorAction SilentlyContinue
    & (Join-Path $build 'preview.exe') (Join-Path $assets 'ui_preview.bmp')
    $previewRc = $LASTEXITCODE
    Remove-Item Env:KF_SETTINGS_FILE -ErrorAction SilentlyContinue
    if ($previewRc -ne 0) { throw 'UI preview failed' }
    Add-Type -AssemblyName System.Drawing
    $bmpPath = Join-Path $assets 'ui_preview.bmp'
    $pngPath = Join-Path $assets 'ui_preview.png'
    $img = [System.Drawing.Image]::FromFile($bmpPath)
    $img.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()

    Write-Host '== render demo kicks =='
    & (Join-Path $build 'demo.exe')
    if ($LASTEXITCODE -ne 0) { throw 'demo render failed' }
}
finally {
    Pop-Location
}

Write-Host ''
Write-Host 'BUILD OK'
Get-Item (Join-Path $dist 'KickForge.dll'), (Join-Path $assets 'ui_preview.png') | Select-Object FullName, Length | Format-Table -AutoSize
Get-ChildItem $demo -Filter *.wav | Select-Object Name, Length | Format-Table -AutoSize
