# 打包资源与开发版启动

资源可分为散文件和 PAK。宿主启动检查只核对开发镜像、根 `config.ini` 和 `dat/`，不要求散装 `dat/files.tbl`。文件夹通过检查并不代表包内内容完整或可玩。

原游戏加载器已支持 PAK。`BaseLib::OpenFile`（`0x82816C80`）先处理文件别名，再调用 `ParseFileName`（`0x82816038`）；后者用 `+` 分隔包路径和内部路径，例如 `dat\tables.pak+eng\GP_TITLE.tbl`。带包路径时调用 `PakManager::get`（`0x82816818`）和 `PakFile::OpenInternalFile`（`0x8281FBE0`），普通路径走 `NormalFile::CreateInstance`（`0x82815720`）。这些函数属 `BaseLib:LoadFile.obj`，OpenInternalFile 属 `BaseLib:PakFile.obj`；MAP/PDB 公共名字和地址一致，模块位置一致。生成代码保留这条路线；不需要在宿主重新实现整个 PAK 解包器。

因此，支持 PAK 和自动从任意散文件路径回退到对应包是两件事。已检查的开发版普通路径分支没有在打开失败后自动重试 `tables.pak+files.tbl`；包内部引用需要使用相应路径或别名。也未验证“散文件始终覆盖包”的通用优先级。

正式资源包实际有 `files.tbl`。以原 `MakeEntryID`（`0x82827208`，小写规范化）及 `MakeStrID`（`0x82817A48`）算法核对，`files.tbl` 的 ID 为 `0x83421153`；对应 tables.pak 条目位于 tables.p00 偏移 `106496`、长度 `1771`，Z1/zlib 解压为 `5768` 字节 `IDXD` 二进制表。表内含字体与各语言表的 `包+文件` 路径；它和开发资源的文本表格式不同，不能仅根据字节编码判断损坏。这两个 ID 函数分别属 `BaseLib:FileEntryManager.obj` 和 `BaseLib:MakeID.obj`，公共名字/地址一致、模块位置一致。

一次开发镜像搭配正式资源的 45 秒 Release 诊断记录到：

- 镜像加载成功，执行游戏主线程。
- 原游戏请求 `game:\dat\files.tbl`，返回 `0xC000000F`，内部 `BaseLib::LoadFile` 报打开失败。
- 随后进入开发版 `GP_TEST`，请求 `game:\dat\GP_TEST\eng\GP_TEST.tbl`，同样失败。正式资源根 INI 的默认语言是 `eng`。
- 音频与部分通用 XPR 继续初始化，但有部分资源注册失败；时限内没有捕获目标异常，测试被诊断器结束。没有完整游戏行为验收。

当前证据指向启动表未路由到包及后续开发调试资源缺失，不是已确认的中文路径或文本编码错误。下一步应保留散表路线，并在散表缺失时针对启动表接入原 PAK 路径，再核对包内入口和所需调试资源；不能靠改名零售 EXE、复制开发散表或忽略加载错误宣称兼容。此诊断没有切换主程序，也没有开启零售版重编译。
