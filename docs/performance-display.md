# 性能显示

Windows 窗口顶部“工具 → 运行信息”统一显示帧率、帧时间、进程 CPU 与内存。右上角面板保留 12 像素边距，可以点击标题左侧箭头折叠；取消菜单勾选后移除面板。默认关闭；F3 仍打开 SDK 自带调试面板，并使用同一帧数据来源。“工具 → 更改资源目录”选择完整资源根目录，保存后重启生效，见 [运行说明](running-development.md)。

菜单修改本次运行的选项。要设置下次启动的默认状态，在 EXE 同目录的 `project_sylpheed.toml` 中填写：

```toml
show_runtime_info = false
```

旧三个配置项仍接受：未显式设置新选项时，任一旧项为 true 就显示统一面板。新配置只需 `show_runtime_info`。折叠状态仅保留在本次运行，不自动改写启动配置。

帧率统计来自开发镜像 `D3DDevice_Swap`（`0x8235CD78`）正常返回，约一秒窗口内计算速率与平均间隔，曲线保留最近 120 个间隔。这是游戏 Swap 完成速率，不代表显示器实际呈现次数或视频解码帧率。没有近期样本时显示等待，不编造数据。

CPU 按全部逻辑处理器容量归一化，每 500 毫秒采样；例如四个逻辑处理器中一个满载约为 25%。内存分别显示工作集和私有提交量，单位 MiB。没有接入 GPU 占用。

原生菜单位于 `src/platform/windows/`，指标与显示位于 `src/features/performance/`，开发镜像接入位于 `src/patches/graphics/development/`。接入保留并调用原始 `__imp__` 函数，不修改生成代码。关闭显示后不保留监测面板或进程采样，仍收集固定容量的 Swap 时间戳供 F3 使用。

数值和原函数上下文透传测试：

```powershell
cmake -S tests/performance -B out/tests/performance -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="<完整 SDK 目录>"
cmake --build out/tests/performance
ctest --test-dir out/tests/performance --output-on-failure
```

测试不能替代实际菜单勾选、输入和游戏场景验收。当前功能用于观测；视频卡顿尚未优化。

Windows 菜单使用 `MNS_MODELESS`，避免打开菜单时阻塞 SDL 事件及显示循环。没有通过重入绘制或暂停音频处理菜单。

线程调查可以在游戏运行时执行 `python scripts/sample_thread_cpu.py --pid <游戏进程ID> --seconds 60`。它每秒读取 Windows 进程与线程 CPU 时间，不挂起线程；输出默认留在工作区外层 `logs/thread-cpu-*`。`cpu_total_percent` 按全部逻辑处理器归一化，`cpu_one_core_percent` 按一个逻辑处理器计，进程合计可超过 100%。这些数据用于确定哪些线程消耗 CPU，不能识别调用栈、等待原因或音频缓冲不足。

SDK 默认 `ignore_thread_affinities=true`、`ignore_thread_priorities=true`，由宿主调度线程，不强制使用 Xbox 的固定核心绑定。SDK 0.10 在少于六个逻辑处理器时，GPU 中断等路径仍会反复打印相同调度警告。宿主通过官方日志接口对这一条精确匹配的警告限频：保留首条，之后每十秒保留一条及重复计数；其他警告和错误照常记录。设置 `throttle_affinity_warning=false` 并重启可恢复完整记录。这减少重复日志输出，不改变游戏线程调度，也不保证解决视频断音。
