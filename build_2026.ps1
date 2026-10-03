$ErrorActionPreference = "Stop"

$msbuild = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $found = & $vswhere -all -products * -find MSBuild\**\Bin\MSBuild.exe
        if ($found -is [array]) { $found = $found[0] }
        if ($found) { $msbuild = $found }
    }
}

if (-not (Test-Path $msbuild)) {
    Write-Host "MSBuild not found!"
    exit 1
}

Write-Host "Found MSBuild at: $msbuild"
Write-Host "Building C4D_QuadDraw (Release x64)..."

$vcxproj = "$PSScriptRoot\sdk_2026\build\C4D_QuadDraw\project\C4D_QuadDraw.vcxproj"
& $msbuild $vcxproj /p:Configuration=Release

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$xdl64Path = "$PSScriptRoot\sdk_2026\build\bin\Release\plugins\C4D_QuadDraw\C4D_QuadDraw.xdl64"
if (Test-Path $xdl64Path) {
    Write-Host "Plugin build successful: $xdl64Path"
    $pdbPath = "$PSScriptRoot\sdk_2026\build\bin\Release\plugins\C4D_QuadDraw\C4D_QuadDraw.pdb"
    if (Test-Path $pdbPath) {
        Write-Host "Removing unnecessary .pdb file..."
        Remove-Item -Path $pdbPath -Force
    }
}

Write-Host "Build complete successfully!"
