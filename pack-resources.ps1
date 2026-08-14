<#
.SYNOPSIS
    打包 Kotocord 未跟踪的二进制资源为迁移用 zip

.DESCRIPTION
    将 git 未跟踪的二进制资源 (Vosk DLL/导入库 + 本地模型) 打包为:
      dist/kotocord-resources-<日期>.zip

    zip 内路径镜像仓库结构, 拆解时直接解压到仓库根即可。
    配套脚本: unpack-resources.ps1 (拆解安放), check-deps.ps1 (验证)

    说明: 不使用 git 操作。dist/ 已被 .gitignore 排除。

.EXAMPLE
    .\pack-resources.ps1
#>

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

# ==========================================
# 资源清单 — 需要打包的路径 (相对仓库根)
# zip 内保留此镜像路径
# ==========================================
$Manifest = @(
    # Vosk 运行时 (预编译包 vosk-win64-*.zip 解出 + 导入库)
    "third_party/vosk/lib/libvosk.dll",
    "third_party/vosk/lib/libgcc_s_seh-1.dll",
    "third_party/vosk/lib/libstdc++-6.dll",
    "third_party/vosk/lib/libwinpthread-1.dll",
    "third_party/vosk/lib/vosk.lib",
    # 本地模型 (体积大, 绝不进 git)
    "resources/model"
)

# ==========================================
# 1. 清单就位检查
# ==========================================
Write-Host "=== Pack Kotocord Resources ===" -ForegroundColor Cyan

$missing = @()
foreach ($item in $Manifest) {
    if (-not (Test-Path (Join-Path $ProjectRoot $item))) {
        $missing += $item
    }
}
if ($missing.Count -gt 0) {
    Write-Host "[ERR] 以下资源缺失, 无法打包:" -ForegroundColor Red
    $missing | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
    exit 1
}
Write-Host "[OK] 清单全部就位 ($($Manifest.Count) 项)"

# ==========================================
# 2. 打包 (tar.exe — Windows 10 1803+ 自带, 大文件比 Compress-Archive 快)
# ==========================================
$tarExe = Get-Command tar.exe -ErrorAction SilentlyContinue
if (-not $tarExe) {
    Write-Host "[ERR] 未找到 tar.exe (Windows 10 1803+ 自带)" -ForegroundColor Red
    exit 1
}

$zipName = "kotocord-resources-" + (Get-Date -Format "yyyy-MM-dd") + ".zip"
$distDir = Join-Path $ProjectRoot "dist"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
$zipPath = Join-Path $distDir $zipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

Write-Host "[..] 打包中 ... (模型体积大, 耐心等待)"
& $tarExe.Source -a -cf $zipPath -C $ProjectRoot $Manifest
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERR] tar 打包失败 (exit $LASTEXITCODE)" -ForegroundColor Red
    exit 1
}

# ==========================================
# 3. 汇总
# ==========================================
$sizeMB = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host ""
Write-Host "=== 打包完成 ===" -ForegroundColor Green
Write-Host "产物: $zipPath"
Write-Host "体积: $sizeMB MB"
Write-Host ""
Write-Host "内含:"
$Manifest | ForEach-Object { Write-Host "  - $_" }
Write-Host ""
Write-Host "迁移到新机器后: .\unpack-resources.ps1 dist\$zipName"
