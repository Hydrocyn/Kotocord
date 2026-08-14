<#
.SYNOPSIS
    从 libvosk.dll 生成 MSVC 导入库 vosk.lib

.DESCRIPTION
    vosk-win64 官方预编译包不附带 MSVC 导入库, 需要从 DLL 提取导出符号生成 .def,
    再用 lib.exe 生成 vosk.lib。本脚本自动完成:
      1. 定位 Visual Studio 工具链 (vswhere)
      2. dumpbin /exports 提取导出函数
      3. 生成 vosk.def + vosk.lib (x64)

    ⚠️ 本脚本内不使用 git, 可直接执行。

.EXAMPLE
    # 前置: 已将 libvosk.dll 放入 third_party/vosk/lib/
    .\generate-vosk-import-lib.ps1
#>

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

$VoskLibDir = Join-Path $ProjectRoot "third_party\vosk\lib"
$DllPath    = Join-Path $VoskLibDir "libvosk.dll"

Write-Host "=== Generate vosk.lib (MSVC import library) ===" -ForegroundColor Cyan

# ----------------------------------------------------------
# 1. 前置检查
# ----------------------------------------------------------
if (-not (Test-Path $DllPath)) {
    Write-Host "[ERR] 未找到 $DllPath" -ForegroundColor Red
    Write-Host ""
    Write-Host "步骤:" -ForegroundColor Yellow
    Write-Host "  1. 下载: https://github.com/alphacep/vosk-api/releases → vosk-win64.zip"
    Write-Host "  2. 解压, 将 zip 内的 libvosk.dll 放入:"
    Write-Host "     third_party\vosk\lib\libvosk.dll"
    Write-Host "  3. (建议) 将 zip 内的 MinGW 运行时 DLL 一并放入同一目录:"
    Write-Host "     libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll"
    Write-Host "  4. 重新运行本脚本"
    exit 1
}

# ----------------------------------------------------------
# 2. 定位 Visual Studio 工具链
# ----------------------------------------------------------
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "[ERR] 未找到 vswhere — 需要安装 Visual Studio (含 C++ 桌面开发工作负载)" -ForegroundColor Red
    exit 1
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Write-Host "[ERR] 未找到带 C++ 工具的 Visual Studio" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Visual Studio: $vsPath"

$msvcTools = Get-ChildItem (Join-Path $vsPath "VC\Tools\MSVC") -Directory |
             Sort-Object Name -Descending | Select-Object -First 1
if (-not $msvcTools) {
    Write-Host "[ERR] 未找到 MSVC 工具集目录" -ForegroundColor Red
    exit 1
}

$binDir = Join-Path $msvcTools.FullName "bin\Hostx64\x64"
$dumpbin = Join-Path $binDir "dumpbin.exe"
$libExe  = Join-Path $binDir "lib.exe"
Write-Host "[OK] MSVC 工具集: $($msvcTools.Name)"

# ----------------------------------------------------------
# 3. dumpbin /exports 提取导出函数名
# ----------------------------------------------------------
Write-Host "[..] 提取导出符号 ..."
$raw = & $dumpbin /exports $DllPath 2>$null
$exports = @()
foreach ($line in $raw) {
    # 数据行形如: "          1    0 000030F0 vosk_model_new"
    # forwarder 形如: "          1    0 000030F0 name = otherdll.export"
    if ($line -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(.+)$') {
        $name = ($Matches[1] -split ' = ')[0].Trim()
        if ($name -match '^[A-Za-z_][A-Za-z0-9_]*$') {
            $exports += $name
        }
    }
}

$exports = $exports | Select-Object -Unique

if ($exports.Count -eq 0) {
    # 解析失败回退: 使用 vosk_api.h 的已知 C API 导出清单
    Write-Host "[WARN] dumpbin 解析失败, 回退到内置导出清单" -ForegroundColor Yellow
    $exports = @(
        "vosk_set_log_level", "vosk_model_new", "vosk_model_new_spk",
        "vosk_model_free", "vosk_model_find_word", "vosk_spk_model_free",
        "vosk_recognizer_new", "vosk_recognizer_new_grm", "vosk_recognizer_new_spk",
        "vosk_recognizer_free", "vosk_recognizer_set_max_alternatives",
        "vosk_recognizer_set_words", "vosk_recognizer_set_spk_model",
        "vosk_recognizer_set_grm", "vosk_recognizer_accept_waveform",
        "vosk_recognizer_accept_waveform_s", "vosk_recognizer_accept_waveform_f",
        "vosk_recognizer_result", "vosk_recognizer_result_json",
        "vosk_recognizer_final_result", "vosk_recognizer_partial_result",
        "vosk_recognizer_reset", "vosk_batch_model_new", "vosk_batch_model_free",
        "vosk_batch_model_alloc_pcm", "vosk_batch_model_feed",
        "vosk_batch_model_finish_stream", "vosk_batch_model_wait",
        "vosk_batch_model_async_finish_stream", "vosk_batch_model_async_wait",
        "vosk_grm_compile"
    )
}

Write-Host "[OK] 提取到 $($exports.Count) 个导出函数"

# ----------------------------------------------------------
# 4. 生成 vosk.def + vosk.lib
# ----------------------------------------------------------
$defFile = Join-Path $VoskLibDir "vosk.def"
$defContent = "LIBRARY libvosk`r`nEXPORTS`r`n" + (($exports | ForEach-Object { "    $_" }) -join "`r`n") + "`r`n"
[System.IO.File]::WriteAllText($defFile, $defContent, [System.Text.Encoding]::ASCII)

$env:PATH = "$binDir;$env:PATH"
Push-Location $VoskLibDir
try {
    & $libExe /def:vosk.def /machine:x64 /out:vosk.lib | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERR] lib.exe 执行失败 (exit $LASTEXITCODE)" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

# ----------------------------------------------------------
# 5. 验证
# ----------------------------------------------------------
$libPath = Join-Path $VoskLibDir "vosk.lib"
if (Test-Path $libPath) {
    $size = (Get-Item $libPath).Length
    Write-Host ""
    Write-Host "=== 完成: vosk.lib 已生成 ($size bytes) ===" -ForegroundColor Green
    Write-Host "清理: 可删除中间文件 vosk.def (已被 .gitignore 覆盖)"
} else {
    Write-Host "[ERR] vosk.lib 未生成" -ForegroundColor Red
    exit 1
}
