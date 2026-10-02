# 开发版运行

当前宿主对应开发镜像 `Xacalite_ScriptTeam.exe`。不要将零售 `default.xex` 改名替代它。

## 双击启动

构建后双击 `out/build/win-amd64-debug/project_sylpheed.exe`。同目录 `project_sylpheed.toml` 中配置：

```toml
game_data_root = "<完整资源目录>"
```

资源目录需包含匹配开发镜像、根 `config.ini` 和 `dat/`。相对路径基于宿主 EXE 目录；环境变量和命令行显式设置按 SDK 优先级覆盖配置。重编译宿主仍需要匹配镜像的初始数据和加载信息。

默认 1280×720 窗口。语言默认读取资源根 INI 的 `[LANGUAGE]` 空键；显式 `user_language` 设置优先。配置模板见 `config/project_sylpheed.example.toml`，语言处理见 [language-and-input.md](language-and-input.md)。

## 输入与已知问题

键盘/手柄菜单操作和基本音效已有实际验证。补齐键盘事件并移除不安全诊断采样后，用户重复菜单移动、确认/返回和长按复测未出现原 vector 断言。进入教程仍会触发 `abort()`；任务、场景切换和存档尚未验证。

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

## 其他版本资源实验

开发代码配合零售资源仅验证过镜像加载，完整兼容性未知。需要尝试时可使用 `scripts/Prepare-ResourceTest.ps1 -GameDataRoot "<资源目录>"`，仅补入缺少的开发镜像，保留已有文件。它不会把宿主转换为零售版；故障诊断优先使用配套开发版资源。
