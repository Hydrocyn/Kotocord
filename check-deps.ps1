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
$hasWhisperDll = Test-Path "third_party\whisper\lib\whisper.dll"
$hasWhisperLib = Test-Path "third_party\whisper\lib\whisper.lib"
$hasGgmlDll    = Test-Path "third_party\whisper\ggml\lib\ggml.dll"
$total++; if ($hasWhisperDll) { $ok++; Write-Host "  [OK] whisper.dll" -ForegroundColor Green } else { $miss++; Write-Host "  [--] whisper.dll" -ForegroundColor Yellow }
$total++; if ($hasWhisperLib) { $ok++; Write-Host "  [OK] whisper.lib" -ForegroundColor Green } else { $miss++; Write-Host "  [--] whisper.lib" -ForegroundColor Yellow }
$total++; if ($hasGgmlDll)    { $ok++; Write-Host "  [OK] ggml.dll (third_party/whisper/ggml/lib/)" -ForegroundColor Green } else { $miss++; Write-Host "  [--] ggml.dll (third_party/whisper/ggml/lib/)" -ForegroundColor Yellow }
Write-Host "  (vcpkg mode provides whisper/ggml automatically)" -ForegroundColor DarkGray

Write-Host "-- Models --" -ForegroundColor White
$hasVoskModel    = Test-Path "resources\model\vosk-model-small-cn-0.22\am\final.mdl"
$hasWhisperModel = Test-Path "resources\model\ggml-small.bin"
$total++; if ($hasVoskModel)    { $ok++; Write-Host "  [OK] Vosk model (vosk-model-small-cn-0.22/)" -ForegroundColor Green } else { $miss++; Write-Host "  [--] Vosk model (vosk-model-small-cn-0.22/)" -ForegroundColor Yellow }
$total++; if ($hasWhisperModel) { $ok++; Write-Host "  [OK] Whisper model (ggml-small.bin)" -ForegroundColor Green } else { $miss++; Write-Host "  [--] Whisper model (ggml-small.bin)" -ForegroundColor Yellow }

Write-Host "-- Environment --" -ForegroundColor White
$envVcpkg = [Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
$envQt6   = [Environment]::GetEnvironmentVariable("Qt6_DIR", "User")
$total++; if ($envVcpkg) { $ok++; Write-Host "  [OK] VCPKG_ROOT (user env var)" -ForegroundColor Green } else { $miss++; Write-Host "  [--] VCPKG_ROOT (user env var) — setx VCPKG_ROOT ..." -ForegroundColor Yellow }
$total++; if ($envQt6)   { $ok++; Write-Host "  [OK] Qt6_DIR (user env var)" -ForegroundColor Green } else { $miss++; Write-Host "  [--] Qt6_DIR (user env var) — setx Qt6_DIR ..." -ForegroundColor Yellow }

Write-Host ""
if ($miss -eq 0) {
    Write-Host "=== $ok / $total ready ===" -ForegroundColor Green
    Write-Host "All resources ready. You can build now."
} else {
    Write-Host "=== $ok / $total ready ($miss missing) ===" -ForegroundColor Yellow
    Write-Host "See BUILD.md for download instructions."
}
