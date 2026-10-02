# Windows 构建

已验证的目标为 Windows x64 Debug；Release 和其他平台尚未完成运行验证。

## 依赖

项目使用 C++23，ReXGlue v0.10.0 要求 Clang。安装：

- Visual Studio 2022 / Build Tools 的“使用 C++ 的桌面开发”：MSVC v143 x64 工具集和 Windows SDK。
- LLVM/Clang、CMake 3.25+、Ninja；可使用 Visual Studio 自带版本。
- [ReXGlue v0.10.0 完整 Windows SDK](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0)。保留 `bin/`、`include/`、`lib/` 和依赖目录结构；只有 codegen 可执行文件不足以构建宿主。
- Python 3.11+，用于输入校验、符号查询和运行诊断；无需第三方 Python 包。

已验证工具版本：Clang 19.1.5、CMake 3.31.6、Ninja 1.12.1、MSVC 14.44、Windows SDK 10.0.26100.0。不要混用 MinGW/TDM-GCC 的工具链与 MSVC SDK 库。

## 生成与构建

仓库已跟踪 [匹配的开发版输入与生成源码](game-inputs.md)。从仓库目录执行；修改生成配置时需要 codegen，初次构建也可直接执行 `Build.ps1`：

```powershell
python scripts/verify_game_inputs.py --image-only
./scripts/Codegen.ps1 -ReXGlue "<SDK 目录>/bin/rexglue.exe"
./scripts/Build.ps1 -SdkRoot "<SDK 目录>"
```

`Build.ps1` 自动加载 VS x64 工具环境、检查依赖、配置 CMake 并构建，默认两个并行任务。也可通过环境变量 `REXGLUE_SDK_ROOT` 指定 SDK；默认发现位置为仓库同级 `tools/rexglue-sdk-0.10.0-win-amd64/win-amd64/`。

若执行策略阻止脚本，可对单次进程使用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -SdkRoot "<SDK 目录>"
```

代码生成、configure 和 compile 日志保存到仓库同级 `logs/`。构建后输出位于 `out/build/win-amd64-debug/`，已有启动配置会保留。

运行性能对比使用优化构建：`./scripts/Build.ps1 -Configuration Release`，输出位于
`out/build/win-amd64-release/`。它与 Debug 分别保留各自的 EXE 旁配置；
Release 仍运行同一份开发版游戏，不会转换成零售版。首次构建需要编译全部生成代码。

## 手动构建

先打开 VS 的 x64 开发者终端，或在 PowerShell 点源 `. ./scripts/Initialize-WindowsToolchain.ps1`：

```powershell
./scripts/Check-BuildEnvironment.ps1 -SdkRoot "<SDK 目录>"
cmake --preset win-amd64-debug -DCMAKE_PREFIX_PATH="<SDK 目录>"
cmake --build --preset win-amd64-debug --parallel 2
```

本机路径可写入被忽略的 `CMakeUserPresets.json`，不要写入共享 manifest/presets。

SDK 的库与 DLL 必须和 Debug/Release 配置一致。CMake 会部署运行时和 `xenos` GPU 插件；缺少依赖时检查完整 SDK 包和配置输出，不要混装零散 DLL。

只从 SDK 源码构建时，需要固定版本、初始化子模块，并满足 SDK 自身构建依赖。详见 [官方文档](https://github.com/rexglue/rexglue-sdk/wiki)。

编译链接成功不代表游戏流程通过。运行配置见 [运行说明](running-development.md)，生成边界处理见 [build-fixes.md](build-fixes.md)。
