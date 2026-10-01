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

用户安装 C++ 工作负载后已确认 VS 2022 Build Tools 17.14、MSVC 14.44、Windows SDK 10.0.26100.0，
以及 VS 内的 Clang 19.1.5（x86_64-pc-windows-msvc）、CMake 3.31.6、Ninja 1.12.1。
普通终端不一定包含 VS 工具目录；构建脚本会为当前进程加载 x64 开发环境，不修改系统 PATH。
现有 GCC 目标为 `mingw32`，不作为本工程的工具链。
完整官方 SDK 已补齐到仓库外层 `tools/rexglue-sdk-0.10.0-win-amd64/win-amd64/`，含开发文件与依赖。
原 F 盘的 bin-only 安装保留。实际工具和 SDK 发现仍以检查脚本及 CMake 结果为准。

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

上述系统工具由用户安装；首次构建时补齐的是官方 SDK 开发包，没有修改系统 PATH。

## 安装后怎样编译

日常编译可以直接在 PowerShell 执行：

```powershell
cd E:\ProjectSylpheedRecomp\repo
./scripts/Build.ps1
```

脚本自动加载 VS x64 工具、检查完整 SDK、配置 CMake 并以两个并行任务编译 Debug。
生成标签检查通过后，有 Python 和本地 MAP/PDB 时还会刷新符号索引。
默认优先使用上面外层 tools 中的完整 SDK；其他位置通过 `-SdkRoot` 或进程环境变量
`REXGLUE_SDK_ROOT` 指定。可用 `-Configuration Release` 或 `-Parallel 2` 调整。
Windows PowerShell 5.1 和 PowerShell 7 均采用同一套脚本；执行策略阻止脚本时可用
`powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Build.ps1`，只作用于该进程。

完整原生 configure/build 输出分别保存到外层 `logs/configure-win-amd64-debug.log`、
`logs/compile-win-amd64-debug.log`，过程摘要位于 `logs/build-win-amd64-debug.log`。
日志采用追加模式保留返工记录。

需要手动控制时，先开 **x64 Native Tools Command Prompt for VS 2022**，在里面进入 PowerShell。
还可在现有 PowerShell 点源 `. ./scripts/Initialize-WindowsToolchain.ps1`，然后执行以下命令。
确认 clang++ 的目标是 Windows x64/MSVC，而不是 MinGW。

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
首次编译遇到的 catch funclet 边界问题及配置修复见 [build-fixes.md](build-fixes.md)。

## 当前构建结果与运行限制

Windows x64 Debug 编译和链接已通过。CMake 会部署 runtime、Tracy 及运行时加载的 xenos GPU 插件。
插件需要明确请求 `GPU_PLUGINS xenos`，因为它不是链接依赖，自动扫描链接 DLL 不会把它复制出来。

宿主已通过 OnLoadXexImage 选择开发镜像 Xacalite_ScriptTeam.exe。
运行脚本检查开发镜像 SHA256，并设置资源目录、隔离用户数据、xenos 插件、日志和超时。
使用方法及实际运行证据见 [开发版启动说明](running-development.md)。
当前已修复 Fiber 崩溃并显示开发调试菜单，见 [修复证据](fiber-fix.md)；输入、声音播放和任务仍待验收。
构建通过或双击出现窗口不能证明重编译语义正确。
SDK 宿主没有工具版 rexglue 的 `--help` 退出行为，不能用该参数作为无窗口 smoke test。

## 若只有 SDK 源码

SDK 也支持 `-DREXSDK_DIR=<源码目录>`，但这是另一种更复杂的构建方式：
必须固定 v0.10.0、初始化 Git 子模块，且满足 SDK 自身的外部依赖和构建要求。
外层 tools/rexglue-sdk-source 当前只是浅克隆的参考源码，子模块没有初始化，不能当作已可构建的 SDK。
先优先使用完整开发包；只有确实需要修复 SDK 时，再为其建立单独计划、Git 维护与可重复构建流程。
