# pycdc-studio

[English README](./README.md)

一个基于 Qt Widgets 的桌面图形界面，用来配合 `pycdc` / `pycdas` 浏览 Python 字节码、查看原生反编译结果，并在遇到不支持的 code object 时使用 AI 做兜底重建。

## 功能

- 直接打开 `.pyc` / `.pyo`
- 支持把文件夹拖进窗口，递归扫描其中的字节码文件
- 多文件会一起显示在左侧树里
- 查看模块、类、函数、lambda、推导式等 code object 树
- 对比三种结果：
  - `Merged`
  - `Native`
  - `AI`
- 查看每个节点的元数据和反汇编结果
- 预览发送给 AI fallback 的完整 prompt
- 通过醒目的 `Retry with AI` 按钮或 `Ctrl+R` 对当前节点做 AI 重建

## 当前界面结构

- 左侧：code object 树
- 中间：`Merged / Native / AI` 源码视图
- 右侧：`Disassembly / Metadata / Prompt / Log`

## AI fallback 是怎么做的

程序默认**不会把整个 `.pyc` 文件直接发给 AI**。

它会基于当前选中的 code object 组织 prompt，通常包含：

- qualified name
- object type
- names / locals / consts 预览
- native error
- disassembly

这样做的好处是：

- token 更省
- 噪声更少
- AI 更容易聚焦当前失败节点，而不是整个文件

## pycdc 集成方式

当前会优先查找和程序本体放在同一目录下的可执行文件：

- `pycdc.exe`
- `pycdas.exe`

也就是 `pycdc-studio.exe` 所在目录。

也可以用环境变量覆盖：

- `PYCDC_STUDIO_PYCDC`
- `PYCDC_STUDIO_PYCDAS`

## Windows 发布工作流

仓库里已经带了一份 GitHub Actions workflow，会自动：

- 在 `windows-latest` 上安装 Qt
- 准备 MSVC 工具链，并通过 `jurplel/install-qt-action` 安装 Qt
- 拉取官方上游 `pycdc` 仓库
- 编译 `pycdc-studio`、`pycdc` 和 `pycdas`
- 打包成 Windows zip
- 在 `THIRD_PARTY_NOTICES.txt` 里写入当前打包使用的 `pycdc` 上游仓库和 commit
- 在推送 `v*` tag 时把 Windows zip 自动发布到 GitHub Releases

## AI 配置

在使用 AI fallback 之前，需要先在 `设置` 中至少配置：

- `Base URL`
- `API Key`
- `Model`

当前接入的是 **兼容 OpenAI 的 API**。

设置页会通过 `QSettings` 保存 AI 配置。  
如果某个字段为空，则回退到环境变量。

支持的环境变量：

- `PYCDC_STUDIO_AI_BASE_URL`
- `PYCDC_STUDIO_AI_API_KEY`
- `PYCDC_STUDIO_AI_MODEL`
- `PYCDC_STUDIO_AI_SYSTEM_PROMPT`

## 构建

依赖：

- Qt 6 Widgets
- Qt Network
- CMake
- C++17 编译器

示例：

```bash
cmake -S . -B build
cmake --build build
```

## 使用方式

1. 启动 `pycdc-studio`
2. 打开一个 `.pyc` / `.pyo`，或者直接把文件夹拖进窗口
3. 如果要使用 AI fallback，请先通过菜单栏顶层的 `设置` 配置 AI 模型
4. 在左侧树中选择文件或某个 code object
5. 查看 native 结果、反汇编、元数据和 prompt
6. 当原生反编译不完整或不正确时，对当前节点点击 `Retry with AI`

## 测试样例

`test/` 目录下已经带了一些较长的测试样例：

- `workflow_orchestrator.py`
- `async_batch_runner.py`
- `plugin_config_resolver.py`
- 以及其他较小的样例

仓库里默认只跟踪这些 **源码样例**。

如果你要生成本地拖拽测试用的 `.pyc`，可以运行：

```bash
python test/compile_test_samples.py
```

生成结果会写到 `test/__pycache__/`。

## 说明

这个项目目前仍然是实验性质。

当前 `Merged` 视图是比较诚实的“混合结果”：

- 能原生反编译的部分显示 native source
- AI 重建部分会以 patch 的形式附加在后面

它目前**还不是**真正的 inline source merger。

## 许可证

这个项目会配合 `pycdc` / `pycdas` 使用，而它们的上游项目采用 **GPL-3.0** 许可证。

- `pycdc`：GPL-3.0
- 如果和 `pycdc` / `pycdas` 一起分发，应同时附带对应的 GPL-3.0 许可证说明
- 当前 release workflow 会在 `THIRD_PARTY_NOTICES.txt` 中记录打包所使用的 `pycdc` 上游仓库和 commit

上游项目地址：
- [zrax/pycdc](https://github.com/zrax/pycdc)
