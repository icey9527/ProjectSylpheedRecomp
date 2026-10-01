# Project Sylpheed Recomp

使用 ReXGlue v0.10.0 的 Project Sylpheed Xbox 360 静态重编译初始工程。
已完成 Windows x64 Debug 宿主编译与链接，尚未验证游戏运行。

## 本地目录

将仓库克隆到工作区的 `repo/`，在同级 `assets/` 放置自己的游戏输入：

```text
workspace/
  assets/
    Xacalite_ScriptTeam.exe
    Xacalite_ScriptTeam.map
    Xacalite_ScriptTeam.pdb
  repo/
    config/
    scripts/
    src/
    project_sylpheed_manifest.toml
```

源码、manifest、分析修复和脚本使用 Git 管理。游戏输入、符号原件、生成代码和构建产物不纳入仓库。
本地符号索引已读取 MAP 和 PDB 公共/模块函数及标签，并连接生成代码位置。
生成函数仍使用地址名字，符号名字通过索引查询；没有将 PDB 全量灌入生成器。
原始符号和完整索引留在仓库外层。

## 代码生成

安装 ReXGlue v0.10.0，并将 rexglue.exe 所在目录加入 PATH。在仓库目录执行：

```powershell
./scripts/Codegen.ps1
```

`generated/` 可重新生成，不直接修改。官方 init 生成的 CMakeLists、presets 和宿主入口保存在仓库中。
Codegen.ps1 默认保存外层日志、检查未解析调用致命占位，并在有 Python/MAP/PDB 时刷新符号索引。

## 分析修复

原始分析报告出现 53 条 UnresolvedCall，涉及 43 个不同目标地址。
生成阶段另发现 XapiFiberSwapContext (0x8233AB90) 未解析。合计 44 个入口全部在原始 MAP 的函数符号中找到，因此在
`config/map-functions.toml` 添加函数入口。未设置猜测的函数长度，也没有绕过错误校验。
该配置由以下脚本从本地日志和 MAP 生成：

```powershell
./scripts/Import-UnresolvedMapFunctions.ps1 -LogPath ../logs/map-import-input.txt
```

导入脚本只接受 MAP 标记为函数的精确地址；任一地址不匹配则拒绝写入。
原日志和 MAP 留在本地，已生成的入口配置已纳入 Git。

## 宿主构建

还需要包含开发文件与 CMake package 的完整 ReXGlue SDK、CMake、Ninja、Clang 和 Windows 开发环境。
通过本地 CMakeUserPresets.json 或 CMAKE_PREFIX_PATH 配置 SDK，不将本机绝对路径写进共享配置。

日常编译入口：`./scripts/Build.ps1`。脚本自动加载 VS x64 环境；优先发现外层 tools 中的完整 SDK，
其他位置可用 `-SdkRoot` 指定。详细步骤见下面的 Windows 教程。

```powershell
cmake --preset win-amd64-debug -DCMAKE_PREFIX_PATH="<SDK安装目录>"
cmake --build --preset win-amd64-debug
```

本机安装后的工具已核实：Clang 19.1.5、CMake 3.31.6、Ninja 1.12.1、MSVC 14.44、Windows SDK 10.0.26100.0。
完整官方 SDK 已补齐，CMake 配置通过。现有 32 位 TDM-GCC 不作为本项目编译器。

## 项目文档

- [Windows 安装与编译入门](docs/building-windows.md)
- [符号对应、验证范围与查询方法](docs/symbols.md)
- [代码结构与模块目录规则](docs/architecture.md)
- [修改、返工、验证和 Git 提交规则](docs/contributing.md)
- [分阶段实施路线](docs/roadmap.md)
- [首次编译的异常处理边界修复](docs/build-fixes.md)
- [三版本比较、符号基线与运行阶段](docs/versions.md)

快速查询：`python scripts/symbols.py query TextObj`。完整索引生成命令：`python scripts/symbols.py build`。
手写宿主代码位于 `src/app/`；后续补丁按职责分模块，生成代码保持由 ReXGlue 管理。

最新验证：codegen 成功，生成 269 个文件；生成 C++ 中未解析调用的致命占位代码为 0。
首次构建还修复了 17 个重新抛出异常的 catch funclet 边界，并检查函数内 goto 标签完整性。
Windows x64 Debug 编译、链接已通过；输出为 `out/build/win-amd64-debug/project_sylpheed.exe`。
仍有 30 条 `Unexpected float16_4 pack instruction` 警告，待核对 SDK 指令语义，游戏运行尚未验证。
