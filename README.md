# Project Sylpheed Recomp

使用 [ReXGlue v0.10.0](https://github.com/rexglue/rexglue-sdk) 将 Project Sylpheed 的 Xbox 360 开发版静态重编译为 Windows 程序。

目前已验证 Windows x64 Debug 构建、调试菜单画面、键盘/手柄菜单操作和基本音效。已修复教程图片加载的回调遗漏及错误返回崩溃；任务与存档尚未验证。零售版主程序不在当前支持范围内。

## 构建

需要 Clang、MSVC 工具集、Windows SDK、CMake、Ninja 和完整 ReXGlue v0.10.0 开发 SDK。

仓库 `assets/` 包含当前开发镜像及完整 MAP/PDB，`generated/` 包含对应生成源码。运行仍需完整游戏资源。文件与匹配信息见 [输入文件说明](docs/game-inputs.md)。

在仓库目录执行：

```powershell
python scripts/verify_game_inputs.py --image-only
./scripts/Codegen.ps1 -ReXGlue "<完整 SDK 目录>/bin/rexglue.exe"
./scripts/Build.ps1 -SdkRoot "<完整 SDK 目录>"
```

构建结果为 `out/build/win-amd64-debug/project_sylpheed.exe`。安装和工具链配置见 [Windows 构建说明](docs/building-windows.md)。

## 运行

双击构建出的 EXE，在同目录 `project_sylpheed.toml` 中设置 `game_data_root`，指向含匹配开发镜像、`config.ini` 和 `dat/` 的资源目录。默认窗口为 1280×720；键盘和手柄都能操作菜单，按键可在 TOML 自定义。

配置模板在 `config/project_sylpheed.example.toml`。详细配置和诊断启动见 [运行说明](docs/running-development.md)。

## 开发

- [代码结构](docs/architecture.md)：生成代码、宿主适配和补丁的边界。
- [函数命名与符号索引](docs/symbols.md)：已有 62 个入口恢复可读名字，按地址或名字定位函数。
- [贡献说明](docs/contributing.md)：验证和提交要求。
- [语言与输入](docs/language-and-input.md)、[Fiber 修复](docs/fiber-fix.md)、[生成边界修复](docs/build-fixes.md)：现有适配依据与限制。

`generated/` 由 ReXGlue 重建并跟踪，不直接编辑。完整资源包、构建产物和本机配置不纳入 Git。

项目原创代码使用 [BSD-3-Clause](LICENSE)；第三方声明见 [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt)。
