# 开发版 Fiber 访问异常修复（2026-10-02）

## 问题与修复

开发版初始化曾约 5 秒后以 `0xC0000005` 退出，日志只报告读取 guest `0x0000000C`。
Windows 调试 API 和 DbgHelp 捕获实际故障：`DeleteFiber` 接收空指针，在 guest 指令
`0x82338978` 执行 `lwz r4,12(r31)`；原生成位置为 `.4.cpp:8500`。
调用链是 `silph::Session::Impl::FiberManager` → `BootFiberManager` → `XapiThreadStartup`。

管理器前一步调用 `SwitchToFiber`，原转译路径进入 `XapiFiberSwapContext`，仅更改 guest 寄存器、
guest 栈和跳转。协作执行还需要暂停/恢复宿主 C++ 调用栈，否则返回管理器时可能沿用被切换的寄存器状态。
ReXGlue v0.10.0 已提供基于宿主 Fiber 的完整生命周期实现，但原 manifest 未接入它。

`config/fiber-rexcrt.toml` 将完整五函数接到 SDK 的 `rexcrt_*` 实现。没有为 `DeleteFiber(0)`
添加忽略路径，也没有改原游戏资源、生成 C++ 或 SDK 二进制。采用完整生命周期，避免混用两套上下文布局。

| 原函数名 | 开发版地址 | 对应 SDK 实现 |
| --- | --- | --- |
| ConvertThreadToFiber | 0x82338788 | rexcrt_ConvertThreadToFiber |
| ConvertFiberToThread | 0x82338818 | rexcrt_ConvertFiberToThread |
| CreateFiber | 0x82338868 | rexcrt_CreateFiber |
| DeleteFiber | 0x82338948 | rexcrt_DeleteFiber |
| SwitchToFiber | 0x823389A8 | rexcrt_SwitchToFiber |

五个精确起点均来自 `xapilib:fiber.obj`，MAP/PDB 公共及模块原始名字、位置匹配。
仅适用记录的开发镜像 SHA256；不将地址复制给其他镜像。
SDK 参考位置：`src/kernel/crt/threading.cpp`、`src/core/fiber_win32.cpp`。

## 验证与限制

- codegen：42 个输出更新、227 个保持不变；未解析致命占位/函数内 goto 检查通过。
- Debug 增量构建：最终 24 步编译/链接通过；SDK DLL 的五个 rexcrt 导出已核实。
- 三次受控重测：均存活到 30 秒时限，由脚本终止；没有原访问异常或新的 FATAL。
  超时退出码不是自然正常退出，不能称稳定运行通过。
- 日志记录真实 Fiber 创建、转换与删除；Session 管理器调用已改到原生 hooks。
- 实际窗口截图显示 GP_TEST 调试菜单，有 Test UI、Mission Select、Tutorial、Config、
  Go Title、Model Viewer、Particle Viewer 等。只验收菜单首帧，不保证这些选项都能运行。
- 30 条 float16_4 指令警告、符号严格身份、部分资源/缓存失败与 CPU 核数警告仍待调查。
  控制器、实际声音播放、3D 场景、任务及存档未验收。

五个函数不再有独立生成 `sub_ADDRESS` 定义，属于预期原生接入。
索引现有 34,575 个生成函数、34,369 个 MAP 地址对应独立生成入口；其余 269 个中包含这五个原生接入，
不能把数量下降误判为新漏生成。索引当前只识别 sub_ADDRESS，查询原生接入时同时查此配置。

## 怎样复现和捕获后续异常

正常重测：

```powershell
./scripts/Codegen.ps1
./scripts/Build.ps1 -Configuration Debug -Parallel 2
python scripts/run_development.py --seconds 30
```

异常捕获（Windows + 64 位 Python，标准库，无需单独安装 LLDB）：

```powershell
python scripts/debug_development.py --seconds 45
```

沿用本地资源配置和开发 EXE hash 校验，捕获匹配的 first-chance 访问异常或任意 second-chance 访问异常。
默认匹配 host `0x10000000C`（本次 guest 0xC 的映射）；可通过 `--fault-address 0x...` 指定新的地址。
已修复版本不再触发旧异常时，工具可能结束并报告没有捕获目标，不能据此判游戏失败或完全正常。

证据写在外层 `logs/crash-probe-<时间>/`：launch/capture JSON、宿主寄存器、PDB 函数/源码行、栈和相关内存。
这里的寄存器是宿主 x64 寄存器，不能直接当 PPC 寄存器。匹配异常后主动终止调试进程，保留现场；
其他 first-chance 异常交给宿主自己的处理器（图形 MMIO 等可能正常使用访问异常）。
无目标异常时按时限清理，也不自动修复或跳过故障。

本地测试原型已捕获真实游戏崩溃；仓库脚本另用一个主动读取相同无效地址的本地小程序验证了
first-chance 捕获、`main` 函数名/源码行解析和进程清理。游戏文件、截图、栈数据及测试产物不提交。
