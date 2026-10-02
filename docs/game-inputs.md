# 游戏输入

当前代码与地址配置对应开发镜像。仓库 `assets/` 保存完整镜像、MAP 和 PDB，`generated/` 保存由该镜像生成的 C++、头文件及 CMake 描述。运行时使用宿主 EXE 旁的 `BaseLib.dll`；它仍是 XEX2，不是 Windows DLL。

## 文件与位置

```text
repo/
  assets/
    Xacalite_ScriptTeam.exe
    Xacalite_ScriptTeam.map
    Xacalite_ScriptTeam.pdb
  generated/
```

构建后，宿主 EXE 目录会自动得到同一镜像的副本 `BaseLib.dll`；仓库原件保留其原始文件名，供 codegen 和符号匹配使用。资源目录只放 `config.ini`、`dat/` 及游戏资源。

| 文件 | 用途 | 必需场景 |
| --- | --- | --- |
| `BaseLib.dll` | 宿主 EXE 旁的运行时开发镜像；内容必须与仓库输入一致 | 运行 |
| `assets/Xacalite_ScriptTeam.exe` | codegen 输入原件 | 生成、符号匹配 |
| `Xacalite_ScriptTeam.map` | 函数地址、名称、对象归属 | 重建符号索引、分析入口 |
| `Xacalite_ScriptTeam.pdb` | 公共和模块函数/标签位置核对 | 重建符号索引、符号调查 |
| 资源根 `config.ini` 和完整 `dat/` | 游戏配置与资源包 | 运行 |

已有提交的入口/边界配置可用于代码生成；不做符号分析时 MAP/PDB 可缺省。三份输入不足以运行游戏，完整游戏资源仍需另行准备，资源目录由宿主 TOML 指向。可用 `scripts/Prepare-ResourceTest.ps1` 将仓库镜像补到宿主 EXE 目录，资源目录保持不变。

克隆后已有生成源码，可直接用完整 SDK 和工具链构建；生成配置修改后仍须重新 codegen，并在同一提交更新相关生成文件。MAP/PDB 用于调查和重建查询索引；完整索引、codegen 缓存和构建产物不跟踪。

## 匹配检查

`config/game-inputs.json` 记录已使用三份文件的大小、SHA-256，以及 PDB GUID/age。执行：

```powershell
python scripts/verify_game_inputs.py
python scripts/verify_game_inputs.py --image-only
python scripts/verify_game_inputs.py --input-dir "<输入目录>"
```

默认检查仓库 `assets/` 的三份文件；`--image-only` 只检查镜像。缺失或哈希不符时退出非零，并显示对应文件。该命令只读文件，不复制或修改输入。

哈希匹配说明文件与已记录输入一致。MAP/PDB 的名称和位置已有比对，但加载镜像的 CodeView GUID/age 尚未与 PDB 完整核对，不能据此宣称 EXE/PDB 身份认证已完成。

零售版/试玩版的同名或改名文件不能替代当前输入。重新支持其他镜像时，应独立记录输入身份、manifest 和补丁地址。
