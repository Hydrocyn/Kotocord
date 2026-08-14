<#
.SYNOPSIS
    Kotocord 依赖检查 — 扫描项目资源就绪状态

.EXAMPLE
    .\check-deps.ps1
#>

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

Write-Host "=== Kotocord Resource Check ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectRoot"
Write-Host ""

$total = 0; $ok = 0; $miss = 0

Write-Host "-- Vosk --" -ForegroundColor White
$hasDll = Test-Path "third_party\vosk\lib\libvosk.dll"
$hasLib = Test-Path "third_party\vosk\lib\vosk.lib"
$total++; if ($hasDll) { $ok++; Write-Host "  [OK] libvosk.dll" -ForegroundColor Green } else { $miss++; Write-Host "  [--] libvosk.dll" -ForegroundColor Yellow }
$total++; if ($hasLib) { $ok++; Write-Host "  [OK] vosk.lib (MSVC import lib)" -ForegroundColor Green } else { $miss++; Write-Host "  [--] vosk.lib (MSVC import lib)" -ForegroundColor Yellow }

Write-Host "-- whisper.cpp --" -ForegroundColor White
Write-Host "  [OK] FetchContent 自动拉取 (configure 阶段, 需网络)" -ForegroundColor Green
Write-Host "  (无需手动放置 third_party/whisper — 2026-08-12 起)" -ForegroundColor DarkGray

Write-Host "-- Models --" -ForegroundColor White
$hasVoskModel    = Test-Path "resources\model\vosk-model-small-cn-0.22\am\final.mdl"
$hasWhisperModel = Test-Path "resources\model\ggml-small.bin"
$total++; if ($hasVoskModel)    { $ok++; Write-Host "  [OK] Vosk model (vosk-model-small-cn-0.22/)" -ForegroundColor Green } else { $miss++; Write-Host "  [--] Vosk model (vosk-model-small-cn-0.22/)" -ForegroundColor Yellow }
$total++; if ($hasWhisperModel) { $ok++; Write-Host "  [OK] Whisper model (ggml-small.bin)" -ForegroundColor Green } else { $miss++; Write-Host "  [--] Whisper model (ggml-small.bin)" -ForegroundColor Yellow }

Write-Host "-- Environment --" -ForegroundColor White
$hasUserPreset = Test-Path "CMakeUserPresets.json"
$envQt6        = [Environment]::GetEnvironmentVariable("Qt6_DIR", "User")
$total++
if ($hasUserPreset) {
    $ok++; Write-Host "  [OK] CMakeUserPresets.json (本机 Qt 路径)" -ForegroundColor Green
} elseif ($envQt6) {
    $ok++; Write-Host "  [OK] Qt6_DIR (user env var): $envQt6" -ForegroundColor Green
} else {
    $miss++; Write-Host "  [--] Qt 定位方式 — CMakeUserPresets.json 或 Qt6_DIR 环境变量需其一" -ForegroundColor Yellow
}

Write-Host ""
if ($miss -eq 0) {
    Write-Host "=== $ok / $total ready ===" -ForegroundColor Green
    Write-Host "All resources ready. You can build now."
} else {
    Write-Host "=== $ok / $total ready ($miss missing) ===" -ForegroundColor Yellow
    Write-Host "See BUILD.md for download instructions."
}
