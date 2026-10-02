# 标题与震动调查

## 调试菜单进入标题

开发版调试菜单的“进入标题”与教程返回菜单是不同路径。
实际捕获的宿主断点在 SDK `HandleCppException`，由以下原游戏调用链触发：

| Xbox 地址 | 符号 | 对象 | MAP/PDB 匹配 |
| --- | --- | --- | --- |
| `0x821E9808` | `silph::GamePart_Movie::Impl::OnStart` | `GamePart_Movie.obj` | 公共原名及位置一致；模块位置一致 |
| `0x8256A518` | `XMediaCreateXmvPlayerFromFile` | `Xmedia:xmvplayerapi.obj` | 公共及模块原名、位置一致 |
| `0x821E9168` | 匿名命名空间 `Throw` | `GamePart_Movie.obj` | 公共原名及位置一致；模块位置一致 |
| `0x82832618` | `_CxxThrowException` | `LIBCMT:throw.obj` | 公共及模块原名、位置一致 |
| `0x8284D328` | `RaiseException` | `xapilib:raiseexception.obj` | 公共及模块原名、位置一致 |

资源 `dat/tables/jpn/GP_MOVIE.tbl` 的 `PATH=dat\GP_MOVIE\` 与 `ADVERTISE_MOVIE` 中 `MOVIE=ADV.wmv` 共同组成请求路径。首次故障时缺少该文件，运行日志记录打开失败；随后电影创建返回负 HRESULT，`OnStart` 在 `0x821E9C20` 调用 `Throw`。

官方 v0.10.0 的 `src/kernel/xboxkrnl/xboxkrnl_debug.cpp` 中，`HandleCppException` 读取抛出信息后直接调用 `rex::debug::Break()`，尚未完成异常展开。没有通过放行此断点或把抛出函数改为空函数来绕过故障。

用户补齐视频后的新现场为播放器初始化中的未注册目标 `0x82555F58`，调用点 `0x82552B5C`。该目标是 `XAUDIO::CPCMSourceEffect::SetFrequencyScale` 的 `adjustor{16}` thunk，原名 `?SetFrequencyScale@CPCMSourceEffect@XAUDIO@@WBA@AAJM@Z`，对象 `xaudio:pcmsourceeffect.obj`；MAP/PDB 公共原名及位置一致，模块同位置符号缺失。

`config/runtime-functions.toml` 现补充该入口，由官方生成器恢复两条原始指令：将 `r3` 减去 16，跳转到已有的 `0x82555E18` 本体。没有猜测长度、改写音频逻辑或手改生成代码。Debug 构建后关闭跳过，玩家确认已有实际视频画面和声音，但报告卡顿，尤其切换到其他窗口时；自然结束尚待确认。补入口不等于所有视频和播放器功能都已验证。

## 跳过与创建失败兜底

当前 `skip_movies = false` 默认播放电影，`skip_failed_movies = true` 默认处理负 HRESULT 的创建失败。设 `skip_movies = true` 可使用已验证的跳过路径；关闭两者恢复原启动行为。创建成功且没有配置跳过时仍使用原更新函数；兜底不能恢复 `abort()`、硬件异常或任意解码故障。已有本机 TOML 若显式写了旧值 `true`，需要手动改为 `false`，构建脚本会保留玩家配置。

手写补丁在 `src/patches/movie/development/movie_fallback.cpp`，不修改生成函数。通过官方弱别名接入：

- 在 `OnStart` 的精确创建点 `0x821E9BC8` 截获已配置的跳过，或创建返回失败。该点之前的临时字符串已释放，渲染和声音初始化仍由原函数完成。
- 只抛出、捕获补丁自己的控制信号，恢复入口 PPCContext，保留已完成的游戏对象状态。补齐原启动尾部的四个振动暂停和声音组暂停，再标记下一次更新结束。激活场景前设置原有停止绘制标志（`Impl + 52` 的 `0x4` 位）与结束请求（`0x8` 位），避免渲染线程在更新之前访问空播放器。
- 下一次 `OnUpdateFrame`（`0x821E9F50`）调用原 `ExitToPrevPart`（`0x821E8EE0`）。只有这次完成查询、返回位置 `0x821E8F0C`、且没有播放器时，`IXMediaXmvPlayer_GetStatus`（`0x82569918`）返回完整的零状态对；其他查询保持原行为。
- 原结束函数释放准备好的 UI、资源表、场景对象，恢复渲染循环和声音模式、恢复四个手柄振动，再调用 `GamePartTask::ReturnToPrevPart`（`0x82205E28`）。不立即在启动回调里切换场景，不伪造播放器。
- 原析构入口 `0x821EB8D0` 清除尚未更新的跳过标记，防止取消场景后地址复用。

`OnStart`、`OnUpdateFrame`、`ExitToPrevPart`、析构、`ReturnToPrevPart` 的对象分别为 `GamePart_Movie.obj`、`GamePartTask.obj`，公共原名与位置一致，模块位置一致。`GetStatus` 的公共及模块原名、位置一致。

首次跳过复测曾在渲染线程发生空指针访问：`OnScene`（`0x821E8B38`，`GamePart_Movie.obj`，公共原名及位置一致、模块位置一致）调用 `IXMediaXmvPlayer_RenderNextFrame`（`0x825696C8`，`Xmedia:xmvplayerapi.obj`，公共及模块原名、位置一致）。停止绘制标志复用 `OnScene` 已有的提前返回分支，避免这次调用。

独立 ABI 测试使用真实 SDK 和补丁、受控原函数替身，验证 PPC 栈/非易失寄存器恢复、停止绘制及结束标志、成功创建透传、失败 HRESULT、精确调用点限制、一次性结束、取消清理和不吞其他异常；不能代替实际游戏的标题与资源清理验收。

```powershell
cmake -S tests/movie -B out/tests/movie -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="<SDK 目录>"
cmake --build out/tests/movie --parallel 2
ctest --test-dir out/tests/movie --output-on-failure
```

Debug 构建及 ABI 测试通过后，实际复测已记录跳过、完成状态 `7`、电影对象清理和返回 `GP_TITLE`；玩家确认调试菜单“进入标题”正常。此验收验证了默认跳过路径，未验证原视频播放或全部任务。

第一关随后在 `OnPrepare` 触发 `XamShowDirtyDiscErrorUI`：请求 `dat/GP_MOVIE/jpn/pwterop_s01a.prt`、`HGRGE00.TTF` 和 `SUBTITLE_S01A.tbl` 失败。检查资源目录发现同名文件在 `dat/jpn/`，是路径布局不一致的线索，尚未验证文件内容兼容性。此准备阶段早于本补丁的播放器创建接入点，仍需处理。SDK 弹窗的“bad or unimplemented file IO”是固定提示文字，不是具体根因诊断；缓存写入也存在访问拒绝，不能仅凭时间相邻就认定它触发了弹窗。

## 振动命令

`XInputSetState` 位于 `0x82338D40`，对象 `xapilib:xinpapi.obj`，MAP/PDB 公共及模块原名、位置一致。
`BaseLib::GamePad::SetVibration` 位于 `0x82818DE0`，对象 `BaseLib:XInput.obj`，公共原名及位置一致、模块位置一致；调用返回地址为 `0x82818E4C`。

`trace_vibration` 通过官方生成的弱别名机制记录命令，继续调用原 `__imp__sub_82338D40`。按大端读取两个 16 位强度，保留原结果与上下文，不插入输入查询或改变震动。

一次教程运行记录到 2,406 次命令变化，SDK 返回值均为成功；左右强度在不断变化。除最初归零外，本次日志在进程终止前未记录后续归零。用户未观察停止时机，未证明已经完成教程返回菜单路径，因此不能判定停止故障的唯一原因。

后续调查入口为 `SilpheedSCS::CVibrationManager::Update(float)`（`0x826CD828`，`SilpheedXenon:CVibrationManager.obj`）与 `silph::gamepart_helper::ResetVibration`（`0x82201628`，`GamePartHelper.obj`）；两者公共原名及位置一致、模块位置一致。需要结合实际场景区分持续效果、暂停/退出归零和设备停止问题。

## 性能边界

Debug 实际电影播放期间，原游戏的 `System UI Framerate` 警告记录了 91 个 66–144 毫秒样本，中位数 73 毫秒；这不是精确视频 FPS。Windows x64 Release 首次全量构建及最终增量构建已通过，使用 `-O3 -DNDEBUG` 和匹配的 Release SDK 库；同资源、按键、日志级别的受控运行记录到电影启动、清理和返回标题，没有捕获异常。清理日志无法区分自然结束与按键跳过，流畅度对比也仍待玩家确认。

四个逻辑核的运行中仍反复出现 SDK 少于六核的调度警告。Debug、日志和焦点切换是后续性能调查线索，尚无与模拟器的有效基准；没有为此改游戏时间步长或屏蔽读盘错误。

v0.10.0 源码虽定义 `perf_log_csv`，本次启用后没有生成 CSV，源码中也未找到帧循环调用 `SetCsvLogPath`/`WriteCsvFrame`，不能把这个选项当作已可用的帧率测量方案。
