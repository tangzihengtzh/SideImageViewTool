param([switch]$Test)
$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    & cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败' }
    & cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) { throw '编译失败' }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination (Join-Path $PSScriptRoot 'dist/README.md') -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'CHANGELOG.md') -Destination (Join-Path $PSScriptRoot 'dist/CHANGELOG.md') -Force
    if ($Test) {
        & ctest --test-dir build -C Release --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw '自动验证失败' }
    }
    $packageFiles = @('dist/ScreenDock.exe', 'dist/README.md', 'dist/CHANGELOG.md') | ForEach-Object { Join-Path $PSScriptRoot $_ }
    $packagePath = Join-Path $PSScriptRoot 'ScreenDock-portable.zip'
    Compress-Archive -LiteralPath $packageFiles -DestinationPath $packagePath -CompressionLevel Optimal -Force
    Write-Host ('便携包：' + $packagePath)
    Write-Host ('运行程序：' + (Join-Path $PSScriptRoot 'dist/ScreenDock.exe'))
} finally { Pop-Location }
