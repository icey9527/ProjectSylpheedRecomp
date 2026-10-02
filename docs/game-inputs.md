# 游戏输入

当前代码与地址配置对应开发镜像 `Xacalite_ScriptTeam.exe`。仓库包含宿主源码、分析配置和重建工具；不包含游戏或符号原件。克隆仓库后仍需自行准备匹配输入。

## 文件与位置

```text
workspace/
  assets/
    Xacalite_ScriptTeam.exe
    Xacalite_ScriptTeam.map
    Xacalite_ScriptTeam.pdb
  repo/
```

| 文件 | 用途 | 必需场景 |
| --- | --- | --- |
| `Xacalite_ScriptTeam.exe` | codegen 与运行时镜像数据 | 生成、运行 |
| `Xacalite_ScriptTeam.map` | 函数地址、名称、对象归属 | 重建符号索引、分析入口 |
| `Xacalite_ScriptTeam.pdb` | 公共和模块函数/标签位置核对 | 重建符号索引、符号调查 |
| 资源根 `config.ini` 和完整 `dat/` | 游戏配置与资源包 | 运行 |

已有提交的入口/边界配置可用于代码生成；不做符号分析时 MAP/PDB 可缺省。只有三份分析输入仍不足以运行游戏，运行资源可以放在独立目录，并由宿主 TOML 指向。

从干净源码树仅提供匹配镜像，可以完成代码生成与 CMake 配置；`generated/rexglue.cmake` 也由官方 codegen 重建。MAP/PDB 用于进一步调查，并非使用已提交修复配置的构建前提。编译还需要完整 SDK 和工具链，运行还需要完整资源。

## 匹配检查

`config/game-inputs.json` 记录已使用三份文件的大小、SHA-256，以及 PDB GUID/age。执行：

```powershell
python scripts/verify_game_inputs.py
python scripts/verify_game_inputs.py --image-only
python scripts/verify_game_inputs.py --input-dir "<输入目录>"
```

默认检查全部三份文件；`--image-only` 只检查镜像。缺失或哈希不符时退出非零，并显示对应文件。该命令只读文件，不复制或修改输入。

哈希匹配说明文件与已记录输入一致。MAP/PDB 的名称和位置已有比对，但加载镜像的 CodeView GUID/age 尚未与 PDB 完整核对，不能据此宣称 EXE/PDB 身份认证已完成。

零售版/试玩版的同名或改名文件不能替代当前输入。重新支持其他镜像时，应独立记录输入身份、manifest 和补丁地址。
