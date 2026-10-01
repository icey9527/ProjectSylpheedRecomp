# 开发版受控启动与首次运行修复

## 怎样启动

### 日常双击启动

构建后直接双击 `out/build/win-amd64-debug/project_sylpheed.exe`。
`Build.ps1` 自动创建 EXE 同目录的 `project_sylpheed.toml`，已有文件会保留。
首次创建时从工作区 `assets/runtime.local.json` 读取资源目录；也可运行：

```powershell
./scripts/Configure-Startup.ps1 -GameDataRoot "<开发版完整资源目录>"
```

之后用文本编辑器修改 EXE 旁 TOML 中的 `game_data_root` 即可。使用正斜杠，
例如 `game_data_root = "game-data"` 表示 EXE 旁的 game-data 目录。
模板见 [project_sylpheed.example.toml](../config/project_sylpheed.example.toml)。
不要修改资源包自身的 `config.ini`；游戏仍读取原来的资源配置。
日常运行不需要 Python；默认用户数据放在 SDK 选择的系统用户目录，日志在 EXE 旁 `logs/`。
可在 TOML 指定 `user_data_root`，缓存默认位于该用户目录的 `cache/`。
命令行优先于 TOML；TOML 优先于宿主默认。相对路径统一以 EXE 目录为基准。
宿主适配代码在 `src/app/startup_config.h`，没有改动生成游戏代码或 SDK。

默认窗口 1280×720，模板同时指定 guest video mode 1280×720。
`fullscreen = true` 可启用全屏；`window_width` / `window_height` 可改窗口大小，
其他分辨率的画面比例与 UI 适配尚未验收。

键盘通过 SDK 模拟手柄，窗口需要获得焦点：

| 手柄功能 | 键盘 |
| --- | --- |
| 方向键 | ↑ ↓ ← → |
| A（一般为确认） | Enter / 空格 |
| B（一般为返回） | Backspace |
| Start / Back | X / Tab 或 Z |
| 左摇杆 / 按下 | WASD / F |
| 右摇杆 / 按下 | 小键盘 8、2、4、6 / K |
| X / Y | L / P |
| 左右肩键 | 1 / 3 |
| 左右扳机 | Q / E（也可 I / O） |

鼠标摇杆保持关闭。每个游戏菜单采用哪种手柄按钮，仍以实际交互为准。
这些是输入配置，不代表全部菜单、任务和声音已经通过验证。
直接启动不会进行 Python 脚本的 SHA256 核对，必须使用匹配的开发版输入；诊断时优先使用下述受控脚本。

### 受控验证

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

## 2026-10-02 窗口与启动适配验证

Debug 增量编译/链接通过，重复构建保留已有 TOML。
从其他工作目录启动宿主，不传资源参数，已进入游戏资源加载与渲染。
本地窗口截图的客户区为 1280×720，并显示 Mission Select。
Windows 自动观察失败（`FrameArrived timed out`、前台窗口未提供 PID），随后用户按 Esc 停止 Computer Use；
自动化没有发送菜单按键，不将截图当成完整按键验收。
一次无参数运行约 30 秒后返回 `0x80000003`，末尾没有明确故障栈，原因未定位；
窗口配置交付不代表稳定性已完成。声音、任务、存档与该退出原因留待后续计划。

## 用户实测与下一处阻塞

用户随后实测反馈：音效正确，已有较多 UI 画面，多个 UI 交互正确。
这补充了菜单交互和实际音效的人工验证；尚未覆盖全部按键、所有菜单或任务流程。
进入教程时出现 Microsoft Visual C++ Runtime Library 的 `abort() has been called`。
目前只有用户弹窗文字，没有故障栈或 guest PC；不可直接认定与早先 `0x80000003` 同因。
下一轮先捕获教程入口的异常栈，再按符号定位；本轮按用户要求不修复此问题。

## 用零售资源测试开发代码

当前宿主执行的是开发镜像对应的重编译代码，但 SDK 仍读取该原始镜像的初始数据、
XEX 头、导入信息等。`Entrypoint XEX not found` 是 SDK 在真实加载前发现文件缺失；
不是本工程新增的版本校验，删除存在性检查不会免除后面的镜像读取。

不要把零售 `default.xex` 改名交给开发宿主。若要试用零售资源，先执行：

```powershell
./scripts/Prepare-ResourceTest.ps1 -GameDataRoot "<零售资源目录>"
```

脚本从本地 `assets/Xacalite_ScriptTeam.exe` 补入缺少的开发镜像，已有文件则保留，
不新增 hash/版本校验，不改变零售 `default.xex`、`config.ini` 或 `dat/`。
再将 EXE 旁 TOML 的 `game_data_root` 改为该零售资源目录，双击运行。
这测试“开发版代码 + 零售资源”，不是零售版主程序重编译；兼容性需逐项验证。

本机 15 秒受控试跑已通过原文件缺失位置，记录 `image_loaded` 和 `guest_thread_prepared`，
到时限终止、退出码 1。仅验收镜像加载；未操作菜单，也未验收零售资源的全部画面、教程或玩法。
