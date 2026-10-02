# 开发版与正式版：初步代码比较

当前只推进开发版主程序，目标是同一个开发版 PC 程序适配不同资源目录并保留调试能力；正式生成代码作为功能对照，独立正式运行修复暂停。此前正式探测已完成生成、编译和链接，但启动在音频初始化期间退出，未验收标题或玩法。探测工程保留在仓库外作参考。

## 比较范围与结论

使用 ReXGlue v0.10.0 分别加载两份 XEX 镜像，导出初始映像后比较 PPC 指令。导出工具使用 `tool_mode`，不启动游戏，不从运行中的模拟器抓取内存。生成 C++ 是转译结果，不是还原出的原始 C++ 源码。

| 项目 | 开发版 | 正式版 |
| --- | --- | --- |
| 镜像 SHA256 | `a8fd9273e37a3930f3d1d4e8614f3b487f583d7d3335347b47fbf67f80972699` | `8935855e0fbd2461c0aba73163dd023e110d099f01816d6286d3a52211bc4286` |
| `.text` 字节数 | 7,910,188 | 7,333,532 |
| PDATA 函数记录数 | 28,471 | 23,073 |
| 启动 `main` | `0x821A0080`，Main.obj，MAP/PDB 公共及模块名称/位置一致 | `0x8216EA68`，由路径引用和调用顺序确认的对应入口候选 |
| 启动表路线 | 散 `dat\\files.tbl` 失败后尝试 `dat\\tables.pak+files.tbl` | 同样存在两次尝试 |
| 调试能力 | 已实际显示调试菜单，保留 MAP/PDB | 调试模块标记缺失，原菜单尚未发现 |

正式版代码段小约 7.29%，不代表少了 7.29% 的功能。两版编译优化、库实现、内联和链接布局均可能影响体积与函数数量；XEX 版本字段也不能用来直接判断开发先后或功能优劣。

比较器只处理 PDATA 中至少 64 字节的函数：563 个唯一字节匹配，5,043 个唯一规范化候选，11,136 个无法唯一对应；另有 11,729 个开发版短函数未参与。规范化忽略部分外部跳转和地址装载差异，可能把调用不同目标的函数归到同一形状，必须再核对调用、数据和运行行为。未匹配不证明功能新增或删除；没有 PDATA 的函数也不在此统计范围。

## 调试菜单是否只是关闭了

独立加载镜像后的 ASCII 字节检索结果：

| 标记 | 开发版出现次数 | 正式版出现次数 |
| --- | ---: | ---: |
| `GamePart_Test` | 15 | 0 |
| `test_part` | 28 | 0 |
| `CTextTester` | 15 | 0 |
| `CheatMenu` | 4 | 0 |

开发版 `silph::GamePartTask::RegisterToFactory<28, silph::GamePart_Test>::Create` 位于 `0x821FA5B0`，注册构造函数位于 `0x821FA860`，初始化入口为 `0x828BA480`，均归属 GamePart_Test.obj。前两个与 MAP/PDB 公共名称和位置匹配；三个模块记录都是位置匹配，初始化入口未匹配公共符号。正式版对应工厂尚未确认。

这些证据支持“正式构建裁掉了开发工具模块”的判断，尚不能证明所有调试代码完全删除。两版仍有 `GP_TEST` 枚举文字，正式版还有标题、教程、电影和战斗模块标记；仅有枚举字符串不代表调试菜单实现仍存在。换开发资源或配置不能增加主程序中缺少的函数，不能承诺修改一个开关就恢复菜单。

目前没有确认可从正式版移植的新增玩法或修复清单。要逐项比较任务、存档、场景切换等实际行为，再定位相关函数。不能根据未匹配函数数量宣称正式版有大量新增功能。

## 正式版探测状态

- **codegen**：官方生成 253 个输出文件。根据正式镜像指令补入 13 个入口种子、14 个终止重抛边界；致命未解析占位和函数内跳转标签检查通过。仍有 30 条 `float16_4` 指令警告，语义没有因此自动验收。
- **configure / compile / link**：Windows x64 Release 已通过。转译代码产生的 `roundevenf` 不由当前 Windows CRT 提供，手写兼容实现保留 ties-to-even 与线程舍入模式；独立测试覆盖四种舍入模式、正负零、半整数、边界、无穷与 NaN。正式探测使用官方 `GPU_PLUGINS xenos` 部署插件。
- **run**：GPU 初始化、XEX 加载、游戏主线程和音频注册已执行。约 3.64 秒后因 guest `0x824D4100` 未注册而 FATAL，退出 `0xC0000409`。没有完成菜单或玩法验收。第一次更早的退出是缺少 GPU 插件，不能与此次游戏调用失败混为一谈。

`0x824D4100` 的前五条指令与开发版 `0x82548448` 的 20 字节 AddRef 叶函数相同。开发地址有 XAUDIO/D3DX 多个 AddRef 别名，公共符号精确匹配、模块位置匹配。它是下一步函数发现调查线索，未据此强行命名或安装替换；完整对象归属还需调用现场确认。这类运行时间接调用缺口说明生成检查通过不等于所有回调都已注册。

## 当前维护路线与版本隔离

继续保留开发版的符号、原调试菜单和已验收行为。优先定位开发程序搭配正式资源的启动表、入口和资源差异；不因正式版名称就迁移主程序。目前没有正式新增玩法清单或完成度优劣证据。

