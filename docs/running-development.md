# 开发版运行

当前宿主对应开发镜像 `Xacalite_ScriptTeam.exe`。不要将零售 `default.xex` 改名替代它。

## 双击启动

构建后双击 `out/build/win-amd64-debug/project_sylpheed.exe`。同目录 `project_sylpheed.toml` 中配置：

```toml
game_data_root = "<完整资源目录>"
```

资源目录需包含匹配开发镜像、根 `config.ini` 和 `dat/`。相对路径基于宿主 EXE 目录；环境变量和命令行显式设置按 SDK 优先级覆盖配置。重编译宿主仍需要匹配镜像的初始数据和加载信息。

默认 1280×720 窗口。语言默认读取资源根 INI 的 `[LANGUAGE]` 空键；显式 `user_language` 设置优先。配置模板见 `config/project_sylpheed.example.toml`，语言处理见 [language-and-input.md](language-and-input.md)。

启动时资源目录无效，或缺少开发镜像、`config.ini`、`dat/files.tbl`，会提示具体缺项并打开目录选择。取消选择会退出本次启动。“工具 → 更改资源目录”也可选择目录；游戏继续使用当前资源，保存的新路径在重启后生效。选择器保存绝对路径；手写 TOML 也可使用相对路径，相对基准始终是 EXE 所在目录。

保存只替换 `game_data_root` 的值，保留其他设置、注释与换行；先验证 TOML 再原子替换，文件无效或不可写时明确提示，原配置不覆盖。命令行、环境变量显式指定的路径仍优先，测试脚本也会显式指定资源。这里只检查启动必需结构，没有将可选宣传片或全部关卡资源缺失都当作启动失败；运行时具体资源仍由游戏加载器检查并记录日志。

过场字幕默认开启，可以在同一 TOML 设置 `movie_subtitles = "on"`、`"off"` 或 `"game"`；后者遵循原游戏档案。原 `config.ini` 的 LANGUAGE 设置与字幕开关不同，字幕资源表只描述文字、字体及时间，详见 [字幕设置与存档](settings-and-saves.md)。

## 输入与已知问题

键盘/手柄菜单操作和基本音效已有实际验证。补齐键盘事件并移除不安全诊断采样后，用户重复菜单移动、确认/返回和长按复测未出现原 vector 断言。教程初始化的 JPEG 回调遗漏与非局部返回崩溃已修复，用户确认已看到飞船与太空的 3D 场景；完整任务、飞行操作、场景切换和存档尚未验证。修复依据见 [生成与运行配置修复](build-fixes.md)。

