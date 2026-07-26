# IDE 配置实践记录

> 本文记录了一台典型"旧设备"开发环境的完整 CMake + IDE 配置，供之后学习和参考。
> 环境特征：Qt 装在 H 盘（节约 C 盘）、无 vcpkg、VS Code 与 VS2022 双 IDE 并存。

## 目录

- [旧设备环境参数](#旧设备环境参数)
- [CMakeUserPresets.json — 构建预设](#cmakeuserpresetsjson--构建预设)
- [VS Code 配置](#vs-code-配置)
- [Visual Studio 2022 配置](#visual-studio-2022-配置)
- [双模式 CMakeLists.txt 工作原理](#双模式-cmakeliststxt-工作原理)
- [从旧代码迁移到双模式 (git pull 后只需改一行)](#从旧代码迁移到双模式)

---

## 旧设备环境参数

| 项目 | 值 |
|---|---|
| 操作系统 | Windows 10 |
| Qt 版本 | 6.9.3 (官方安装器) |
| Qt 路径 | `H:/Software/Qt/Qt6.9.3/6.9.3/msvc2022_64` |
| 编译器 | MSVC (Visual Studio 2022 Community) |
| CMake | 3.30.5 |
| IDE | VS Code (CMake Tools 插件) + VS2022 |
| 项目路径 | `H:\Laboratory\Kotocord` |
| vcpkg | 未安装 |

---

## CMakeUserPresets.json — 构建预设

旧设备使用 `CMakeUserPresets.json` 保存本地路径。环境变量化后，可以用更简洁的方式。

**旧版 (硬编码路径)：**

```json
{
    "cacheVariables": {
        "CMAKE_PREFIX_PATH": "H:/Software/Qt/Qt6.9.3/6.9.3/msvc2022_64"
    }
}
```

**新版 (环境变量) — 设置一次 `setx Qt6_DIR "..."` 之后，无需此文件：**

```json
{
    "cacheVariables": {
        "CMAKE_PREFIX_PATH": "$penv{Qt6_DIR}"
    }
}
```

> `$penv{NAME}` 是 CMakePresets 专用的宏——在 configure 时将环境变量 `NAME` 的值注入为 CMake 变量。这意味着 Preset 文件中不包含任何机器绝对路径，可以直接提交。

---

## VS Code 配置

### settings.json

```json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.sourceDirectory": "${workspaceFolder}",
    "cmake.buildDirectory": "${workspaceFolder}/build"
}
```

> `configurationProvider` 设为 `ms-vscode.cmake-tools` 后，C/C++ 扩展的 IntelliSense 会直接从 CMake 获取 include 路径和宏定义，不需要手动配置 `.vscode/c_cpp_properties.json`。

### launch.json

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Kotocord Debug (MSVC)",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "${workspaceFolder}/bin/Debug/Kotocord.exe",
            "cwd": "${workspaceFolder}/bin/Debug",
            "environment": [
                {
                    "name": "PATH",
                    "value": "H:/Software/Qt/Qt6.9.3/6.9.3/msvc2022_64/bin;${env:PATH}"
                }
            ],
            "preLaunchTask": "CMake: build"
        }
    ]
}
```

**关键设计点：**

1. **`type: "cppvsdbg"`** — 使用 Visual Studio 调试引擎（不是 gdb）。MSVC 编译的程序只能用 cppvsdbg 调试。

2. **`cwd` 设为 exe 所在目录** — 这样 `AppPaths::getProjectRootDir()` 的相对路径计算才能正确工作。Qt 也会在此目录下寻找 `platforms/` 插件。

3. **PATH 注入 Qt bin** — 这是手动模式下最关键的一行。因为 `windeployqt` 不会在手动模式下运行，Qt 的 `platforms/qwindows.dll` 不在 exe 同级。将 Qt bin 加入 PATH 让 Qt 能找到全局插件目录。

   ```
   运行时插件搜索链:
   Qt 启动 → 找 platforms/qwindows.dll
     → exe 同级 platforms/ 目录 (没有 → 跳过)
     → PATH 中的 Qt bin 目录 (H:/.../msvc2022_64/bin)
       → 找到! H:/.../msvc2022_64/plugins/platforms/qwindows.dll
   ```

4. **`preLaunchTask: "CMake: build"`** — 按 F5 启动调试前自动触发增量编译。CMake Tools 插件内置此 task。

### tasks.json

CMake Tools 扩展会自动注册构建任务，通常不需要手动写 `tasks.json`。旧设备上的文件是扩展生成的默认模板。

---

## Visual Studio 2022 配置

### launch.vs.json

```json
{
  "version": "0.2.1",
  "configurations": [
    {
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "Kotocord.exe",
      "name": "Kotocord.exe",
      "env": "PATH=H:\\Software\\Qt\\Qt6.9.3\\6.9.3\\msvc2022_64\\bin;${env.PATH}"
    }
  ]
}
```

> VS2022 的 CMake 集成通过 `launch.vs.json` 配置调试启动。和 VS Code 一样，PATH 中注入 Qt bin 是让调试时能找到 Qt 插件的关键。

### VS2022 的 CMake 工作流

1. `File → Open → CMake...` → 选择 `CMakeLists.txt`
2. VS 自动运行 CMake configure（使用 `CMakePresets.json` 中选中的预设）
3. 顶部下拉菜单选启动项 `Kotocord.exe`
4. F5 启动调试

VS2022 会在 `build/` 下生成 `.sln` 和 `.vcxproj`，可以直接用解决方案浏览器查看文件。

---

## 双模式 CMakeLists.txt 工作原理

核心控制流：

```
option(USE_VCPKG "..." ON)
       │
       ├── ON (默认) ──────────────────────────────────
       │   find_package(Qt6 ...)          ← vcpkg toolchain 提供路径
       │   find_package(whisper CONFIG)   ← vcpkg installed 提供
       │   链接 whisper target            ← IMPORTED target, 自带 include/libs
       │   部署: 从 vcpkg installed 拷贝 DLL + Qt 插件
       │
       └── OFF ───────────────────────────────────────
           find_package(Qt6 ...)          ← CMAKE_PREFIX_PATH 提供路径
           find_library(WHISPER_LIB ...)  ← third_party/whisper/lib/
           ggml.lib 搜索 → 找到则链接 / 找不到则假定编入 whisper.lib
           部署: 从 third_party/ 拷贝 whisper/ggml/Vosk DLL
```

**两种模式不重叠的部分只有：whisper.cpp 的发现方式和 DLL 部署策略。** 源码（`src/`）完全不知道模式的存在——它只通过 `WHISPER_LIBS` 变量链接，这个变量在两个模式下都被正确设置。

---

## 从旧代码迁移到双模式

旧设备 `git pull` 拉下环境变量化代码后：

### 1. 设置环境变量

```powershell
setx Qt6_DIR "H:\Software\Qt\Qt6.9.3\6.9.3\msvc2022_64"
```

关闭重新打开终端。

### 2. 选择 Preset

```powershell
# 使用全部手动的预设 (官方 Qt + third_party)
cmake --preset msvc-debug-all-manual

# 或保留旧 Preset 名, 在 CMakeUserPresets.json 中加两行:
# "USE_VCPKG_QT": "OFF", "USE_VCPKG": "OFF"
cmake --preset kotocord-msvc-local
```

### 3. 构建

```powershell
cmake --build build/msvc-debug-all-manual --config Debug
```

**不需要改动的文件：** `.vscode/`、`CMakeUserPresets.json`、`third_party/`、`build/`、`bin/` 均在 `.gitignore` 中。

### 旧 CMakeUserPresets.json 的兼容处理

若保留旧 Preset，在 `cacheVariables` 中加两行即可：

```json
"USE_VCPKG_QT": "OFF",
"USE_VCPKG": "OFF"
```