- 两版使用独立 manifest、输入身份、生成目录、构建目录和可执行目标。不能把两套 `sub_ADDRESS` 注册及生成 C++ 链接进同一个目标，也不能使用固定地址偏移猜对应关系。
- 键盘、宿主音频队列、资源选择和窗口菜单等与游戏地址无关的实现可共用，但各版接入仍须验证。
- Fiber、电影、字幕、帧采样等地址或调用位置相关的配置/补丁，分别置于 development/retail 接入；核对正式函数后再共用逻辑。
- 第一阶段让开发主程序搭配正式资源进入标题/教程，核实文件加载和保存读回；没有差异证据时不移植。正式主程序不继续运行修复，试玩仍只作参考。

原开发调试工具迁往正式版会涉及工厂注册、对象布局、资源和调用依赖，工作量高于给 PC 宿主增加独立工具菜单。若只是需要帧率、资源选择等 PC 调试工具，可以共用现有宿主功能；若要原任务选择器或模型查看器，需要另立功能调查。

## 统一启动入口与镜像文件依赖

用户目标是一个开发版重编译程序，通过资源目录/游戏配置加载开发、正式等资源；不实现双 EXE 启动器或版本识别器。资源兼容必须逐场景核实，不能只凭路径相近承诺全部兼容。当前 `initial_game_part` 可按配置选择原游戏入口，默认仍遵循启动表；正式资源缺少调试表，需要另外解决原调试菜单资源供给。

当前 `Runtime::LoadXexImage` 调用 `UserModule::LoadFromFile`，加载 XEX 的 PE 初始映像、头部、导入和模块信息，然后准备游戏主线程。执行函数来自已重编译的 PC 程序；原 XEX 的数据仍提供全局初值、字符串、虚表、线程初始化等内容。把 PPC 函数转成 C++ 并不会自动移走这些数据，也不是先模拟运行游戏再抓取状态。

脱离外置原 EXE 有两条路线：

- 一次导入原镜像，保存带版本身份的输入包；以后加载这个包和完整游戏资源。原数据依赖仍在，只是无需保留原来的文件名或独立 EXE。
- 提取初始节数据和完整模块元信息，定义新的版本化镜像包，或嵌入 PC EXE；加载时恢复原地址布局、零初始化、导入修补、TLS 和页属性，再使用对应函数分发表。这需要可重复导出和新加载接入，不能只复制本次 `image.bin`。

SDK v0.10.0 已有 `UserModule::LoadFromMemory`，但 XEX 路线还需 `LoadXexContinue`、模块登记和运行环境初始化；当前宿主没有接内嵌或镜像包加载。分析快照是在 SDK 加载后导出的，可能含运行环境相关的导入修补，不作为可分发镜像包格式。

建议先让开发主程序搭配正式资源可靠启动，再做开发镜像的一次导入或内嵌，使切换资源目录不再要求复制独立原 EXE。保留原镜像用于重新生成和故障比较；更换输入封装不会自动补齐缺少的调试资源或修正资源格式差异。完整游戏资源仍需保留。

## 重复生成正式探测工程

在仓库目录运行，`<探测目录>` 必须是仓库和游戏资源目录之外的新目录；准备脚本核对镜像 SHA256、SDK 版本和目录边界，拒绝覆盖已有工程：

```powershell
python scripts/prepare_retail_probe.py --rexglue "<SDK>/bin/rexglue.exe" --image "<正式资源>/default.xex" --output-dir "<探测目录>"
& "<SDK>/bin/rexglue.exe" codegen "<探测目录>/sylpheed_retail_probe_manifest.toml"
./scripts/Check-GeneratedCode.ps1 -Directory "<探测目录>/generated/default"

. ./scripts/Initialize-WindowsToolchain.ps1
cmake -S "<探测目录>" -B "<探测目录>/out/build/release" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl "-DCMAKE_PREFIX_PATH=<SDK>"
cmake --build "<探测目录>/out/build/release" --parallel 2
```

该工程是最小探测宿主，尚未接入开发版所有 PC 功能。`config/retail/manifest.example.toml` 另提供只生成代码的相对路径示例；不要用它覆盖开发版 manifest。正式版原件、映像快照、完整匹配报告和探测构建均留本地；提交的是可重复工具与版本专用配置。

## 重复代码比较

```powershell
. ./scripts/Initialize-WindowsToolchain.ps1
cmake -S scripts/binary_analysis -B out/analysis/binary-tool -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl "-DCMAKE_PREFIX_PATH=<SDK>"
cmake --build out/analysis/binary-tool
./out/analysis/binary-tool/dump_xex_image.exe "<开发镜像>" "<仓库外>/development-image"
./out/analysis/binary-tool/dump_xex_image.exe "<正式镜像>" "<仓库外>/retail-image"
python scripts/compare_function_bodies.py --development "<仓库外>/development-image/image.bin" --retail "<仓库外>/retail-image/image.bin" --output "<仓库外>/function-comparison.json"
```

可另加 `--symbols "<本地符号目录>/functions.csv"` 给开发地址附上检索名字/对象线索；它选用索引的首条记录，不替代完整别名查询。导出器拒绝把输出放进原资源目录；比较器拒绝覆盖输入或把完整索引报告写入仓库。

已验证正式镜像重新导出字节一致、自身比较没有额外规范化候选、错误输入与目录保护拒绝。进一步语义比较可借助 IDA 的反汇编、交叉引用和伪代码，但本轮没有 IDA MCP 连接，不要求先恢复全部源码。