默认键盘映射：Enter/空格=A，Backspace=B，方向键=D-pad，WASD=左摇杆，数字小键盘 8/2/4/6=右摇杆，X=Start。鼠标摇杆默认关闭。玩家改键见 [键盘配置](keyboard-events.md#玩家自定义按键)。

## 受控诊断

在仓库目录执行（Python 3.11+）：

```powershell
python scripts/run_development.py --game-data-root "<完整资源目录>" --seconds 45
```

该脚本核对开发镜像 SHA-256、隔离用户数据、启用 GPU 插件，并保存日志。默认 45 秒，允许 1–600 秒；到时限、中断或致命日志时清理所启动的进程。直接双击宿主不执行该 Python 校验。

也可以将路径写入仓库同级 `assets/runtime.local.json`：

```json
{"game_data_root": "<完整资源目录>"}
```

诊断输出位于仓库同级 `logs/run-development-<时间>/`；用户数据位于 `logs/runtime-user-data/development/`，禁止游戏写入原资源映射。

`image_loaded` 只说明镜像加载完成，`guest_thread_prepared` 发生于主线程恢复之前。进程退出 0、窗口存在或资源开始加载，均不能证明任务可玩。

捕获 Windows 异常可使用 `scripts/debug_development.py`，先运行 `--help` 查看目标异常和时间限制；宿主本身没有 codegen 工具的 `--help` 退出行为。

定位 CRT `abort()` 弹窗时：

```powershell
python scripts/debug_development.py --game-data-root "<完整资源目录>" --capture-breakpoints --seconds 300
```

在该脚本启动的窗口中手动复现；如果出现 CRT 弹窗，选择“重试（Retry）”让调试器取得断点现场。脚本跳过初始加载器断点，捕获后续断点、目标地址的首次访问违规或任意未处理异常，并结束本次启动的进程。它不会跳过错误继续游戏。

证据保存在仓库同级 `logs/crash-probe-<时间>/`：`launch.json`、`runtime.log`、`capture.json`（异常与各线程的宿主栈）、`host.dmp` 和 `result.json`。转储包含线程栈及部分关联内存，不是完整 Xbox 内存快照。宿主寄存器不能直接当作 PPC 寄存器；SDK DLL 缺少匹配 PDB 时，部分栈帧只能显示地址或导出名。

调查持续震动时可增加 `--trace-vibration`，或在宿主 TOML 中设置 `trace_vibration = true`。日志记录玩家、左右马达强度、返回值和调用地址；相同命令合并计数。此开关默认关闭，启用时仍执行原函数，不改变马达命令。频繁变化的命令会增加日志量，诊断结束后关闭。

## 已定位的标题与震动问题

调试菜单的“进入标题”会进入电影模块。缺少 `dat/GP_MOVIE/ADV.wmv` 时，原版会抛出 SDK 尚不能展开的 C++ 异常；补齐电影后又确认播放器调用了未注册的音频回调 `0x82555F58`。

该回调现已通过官方 codegen 补齐，关闭跳过后已实际显示视频。当前默认 `skip_movies = false` 播放电影，设为 `true` 可使用已验证的跳过路径。已有 EXE 旁 TOML 若显式写了旧值 `true`，需改为 `false`；构建不会覆盖玩家配置。`skip_failed_movies = true` 会在播放器创建返回失败时自动结束该电影，避免缺文件导致原版抛异常；它不会捕获 `abort()` 或所有运行错误。补丁接入和验证说明见 [电影与标题](title-and-vibration.md)。

跳过路径已实际进入标题，玩家确认正常。第一关曾在播放器创建之前因字幕、字体及 `.prt` 目录不一致出现读盘错误；本地资源布局现已按表对齐，但第一关尚未完整复测。实际电影画面和声音已确认，流畅度与正常结束仍须验收。

视频性能对比可使用优化构建，宿主 Release 仍对应开发版游戏：

```powershell
./scripts/Build.ps1 -Configuration Release
python scripts/debug_development.py --configuration Release --game-data-root "<完整资源目录>" --capture-breakpoints --seconds 600
```

诊断脚本默认仍运行 Debug；`--log-level info` 可减少调试日志。构建配置与日志级别写入 `launch.json`，对比时应记录并尽量保持一致。Release 与 Debug 各自使用 EXE 同目录 TOML，先核对 `skip_movies` 和资源路径。

教程中已记录到游戏持续发送变化的非零震动命令，SDK 返回成功；这次观察未确认回菜单或关闭后的停止时机。持续震动尚未修复，也未判定为正常效果。

Release 构建与受控运行已通过，视频流畅度对比和切换窗口时的卡顿仍待确认。少于六个逻辑核时，SDK `XThread::SetActiveCpu` 会反复输出调度警告；它是否影响帧时间仍需测量。尚无与模拟器的有效性能对比。

## 其他版本资源实验

开发代码配合零售资源仅验证过镜像加载，完整兼容性未知。需要尝试时可使用 `scripts/Prepare-ResourceTest.ps1 -GameDataRoot "<资源目录>"`，仅补入缺少的开发镜像，保留已有文件。它不会把宿主转换为零售版；故障诊断优先使用配套开发版资源。
