# 原版语言配置与键盘调查

## 语言保留原版 INI

实际游戏主流程不是只读取 INI 默认值，也不是简单由镜像分区决定：

1. `silph::Silph::Impl::OnInit`（`0x821A76D8`，Silph.obj）取得配置表的语言节。
2. 调用 `XTLGetLanguage`（`0x82338C98`，xapilib:getlang.obj），通过 `ExGetXConfigSetting` 读取系统用户语言；仅在该值无效时再用区域回退。
3. `TableA::GetStr(int)`（`0x8280BF18`）先取该语言编号对应的值；未找到才通过 `TableA::GetStr(char const*)`（`0x8280BF78`）取默认值。
4. `silph::GetDataPath`（`0x821D8AD8`）等资源路径选择也调用 `XTLGetLanguage`。

这些名字和起点来自当前开发镜像的 MAP/PDB 匹配和生成调用，不能应用到零售代码。
SDK 的另一个 API `XGetLanguage` 固定返回英语，但它不是上述主流程的入口；不要仅修改它就声称游戏语言已修好。

宿主现在在初始化时读取当前资源目录的 `config.ini`，用 `[LANGUAGE]` 的空键默认值设置 SDK `user_language`。
例如：

```ini
[LANGUAGE]
    = jpn ; default
#0x01 = eng
#0x02 = jpn
```

把 `= jpn` 改为 `= eng`，下次启动即可按英语编号选择资源。支持原版六种值：
`eng`、`jpn`、`deu`、`fra`、`esp`、`ita`，分别对应系统语言编号 1–6。
编号行仍由游戏解释，应与默认语言所对应的行保持一致；不支持任意自定义语言代码。
资源目录由 EXE 旁 TOML 的 `game_data_root` 决定，不一定是原来的开发目录。
原 INI 格式、日文注释编码和游戏读取逻辑保留；宿主只读 ASCII 语言项，不重写原文件。

显式 TOML / 环境 / 命令行 `user_language` 高于 INI 默认值；没有可识别默认值时保持 SDK 设置。
改动见 `src/app/game_config.h` 和 `startup_config.h`。
Debug 编译、真实开发/零售 INI 与解析边界检查通过；实际游戏运行日志记录 `user_language=2`。
这证明配置接入；各资源包的完整语言画面/字幕/音频还需实测，调试菜单文字也不一定有翻译。

## 键盘仍待修复

实际测试中 W/S、方向键、Enter、空格均不能操作游戏；正确的 UI 交互由手柄完成。
不能用手柄成功替代键盘验收。

新增输入观察只记录前 20 次游戏窗口按键，不吞键、不注入输入。
实际运行已记录：W/S 到达窗口，玩家 0 的左摇杆 Y 为 ±32767；方向键产生 D-pad 位，其他玩家未连接。
因此至少这些按键已通过窗口和 SDK 状态映射，仍需继续定位游戏事件消费。

SDK v0.10.0 的 `MnkInputDriver::EnqueueKeystroke` 有定义却没有调用。
独立测试当前 SDK 二进制也证实：Return 按住时状态有 A，但 `GetDeviceKeystroke` 返回 4306（EMPTY）。
游戏 `BaseLib::GamePad::Update`（`0x82819520`，BaseLib:XInput.obj，MAP/PDB 精确名字位置匹配）同时查询 `XInputGetKeystroke` 和 `XInputGetState`。
这是已确认的 SDK 事件队列缺口，尚未通过修复重测证明它是菜单无反应的唯一原因。
后续需要验证按下、松开、重复与焦点丢失后的释放，同时检查游戏当前玩家和能力查询。
状态映射正确不能单独证明游戏已消费输入事件。

## 后续配置键清点

保留原版 INI 更合适，暂不迁移到 TOML。TOML 管宿主路径、窗口和输入，游戏 INI 管原有游戏设置。
后续从 `Silph::GetConfigTable`（`0x821A41C0`）的调用者追踪 `OpenSection` / `GetStr` / `GetInt`，
为每个键记录节名、类型、默认行为、使用函数、地址、调试/普通用途及运行证据。
项目中很多 `TableA` 查询属于 `dat/` 的其他表，不能把它们全部列成根 `config.ini` 的隐藏键。
此清点尚未完成；尚无经过验证的完整调试配置清单。教程 abort 为独立待定位问题。
