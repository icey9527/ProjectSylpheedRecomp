# 符号名字与代码定位

## 名字现在怎样对应

已有明确配置的 62 个函数通过官方 `[functions].name` 生成可读 C++ 名字，其余函数继续采用 `sub_地址` 或 SDK 自带名字。名字由 MAP/PDB 的原名和位置匹配确认；没有把整个 PDB 的类型、参数或局部变量还原成原始源码。
可读名字保留类/函数信息，并加 Xbox 地址后缀区分重载和别名冲突。地址仍是配置和函数注册的定位依据。
装饰名如 `?GetRect@TextObj@Lib2D@@UBAXAAURect@2@@Z` 是微软 C++ 对类、参数等信息的编码，不是乱码。

本项目通过本地索引把五件事连接起来：

`Xbox 地址 → MAP 原始名字/对象文件 → PDB 名字与位置 → 可读名字 → 生成 C++ 文件及行号`

例如 `0x82786D78` 对应 `Lib2D::TextObj::GetRect`，来自 `2DGrpLib:TextObj.obj`，
生成函数为 `Lib2D_TextObj_GetRect_82786D78`。PDB 公共符号的装饰名、section:offset 均与 MAP 相同，
PDB 内部函数名是 `Lib2D::TextObj::GetRect`。
`0x8233AB90` 则对应 `XapiFiberSwapContext`，来自 `xapilib:fibera.obj`。

## 2026-10-01 核对结果

| 检查 | 数量与结论 |
| --- | --- |
| MAP 中标记为函数的记录 | 41,717 条；包含编译器辅助符号及别名 |
| MAP 不同函数入口地址 | 34,638 个 |
| 有多个 MAP 名字的地址 | 1,679 个；不能强行选一个名字代表所有含义 |
| PDB 公共符号记录 | 42,423 条 |
| MAP/PDB 公共名字及 section:offset 完全一致 | 27,842 条 |
| 公共符号有同一位置、未找到同一原始名字 | 16 条；不能直接判为同名函数 |
| 公共符号没有同一位置 | 13,859 条；进一步核对模块函数/标签 |
| PDB 模块函数/标签与 MAP 原始名字及位置相同 | 14,467 条；与公共匹配集合有重叠，不能相加 |
| 当前读取的公共/模块函数/标签均未找到同一位置 | 789 条 MAP 记录，仍待调查 |
| 分析修复的 44 个入口 | 全部与 PDB 公共名字及位置匹配；包括两个地址上的别名 |

PDB：GUID `26463ca1-88c8-48df-85e2-a7e8c0eb4d8d`，age `2`，DBI age `2`，
machine `0x01F2`（PowerPC FP）。PDB signature 为 `0x46134E73`，MAP 链接时间戳为
`0x461351C3`；两者本来就不保证相等，不能仅凭此认定不匹配。
EXE 是 XEX2，尚未从加载后的镜像核对 CodeView GUID/age；因此没有宣称 EXE/PDB 身份已严格认证。

内部函数记录常用可读名称，MAP 常用装饰名，所以“同位置、不同字符串”必须保留为独立状态。
脚本只读取 MSF 7.0、公共符号、模块 PROC32 和 LABEL32，不是完整 PDB 类型/变量解析器。
789 条未定位记录不等于 789 个损坏函数；需要检查原记录类型、库符号和节映射。

## 建立和查询索引

ReXGlue v0.10.0 支持在 `[functions]` 中按地址指定 `name`。名称工具只在 `map-functions.toml`、`runtime-functions.toml` 和 `catch-boundaries.toml` 的已有入口加入名字，保留边界和全部别名证据；索引工具负责查询，不改生成文件。

官方名称配置也会将函数设为 CONFIG 入口，而 PDATA 注册遇到已有入口会跳过。因此将普通函数全部作为名称配置导入，可能影响函数发现及异常元数据。这批只命名已有 CONFIG 入口；扩大范围前需要不改变发现/异常行为的名称注入方式。

从 Windows/Python 3.11+ 环境执行：

```powershell
python scripts/restore_function_names.py --check
```

新增已验证的显式入口后，执行 `python scripts/restore_function_names.py` 生成其名字，再按正常流程 codegen/build。工具验证镜像/MAP/PDB 哈希，要求原始名字与 section:offset 精确匹配，并使用 Windows DbgHelp 提取类/函数名。全部文件先验证后写入；自定义名字不会被覆盖。命名选择是确定的，完整别名仍保留在原件和索引中。

不要直接批量替换生成文件，也不能把函数名恢复当作参数和局部变量已恢复。Windows 名称检查测试可执行 `python -m unittest discover -s tests/symbols -v`。

命名前保留一份生成目录基线，codegen 后可验证整个生成源码只变了名字和声明排序：

```powershell
python scripts/check_function_renames.py --before "<基线目录>" --after generated/xacalite_scriptteam
```

该检查要求注册地址和源码文件集合相同，逐文件还原名称后比较全部 C++ 内容；头文件仅允许函数声明的排序变化，不放宽函数主体检查。实际构建与运行仍须另外验证。
生成 C++ 与完整开发镜像/MAP/PDB 已纳入仓库，维护者可以查看当前生成结果并重新生成。完整查询索引仍仅本地保存，可从仓库内 `assets/` 的同版 MAP/PDB 重建；上传原件不代表已恢复原始源码或全部参数名称。

需要 Python 3.11 或更新版本，仅使用标准库。Windows 上额外调用系统 DbgHelp 还原装饰名；
在其他平台或无法还原时保留原始名字，不猜测。
在仓库目录执行：

```powershell
python scripts/symbol_index.py build
python scripts/symbol_index.py query 82786D78
python scripts/symbol_index.py query TextObj
python scripts/symbol_index.py query XAUDIO --limit 10
```

输出到仓库同级的 `symbols/functions.csv` 和 `symbols/functions.summary.json`，仅本地保存。
CSV 保留原始名字、可读名字、对象文件、粗分组、PDB 状态、地址别名数和生成文件位置。
别名预览最多三个，不能把预览误认为完整名字集合；同地址的 MAP 别名各自保留为一行。
分组来自 MAP 的库/对象文件，不是已经确认的业务模块边界。

代码重新生成后文件编号和行号可能变化，必须刷新索引。
`scripts/Codegen.ps1` 在有 Python 与本地 MAP/PDB 时会自动刷新；缺少工具时明确提示。
索引通过生成注册表 `SetFunction(地址, 名字)` 与函数定义关联，支持新名字和 SDK 辅助函数，不从可读名字的后缀猜地址。没有生成函数体的原生导入仍不会被误报为生成定义；旧 `sub_ADDRESS` 格式也可查询。
MAP 中找不到独立生成函数体的地址不能直接判定遗漏，其中包含原生接入函数和其他符号记录。

## 维护定位规则

计划、修复说明和提交记录应同时写 Xbox 地址、原始/可读名字及验证证据。
有多个别名时注明共享地址，不覆盖其他名字。
PDB/MAP 没有确认的行为标为推测，使用生成代码和调用关系进一步验证。
函数长度不能由相邻 MAP 地址直接推断；此脚本不向分析配置写入猜测长度。
