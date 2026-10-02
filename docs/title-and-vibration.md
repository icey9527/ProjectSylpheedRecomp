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

资源 `dat/tables/jpn/GP_MOVIE.tbl` 的 `PATH=dat\GP_MOVIE\` 与 `ADVERTISE_MOVIE` 中 `MOVIE=ADV.wmv` 共同组成请求路径。当前配套资源没有该文件，运行日志记录打开失败；随后电影创建返回负 HRESULT，`OnStart` 在 `0x821E9C20` 调用 `Throw`。

官方 v0.10.0 的 `src/kernel/xboxkrnl/xboxkrnl_debug.cpp` 中，`HandleCppException` 读取抛出信息后直接调用 `rex::debug::Break()`，尚未完成异常展开。没有通过放行此断点或把抛出函数改为空函数来绕过故障。

待办：恢复正确电影资源，再验证播放器创建、解码、结束与跳过流程。零售资源有同名电影，仅文件名相同不能证明开发播放器兼容；当前没有改资源表、复制电影或实现播放器替换。

## 振动命令

`XInputSetState` 位于 `0x82338D40`，对象 `xapilib:xinpapi.obj`，MAP/PDB 公共及模块原名、位置一致。
`BaseLib::GamePad::SetVibration` 位于 `0x82818DE0`，对象 `BaseLib:XInput.obj`，公共原名及位置一致、模块位置一致；调用返回地址为 `0x82818E4C`。

`trace_vibration` 通过官方生成的弱别名机制记录命令，继续调用原 `__imp__sub_82338D40`。按大端读取两个 16 位强度，保留原结果与上下文，不插入输入查询或改变震动。

一次教程运行记录到 2,406 次命令变化，SDK 返回值均为成功；左右强度在不断变化。除最初归零外，本次日志在进程终止前未记录后续归零。用户未观察停止时机，未证明已经完成教程返回菜单路径，因此不能判定停止故障的唯一原因。

后续调查入口为 `SilpheedSCS::CVibrationManager::Update(float)`（`0x826CD828`，`SilpheedXenon:CVibrationManager.obj`）与 `silph::gamepart_helper::ResetVibration`（`0x82201628`，`GamePartHelper.obj`）；两者公共原名及位置一致、模块位置一致。需要结合实际场景区分持续效果、暂停/退出归零和设备停止问题。

## 性能边界

当前是宿主 Debug 构建，尚无同场景 Release 或模拟器基准。四个逻辑核的诊断运行中反复出现 SDK 少于六核的调度警告；日志开销和 Debug 开销是调查线索，未证实为全部性能原因。

v0.10.0 源码虽定义 `perf_log_csv`，本次启用后没有生成 CSV，源码中也未找到帧循环调用 `SetCsvLogPath`/`WriteCsvFrame`，不能把这个选项当作已可用的帧率测量方案。
