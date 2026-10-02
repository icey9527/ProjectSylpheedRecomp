# Project Sylpheed Recomp

使用 [ReXGlue v0.10.0](https://github.com/rexglue/rexglue-sdk) 将 Project Sylpheed 的 Xbox 360 开发版静态重编译为 Windows 程序。

目前已验证 Windows x64 Debug 构建、调试菜单画面、手柄菜单操作和音效。键盘输入尚不可用，进入教程会触发 `abort()`；任务与存档尚未验证。零售版主程序不在当前支持范围内。

## 构建

需要 Clang、MSVC 工具集、Windows SDK、CMake、Ninja、完整 ReXGlue v0.10.0 开发 SDK，以及匹配的游戏输入。仓库不包含游戏或符号原件。

在仓库同级的 `assets/` 放置 `Xacalite_ScriptTeam.exe`；符号分析另外需要同版 MAP/PDB。文件清单与匹配信息见 [输入文件说明](docs/game-inputs.md)。

在仓库目录执行：

```powershell
python scripts/verify_game_inputs.py --image-only
./scripts/Codegen.ps1 -ReXGlue "<完整 SDK 目录>/bin/rexglue.exe"
./scripts/Build.ps1 -SdkRoot "<完整 SDK 目录>"
```

构建结果为 `out/build/win-amd64-debug/project_sylpheed.exe`。安装和工具链配置见 [Windows 构建说明](docs/building-windows.md)。

## 运行

双击构建出的 EXE，在同目录 `project_sylpheed.toml` 中设置 `game_data_root`，指向含匹配开发镜像、`config.ini` 和 `dat/` 的资源目录。默认窗口为 1280×720，目前请使用手柄。

配置模板在 `config/project_sylpheed.example.toml`。详细配置和诊断启动见 [运行说明](docs/running-development.md)。

## 开发

- [代码结构](docs/architecture.md)：生成代码、宿主适配和补丁的边界。
- [符号索引](docs/symbols.md)：按地址或名字定位函数。
- [贡献说明](docs/contributing.md)：验证和提交要求。
- [语言与输入](docs/language-and-input.md)、[Fiber 修复](docs/fiber-fix.md)、[生成边界修复](docs/build-fixes.md)：现有适配依据与限制。

`generated/` 由 ReXGlue 重建，不直接编辑。游戏输入和构建产物不纳入 Git。

项目原创代码使用 [BSD-3-Clause](LICENSE)；第三方声明见 [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt)。
