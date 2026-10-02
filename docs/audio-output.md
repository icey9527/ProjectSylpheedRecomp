# 宿主音频队列

游戏的 WMV/WMA 解码、XAudio 混音、SDK 音频供给线程和 48 kHz / 256 样本的帧格式保持原样。`src/audio/host_audio_system.cpp` 经官方 `RuntimeConfig.audio_factory` 接入，只适配宿主 SDL 输出队列。

SDK 原队列在样本转换、SDL 提交和唤醒供给线程时保持队列锁。宿主队列只在取出和回收缓冲时持锁；样本缓冲预分配，转换临时数据放在栈上。双声道折叠、六声道混合、音量、静音、缺帧填零和已消费帧归还信号量额度沿用 SDK 规则，没有改游戏计时或提升线程优先级。

EXE 同目录 TOML 可以切换后端，修改后重启：

```toml
host_audio_queue = true
trace_audio_queue = false
```

`host_audio_queue=false` 完整恢复 SDK 原输出后端。诊断启动可以使用 `scripts/debug_development.py --configuration Release --trace-audio`，会在单独的低频线程每秒记录 `SYLPHEED_AUDIO` 汇总。正常运行诊断默认关闭。实时回调不记录逐帧日志；输出失败仍报告错误。

| 指标 | 含义 |
| --- | --- |
| submit / played | 当前报告周期收到 / 消费的 256 样本帧数 |
| silence | 当前周期因队列缺帧而填零的块数；主动静音仍计入 played |
| queued | 采样时队列剩余块数 |
| max_submit_gap_us | 当前周期观察到的最大供给间隔，包括跨周期的间隔 |
| max_callback_us | 当前周期最大回调墙钟耗时，可能包含线程被抢占的时间 |

刚创建、切换场景或没有播放内容时的填零不等于故障。需要与实际视频期间的声音中断和帧间隔关联判断。缩短锁区不保证解决解码、计时器或音频供给的所有卡顿。

需要定位供给停顿时，可在 Windows 调试启动中加入 `--capture-breakpoints --audio-starvation-break-ms 100`。启动十秒后，队列耗尽且供给间隔达到阈值时，每个输出驱动最多触发一次调试断点，脚本保存所有线程栈并结束该实例。此诊断默认关闭，仅在调试器附加时生效；场景切换或停止播放也可能触发，捕获必须结合当时画面分析。

验证使用真实 SDL 无设备音频流和 SDK 信号量，不依赖测试机扬声器：

```powershell
cmake -S tests/audio -B out/tests/audio -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="<完整 SDK 目录>"
cmake --build out/tests/audio
ctest --test-dir out/tests/audio --output-on-failure
```

测试检查双声道 / 六声道、静音、FIFO、缺帧填零、已消费帧额度、并发供给消费和重复关闭。实际设备、视频听感和长期稳定性还需运行验证。
