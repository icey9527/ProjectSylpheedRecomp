# Windows 编译入门

## 四种东西各做什么

| 工具 | 作用 |
| --- | --- |
| ReXGlue codegen | 把游戏 PowerPC 指令转换为 C++ |
| Clang/clang++ | 把这些 C++ 编译成 Windows x64 机器码 |
| CMake | 读取项目与 SDK 配置，生成构建规则 |
| Ninja | 按构建规则执行编译与链接 |
| Visual C++ 工具集 + Windows SDK | 提供 Windows 头文件、微软 C++ 标准库、系统库和链接环境 |
| 完整 ReXGlue 开发 SDK | 提供 rex 头文件、库、CMake 配置、依赖及运行时 |

v0.10.0 的 SDK CMake 明确要求 Clang；官方 Windows 自动构建使用 LLVM、CMake、Ninja。
项目使用 C++23。不要把现有 TDM-GCC-32 的 32 位 MinGW 和微软 SDK 库混用。
安装 Visual C++ 工具集仍然必要，虽然实际编译器选择的是 Clang。

## 本机检查结果（2026-10-01）

已安装 VS 2022 Build Tools，但 vswhere 未发现 VC x64/x86 C++ 工具集。
未找到 Windows SDK 的头文件与 x64 系统库。PATH 未找到 clang++、CMake、Ninja。
现有 GCC 目标为 `mingw32`，不作为本工程的工具链。
ReXGlue 安装目录目前只有 `bin/`，还不是可以链接的完整开发 SDK。
这些结果来自当前发现环境；安装后在新的开发者终端重新检查。

## 安装步骤

1. 打开 **Visual Studio Installer**，找到现有的 **Build Tools 2022**，点“修改”。
2. 选择 C++ 构建工作负载，通常显示“使用 C++ 的桌面开发”。确认包含：
   - MSVC v143 的 x64/x86 C++ 工具集；
   - Windows 10 或 Windows 11 SDK（选择一个受支持的新版本即可）；
   - C++ CMake tools for Windows（提供 CMake/Ninja，实际可用性以检查为准）。
3. 安装 x64 的 LLVM/Clang，确保能使用 `clang.exe` 和 `clang++.exe`。
   可用 Installer 中的 C++ Clang 工具或 [LLVM 官方发布](https://github.com/llvm/llvm-project/releases)。
   以官方 SDK 支持的近期版本为准；不要假设旧版工具支持 C++23。记录实际版本，后续保持一致。
4. 如果开发者终端找不到 CMake/Ninja，可单独安装 [CMake](https://cmake.org/download/)（至少 3.25）及
   [Ninja](https://github.com/ninja-build/ninja/releases)，并加入 PATH。
5. 从 [ReXGlue v0.10.0 发布页](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0)
   获取 `rexglue-sdk-0.10.0-win-amd64.zip` 的完整开发 SDK 包，保留目录结构。
   至少应有 `include/rex/rex_app.h`、`lib/cmake/rexglue/rexglueConfig.cmake`、导入库及依赖配置。
   仅有 rexglue.exe 和 DLL 可用于生成，不足以编译项目；不要从零散来源随意凑库。

本次只检查并记录，没有替你安装或修改系统环境。

## 安装后怎样编译

打开 **x64 Native Tools Command Prompt for VS 2022**，在其中输入 `powershell` 或 `pwsh`。
这样微软工具集环境会传给 PowerShell；普通终端仅能找到文件不一定具备链接环境。
确认 `clang++ --version` 的目标是 Windows x64/MSVC，而不是 MinGW。

```powershell
cd E:\ProjectSylpheedRecomp\repo
./scripts/Check-BuildEnvironment.ps1 -SdkRoot 'F:/rexglue-sdk-0.10'
./scripts/Codegen.ps1
cmake --preset win-amd64-debug -DCMAKE_PREFIX_PATH='F:/rexglue-sdk-0.10'
cmake --build --preset win-amd64-debug
```

这里的 F 盘路径是当前本机位置的示例；须替换为完整开发 SDK 的真实目录。
共享 manifest/CMake 不保存本机绝对路径；长期本地配置可以写入已忽略的 CMakeUserPresets.json。
首次 Debug 输出预期在 `out/build/win-amd64-debug/`；是否运行成功还要检查资源和运行日志。
如果并行编译占用过多内存，可在 build 命令后加 `--parallel 2`。

SDK 提供 Debug/Release/RelWithDebInfo 对应的库和 DLL，保持配置一致。
由 SDK CMake 配置处理依赖和运行时部署，不要手动把不同版本 DLL 混进构建目录。
SDK 配置会查找 fmt、spdlog、utf8cpp、SDL3 等包；完整包应携带兼容依赖，配置失败时根据报错定位。

## 若只有 SDK 源码

SDK 也支持 `-DREXSDK_DIR=<源码目录>`，但这是另一种更复杂的构建方式：
必须固定 v0.10.0、初始化 Git 子模块，且满足 SDK 自身的外部依赖和构建要求。
外层 tools/rexglue-sdk-source 当前只是浅克隆的参考源码，子模块没有初始化，不能当作已可构建的 SDK。
先优先使用完整开发包；只有确实需要修复 SDK 时，再为其建立单独计划、Git 维护与可重复构建流程。
