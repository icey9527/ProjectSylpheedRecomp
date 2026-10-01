# Project Sylpheed Recomp

使用 ReXGlue v0.10.0 的 Project Sylpheed Xbox 360 静态重编译初始工程。
当前目标是完成代码生成及宿主构建准备，尚未验证游戏运行。

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
PDB 保留供后续调查；本阶段没有自动导入 PDB。

## 代码生成

安装 ReXGlue v0.10.0，并将 rexglue.exe 所在目录加入 PATH。在仓库目录执行：

```powershell
./scripts/Codegen.ps1
```

`generated/` 可重新生成，不直接修改。官方 init 生成的 CMakeLists、presets 和宿主入口保存在仓库中。

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

```powershell
cmake --preset win-amd64-debug -DCMAKE_PREFIX_PATH="<SDK安装目录>"
cmake --build --preset win-amd64-debug
```

本次初始化环境 PATH 未找到 CMake、Ninja 和 Clang，宿主构建尚未执行。

最新验证：codegen 成功，生成 269 个文件；生成 C++ 中未解析调用的致命占位代码为 0。
仍有 30 条 `Unexpected float16_4 pack instruction` 警告，待核对 SDK 的指令语义；宿主编译和游戏运行尚未验证。
