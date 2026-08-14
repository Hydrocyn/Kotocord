<#
.SYNOPSIS
    拆解资源包 zip, 将二进制资源安放到正确路径

.DESCRIPTION
    将 pack-resources.ps1 产出的 kotocord-resources-<日期>.zip 解压到仓库根
    (zip 内路径镜像仓库结构), 然后:
      1. 兜底: vosk.lib 缺失但 libvosk.dll 存在 → 自动生成导入库
      2. 复用 check-deps.ps1 验证资源就位

    说明: 不使用 git 操作。

.PARAMETER ZipPath
    资源包路径。省略时自动选择 dist/ 下最新的 kotocord-resources-*.zip

.EXAMPLE
    .\unpack-resources.ps1

.EXAMPLE
    .\unpack-resources.ps1 D:\backup\kotocord-resources-2026-08-14.zip
#>

param(
    [string]$ZipPath = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

Write-Host "=== Unpack Kotocord Resources ===" -ForegroundColor Cyan

# ==========================================
# 1. 确定资源包
# ==========================================
if (-not $ZipPath) {
    $distDir = Join-Path $ProjectRoot "dist"
    $zips = Get-ChildItem $distDir -Filter "kotocord-resources-*.zip" -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
    if (-not $zips -or $zips.Count -eq 0) {
        Write-Host "[ERR] dist/ 下没有 kotocord-resources-*.zip" -ForegroundColor Red
        Write-Host "用法: .\unpack-resources.ps1 <资源包路径>" -ForegroundColor Yellow
        exit 1
    }
    $ZipPath = $zips[0].FullName
    Write-Host "[..] 自动选择最新资源包: $($zips[0].Name)"
}

if (-not (Test-Path $ZipPath)) {
    Write-Host "[ERR] 文件不存在: $ZipPath" -ForegroundColor Red
    exit 1
}

# ==========================================
# 2. 解压到仓库根 (zip 内路径镜像仓库结构)
# ==========================================
$tarExe = Get-Command tar.exe -ErrorAction SilentlyContinue
if (-not $tarExe) {
    Write-Host "[ERR] 未找到 tar.exe (Windows 10 1803+ 自带)" -ForegroundColor Red
    exit 1
}

Write-Host "[..] 解压到: $ProjectRoot"
& $tarExe.Source -xf $ZipPath -C $ProjectRoot
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERR] 解压失败 (exit $LASTEXITCODE)" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] 解压完成"

# ==========================================
# 3. 兜底: vosk.lib 缺失 → 自动生成导入库
# ==========================================
$voskLib = Join-Path $ProjectRoot "third_party\vosk\lib\vosk.lib"
$voskDll = Join-Path $ProjectRoot "third_party\vosk\lib\libvosk.dll"
if (-not (Test-Path $voskLib) -and (Test-Path $voskDll)) {
    Write-Host "[..] vosk.lib 缺失, 自动生成 MSVC 导入库 ..."
    & (Join-Path $ProjectRoot "generate-vosk-import-lib.ps1")
}

# ==========================================
# 4. 复用 check-deps.ps1 验证
# ==========================================
Write-Host "[..] 资源就位验证:"
& (Join-Path $ProjectRoot "check-deps.ps1")
