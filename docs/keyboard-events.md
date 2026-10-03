# 键盘手柄事件适配

## v0.10 缺口与接入

ReXGlue v0.10.0 的 `MnkInputDriver` 已实现键盘到手柄状态的映射，但按键回调没有调用 `EnqueueKeystroke`，因此 `GetDeviceKeystroke` 始终返回 EMPTY。

开发镜像的 `BaseLib::GamePad::Update`（`0x82819520`，`BaseLib:XInput.obj`）同时读取 `XInputGetKeystroke`（`0x82338D58`）和 `XInputGetState`（`0x82338D30`）；这些入口均有 MAP/PDB 名称与位置匹配。`GamePad::Open`（`0x828192D8`）通过 `XInputGetCapabilities`（`0x82338D28`）连接设备。

`src/input/keyboard_keystroke_driver.*` 通过官方 `RuntimeConfig::input_factory` 和 `InputSystem::AddDriver` 接入。默认 SDK 驱动继续负责控制器状态、能力、振动和设备分配；补充驱动只提供缺少的 VK_PAD 事件，不增加状态贡献或玩家槽，不改游戏生成代码。

事件使用 SDK 的 `keybind_*` 设置，支持逗号分隔的按键别名、精确 Shift/Ctrl/Alt 修饰符、摇杆方向及对角线。首次按下产生 KEYDOWN，释放产生 KEYUP；长按在 400ms 后重复，每次间隔 100ms，与 SDK SDL 驱动的默认节奏相同。快速点按也保留完整事件。

失焦或覆盖层捕获输入时丢弃待执行动作并释放已按下的键。补充驱动不吞 UI 事件、不读取 Windows 全局键盘状态，也不消耗 SDK 的鼠标运动。

此适配针对 v0.10.0。升级 SDK 时先检查官方 MnK 事件实现；如果已经提供同一事件流，应撤下补充驱动，避免重复输入。

## 玩家自定义按键

编辑宿主 EXE 同目录的 `project_sylpheed.toml`，保存后重启游戏。`config/project_sylpheed.example.toml` 列出全部常用手柄映射；现有配置不会被构建脚本覆盖。省略的项继续使用 SDK 默认值，空字符串可取消该项的键盘映射。

| 手柄功能 | TOML 配置项 | 项目默认键 |
| --- | --- | --- |
| A / B | `keybind_a` / `keybind_b` | Enter 或空格 / Backspace |
| X / Y | `keybind_x` / `keybind_y` | L / P |
| Start / Back | `keybind_start` / `keybind_back` | X / Z 或 Tab |
| 左 / 右肩键 | `keybind_left_shoulder` / `keybind_right_shoulder` | 1 / 3 |
| 左 / 右扳机 | `keybind_left_trigger` / `keybind_right_trigger` | Q 或 I / E 或 O |
| 左摇杆方向 | `keybind_lstick_up/down/left/right` | W / S / A / D |
| 右摇杆方向 | `keybind_rstick_up/down/left/right` | 小键盘 8 / 2 / 4 / 6 |
| 左 / 右摇杆按下 | `keybind_lstick_press` / `keybind_rstick_press` | F / K |
| 十字键方向 | `keybind_dpad_up/down/left/right` | 方向键 |

例如把确认改为 J 或空格，把 Start 改为 Enter：

```toml
keybind_a = "J,Space"
keybind_start = "Return"
```

键名区分大小写：Enter 使用 `Return`，方向键使用 `Up` / `Down` / `Left` / `Right`，小键盘使用 `Numpad8` 等 SDK 名称。逗号表示任选一个键，也支持 `Shift+Up`、`Ctrl+J` 等组合；修饰键须精确匹配，绑定 `J` 时按住 Shift 的 `J` 不等同于它。避免把同一键分配给多个动作，否则它们会同时触发。

`mnk_mode = true` 开启键盘转手柄。鼠标按键可以作为普通绑定：`LMB`、`RMB`、`MMB` 分别表示左、中、右键。当前默认右键复用键盘 `1` 的左肩键动作，左键复用键盘 `3` 的右肩键动作，并保留原键盘替代键。SDK 的相对鼠标镜头默认关闭，因为它会隐藏系统箭头；可见箭头的镜头输入需要独立实现，不能简单打开原生捕获模式。

当前已加入独立的鼠标输入路径：窗口 `MouseEvent::dx/dy` 在 UI 线程累积，合成输入驱动在下一次 `XInputGetState` 轮询时提供左摇杆 `thumb_lx/thumb_ly`，随后立即归零。它是宿主对原生窗口鼠标事件的适配，不是 Xbox 游戏原生鼠标接口；游戏仍只看到标准 XInput 摇杆状态。点击鼠标左键、右键或中键后，宿主调用 SDK 提供的相对鼠标模式、捕获窗口并隐藏光标，因此移动不会在窗口边缘停止；按 Escape 或窗口失焦会释放捕获并恢复原来的光标状态。窗口关闭时会先排空已排队的 UI 回调，再释放捕获，避免残留回调访问已销毁的窗口。当前固定缩放为初始值的 4 倍，后续只根据实际游戏手感调整这一处。

官方 SDK 负责物理输入、按键配置和手柄状态，我们补充缺失的事件流。不能据此推断所有其他重编译游戏都要自己写键盘驱动：只查询状态的游戏可能直接使用 SDK 已有支持。

鼠标镜头由本项目的输入驱动合成左摇杆，不要同时把 `mnk_mouse` 设为 `true`：SDK 的 `MnkInputDriver` 会自行消费鼠标并做相对捕获，两路输入会叠加，且两个模块争用同一窗口的相对鼠标模式。保持默认 `mnk_mouse=false`。

## 独立验证

测试不需要游戏或符号原件，使用真实 SDK MnK 驱动与可控物理手柄替身，验证事件、状态合并、能力、振动和玩家分配。从配置好 Clang/MSVC 的终端执行：

```powershell
cmake -S tests/input -B out/tests/input -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="<SDK 目录>"
cmake --build out/tests/input --parallel 2
ctest --test-dir out/tests/input --output-on-failure
```

测试包含别名同时按下、两次轮询之间点按、重复时序、组合键切换、对角线转换、重复失焦、覆盖层捕获和禁用模式。测试用最小 `WindowedAppContext`/`Window` 替身驱动真实的待执行队列，覆盖点击后延迟捕获、Escape/失焦释放、关窗排空并在 UI 线程分离监听、非 UI 线程析构不阻塞也不触达窗口。

Debug 宿主已编译链接。补齐事件后用户确认键盘可以移动和确认菜单，重复操作曾出现 SDK 的 vector 迭代器断言。

已移除旧输入诊断在 UI 线程调用 `InputSystem::GetState` 的路径：SDK v0.10 的状态与事件查询均刷新设备容器，没有同步保护，不能与游戏线程并发查询。新版用户复测菜单重复移动、确认/返回和长按后报告没有断言；日志也有游戏读取按下、释放和重复事件的记录。未捕获原断言栈，不将这次复测扩大为长期稳定性或任务玩法验收。

宿主前 16 条 `SYLPHEED_KEYSTROKE` 日志记录游戏读取的 VK_PAD 事件；`SYLPHEED_INPUT` 仅记录物理按键，不再查询 SDK 状态。

官方参考：[输入驱动源码](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/input/mnk/mnk_input_driver.cpp)、[运行时配置接口](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/include/rex/runtime.h)、[输入系统](https://github.com/rexglue/rexglue-sdk/blob/v0.10.0/src/input/input_system.cpp)。
