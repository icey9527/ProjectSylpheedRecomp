# 开发版受控启动与首次运行修复

## 怎样启动

先按 [Windows 构建说明](building-windows.md) 生成并编译。在仓库目录执行（Python 3.11+，无需 pip）：

```powershell
python scripts/run_development.py --game-data-root "<开发版完整资源目录>" --seconds 45
```

目录必须包含匹配的 `Xacalite_ScriptTeam.exe`、根 `config.ini` 和 `dat/`。
脚本对镜像 SHA256 做校验，拒绝将零售镜像或其他版本交给当前宿主。
需要完整配套资源，仅有 EXE/MAP/PDB 不能运行游戏。

也可以在外层 `assets/runtime.local.json` 保存本机配置，再执行 `python scripts/run_development.py`：

```json
{"game_data_root": "<开发版完整资源目录>"}
```

本机配置、游戏文件和日志不进 Git。每次运行在外层 `logs/run-development-<时间>/` 保存
`runtime.log`、stdout/stderr 和 `result.json`；最新结果同步到 `logs/development-last-run.json`。
用户数据固定隔离在外层 `logs/runtime-user-data/development/`；原资源映射禁止游戏写入。

默认 45 秒，允许 1–600 秒；超时、Ctrl+C 或日志出现 `[FATAL]` 后清理宿主进程。
结果区分自然退出、超时、致命日志、启动失败和中断。退出 0 只说明进程自然正常结束，不能证明可玩。
当前开发版已显示调试菜单；启动脚本用于受控验证和收集证据，完整玩法尚未验收。

## 日志阶段的含义

- `image_selected`：选定正确虚拟镜像名称。
- `image_loaded`：SDK 完成镜像加载。
- `guest_thread_prepared`：SDK 准备游戏主线程；这个回调发生在 Resume 之前，不代表游戏代码已执行。

实际执行要另外看游戏自己的日志，如声音初始化、资源包加载和 `GP_TEST` 的 `OnStart()`。
包日志里的 `Loading...` 只证明开始加载，不能据此判所有资源加载成功。窗口、存活进程及这些阶段
都不能代替首帧、控制器、音频播放、任务和存档验收。

## 2026-10-01 首次运行与修复

第一次运行已加载开发 XEX，并开始游戏声音初始化，随后报告：

```text
Call to invalid or unregistered function at guest address 0x82546298
```

索引中这个精确函数起点有两个 MAP/PDB 公共名字与位置匹配的别名：
`XAUDIO::CEffectManager::QueryInterface(IXAudioEffectManager**)` 与
`XAUDIO::CFrameBuffer::QueryInterface(IXAudioFrameBuffer**)`，分别来自
`effectmanager.obj` / `framebuffer.obj`。原生成代码没有这个入口。

扩展 `Import-UnresolvedMapFunctions.ps1` 接受运行时未注册函数诊断，仍要求精确 MAP 函数起点，
保存两份别名且不猜函数长度。将其单独写入 `config/runtime-functions.toml` 并接入 manifest。
重新 codegen 自动发现 24 字节函数体，产生 `sub_82546298`；5 个输出更新，264 个保持不变。
Debug 增量构建成功，原有 44 个入口导入回归一致，30 条 float16_4 警告仍未修复。

复现导入（路径使用首次报错的本地日志，不要用已经越过此错误的日志）：

```powershell
./scripts/Import-UnresolvedMapFunctions.ps1 -LogPath "<首次运行的runtime.log>" -OutputPath config/runtime-functions.toml
./scripts/Codegen.ps1
./scripts/Build.ps1 -Configuration Debug -Parallel 2
python scripts/run_development.py
```

重测中该未注册错误消失，日志记录一个初始 sound bank 加载、
`SoundManager::Impl::InitializeWorker - Complete`、三份 Resource3D 包开始加载，以及 `GP_TEST OnStart()`。
调试模块 `xbdm` 没有阻止本次镜像加载；存在 `DmGetXbeInfo` stub 警告，仍不能断言所有调试功能可用。

## 首次入口修复后的阻塞（已续修）

重测约 4.66 秒后退出，Windows 退出码 `0xC0000005`。日志记录线程 `0xF8000124` 读取 guest
`0x0000000C`；另一个线程随后仍写出 `GP_TEST OnStart()` 日志。没有捕获故障 guest PC 或调用栈，
因此尚不能把异常归因到某个资源或函数。

2026-10-02 已捕获故障位置为 DeleteFiber 的 0x82338978，接入 SDK Fiber 生命周期后，
三次 30 秒重测均无该异常，并显示 GP_TEST 调试菜单。完整证据及后续调试命令见 [Fiber 修复](fiber-fix.md)。
缓存设备拒绝/找不到文件、部分效果资源注册失败、`ShaderDumpxe:` 未映射是独立线索，尚非已确认崩溃原因。
`dxcompiler.dll` 缺失日志指向调试反汇编不可用，也不能直接称渲染阻塞。
菜单首帧已验证；控制器、实际声音播放、3D 任务和存档仍未验证。
