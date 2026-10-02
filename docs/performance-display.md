# 性能显示

Windows 窗口顶部的“显示”菜单提供三个独立的勾选项：帧率与帧时间、进程 CPU 占用、内存占用。勾选后显示只读面板，全部取消后移除面板。默认全关；F3 仍打开 SDK 自带调试面板，并使用同一帧数据来源。

菜单修改本次运行的选项。要设置下次启动的默认状态，在 EXE 同目录的 `project_sylpheed.toml` 中填写：

```toml
show_game_fps = false
show_process_cpu = false
show_process_memory = false
```

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
