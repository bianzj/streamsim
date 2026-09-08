param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$SourceDirectory = Join-Path $ProjectRoot 'models\histream'
$BuildDirectory = Join-Path $ProjectRoot 'build\histream'
$FallbackVcpkgRoot = Join-Path (Split-Path $ProjectRoot -Parent) 'vcpkg'

if (-not $VcpkgRoot -and (Test-Path -LiteralPath $FallbackVcpkgRoot)) {
    $VcpkgRoot = $FallbackVcpkgRoot
}

$CmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $CmakeCommand) {
    $VisualStudioRoot = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio'
    $CmakeExecutable = Get-ChildItem -LiteralPath $VisualStudioRoot -Filter cmake.exe -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like '*CommonExtensions*Microsoft*CMake*' } |
        Select-Object -First 1 -ExpandProperty FullName
    if (-not $CmakeExecutable) { throw 'cmake.exe was not found.' }
} else {
    $CmakeExecutable = $CmakeCommand.Source
}

$ConfigureArguments = @('-S', $SourceDirectory, '-B', $BuildDirectory)
if ($VcpkgRoot) {
    $ToolchainFile = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
    if (-not (Test-Path -LiteralPath $ToolchainFile)) { throw "vcpkg toolchain was not found: $ToolchainFile" }
    $ConfigureArguments += "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"
}

& $CmakeExecutable @ConfigureArguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $CmakeExecutable --build $BuildDirectory --config $Configuration --parallel
exit $LASTEXITCODE
