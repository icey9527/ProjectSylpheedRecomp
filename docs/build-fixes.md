# 首次构建发现的分析修复

## catch funclet 跳转到了未生成的标签

Windows x64 首次编译在 `sub_826C4788` 和 `sub_826D8614` 报缺少 `loc_826C4824`、
`loc_826D86B0`。对全量生成代码按函数作用域检查，共发现 17 个同类 catch funclet。

这些起点的 MAP 名称都是 `__catch$...`，PDB 模块 LABEL32 在相同 section:offset 有同名标签。
例如 `0x826C4788` 为 `__catch$229987`，来源 `SilpheedXenon:CStageSpecification.obj`。
每个函数都将 r3/r4 置零后调用 `0x82832618`，MAP/PDB 确认该目标为 `_CxxThrowException`。
这是重新抛出异常，不会正常返回；无界函数发现却继续解码后面的 EH 数据/其他 funclet，
随后生成了指向未包含代码的 goto。

`config/catch-boundaries.toml` 给这 17 个重新抛出异常的 funclet 指定独占结束地址：
调用 `_CxxThrowException` 后的下一条指令地址。该地址来自生成 PPC 指令对应的 LR 值，
不是由相邻 MAP 名字之间的距离猜测长度。其他 catch funclet 和调用目标保持各自入口。
没有添加虚假标签、删跳转、跳过编译或手改 generated。

`scripts/Check-GeneratedCode.ps1` 现在同时检查未解析调用致命占位，以及每个
`DEFINE_REX_FUNC` 内引用但未定义的 `loc_ADDRESS` 标签。此检查用于捕捉该生成器输出格式的
明确错误，不能代替 C++ 编译或指令语义/异常路径的运行验证。

原始发现记录留在外层 `logs/missing-generated-labels.json`，修复日志和完整构建日志也留在外层。
30 条 float16_4 指令警告另行跟踪，本项修复没有解决那些警告。

## 教程图片加载遗漏 JPEG 回调

进入教程时，CRT `abort()` 的宿主栈经过 `ResolveIndirectFunction`、
`D3DX::reset_input_controller`（`0x82425818`）、`jpeg_consume_input` 和
`jpeg_read_header`。运行日志确认未注册的调用目标为 `0x8241C748`。

MAP 将该入口标为 `D3DX::reset_error_mgr`，对象为 `d3dx9:jerror.obj`；
PDB 模块 procedure 在相同 `0003:002AC748` 位置有可读名字，公共符号中没有同位置记录。
`jpeg_std_error`（`0x8241C760`）把该地址写入错误管理器的 `+16` 回调字段，
`reset_input_controller` 从该字段取出并调用。它是原版的真实回调，原生成结果缺少其入口。

`config/runtime-functions.toml` 补充该入口，交给官方分析器生成和注册，不指定猜测长度，
也不替换为空函数。生成的原函数包含六条 PPC 指令：将错误管理器的 `+108` 和 `+20`
字段清零后返回。修复通过配置保留，重新 codegen 会重建对应函数与注册。
