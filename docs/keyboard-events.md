# 键盘手柄事件适配

## v0.10 缺口与接入

ReXGlue v0.10.0 的 `MnkInputDriver` 已实现键盘到手柄状态的映射，但按键回调没有调用 `EnqueueKeystroke`，因此 `GetDeviceKeystroke` 始终返回 EMPTY。

开发镜像的 `BaseLib::GamePad::Update`（`0x82819520`，`BaseLib:XInput.obj`）同时读取 `XInputGetKeystroke`（`0x82338D58`）和 `XInputGetState`（`0x82338D30`）；这些入口均有 MAP/PDB 名称与位置匹配。`GamePad::Open`（`0x828192D8`）通过 `XInputGetCapabilities`（`0x82338D28`）连接设备。

`src/input/keyboard_keystroke_driver.*` 通过官方 `RuntimeConfig::input_factory` 和 `InputSystem::AddDriver` 接入。默认 SDK 驱动继续负责控制器状态、能力、振动和设备分配；补充驱动只提供缺少的 VK_PAD 事件，不增加状态贡献或玩家槽，不改游戏生成代码。

事件使用 SDK 的 `keybind_*` 设置，支持逗号分隔的按键别名、精确 Shift/Ctrl/Alt 修饰符、摇杆方向及对角线。首次按下产生 KEYDOWN，释放产生 KEYUP；长按在 400ms 后重复，每次间隔 100ms，与 SDK SDL 驱动的默认节奏相同。快速点按也保留完整事件。

失焦或覆盖层捕获输入时丢弃待执行动作并释放已按下的键。补充驱动不吞 UI 事件、不读取 Windows 全局键盘状态，也不消耗 SDK 的鼠标运动。

此适配针对 v0.10.0。升级 SDK 时先检查官方 MnK 事件实现；如果已经提供同一事件流，应撤下补充驱动，避免重复输入。

## 独立验证

测试不需要游戏或符号原件，使用真实 SDK MnK 驱动与可控物理手柄替身，验证事件、状态合并、能力、振动和玩家分配。从配置好 Clang/MSVC 的终端执行：

```powershell
cmake -S tests/input -B out/tests/input -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="<SDK 目录>"
cmake --build out/tests/input --parallel 2
ctest --test-dir out/tests/input --output-on-failure
```

测试包含别名同时按下、两次轮询之间点按、重复时序、组合键切换、对角线转换、重复失焦、覆盖层捕获和禁用模式。

Debug 宿主已编译链接。补齐事件后用户确认键盘可以移动和确认菜单，重复操作曾出现 SDK 的 vector 迭代器断言。

已移除旧输入诊断在 UI 线程调用 `InputSystem::GetState` 的路径：SDK v0.10 的状态与事件查询均刷新设备容器，没有同步保护，不能与游戏线程并发查询。新版用户复测菜单重复移动、确认/返回和长按后报告没有断言；日志也有游戏读取按下、释放和重复事件的记录。未捕获原断言栈，不将这次复测扩大为长期稳定性或任务玩法验收。

宿主前 16 条 `SYLPHEED_KEYSTROKE` 日志记录游戏读取的 VK_PAD 事件；`SYLPHEED_INPUT` 仅记录物理按键，不再查询 SDK 状态。

官方参考：[输入驱动源码](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/input/mnk/mnk_input_driver.cpp)、[运行时配置接口](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/include/rex/runtime.h)、[输入系统](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/input/input_system.cpp)。
