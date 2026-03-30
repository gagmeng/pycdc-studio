# pycdc-studio

[中文说明](./README_CN.md)

A Qt Widgets desktop UI for exploring Python bytecode with `pycdc` / `pycdas`, inspecting native decompilation results, and retrying unsupported code objects with AI fallback.

## What it does

- Open `.pyc` / `.pyo` files directly
- Drag a folder into the window and recursively discover supported bytecode files
- Show all discovered files together in the left tree
- Inspect code object trees for modules, classes, functions, lambdas, and comprehensions
- Compare:
  - `Merged`
  - `Native`
  - `AI`
- View per-node metadata and disassembly
- Preview the exact prompt sent to the AI fallback model
- Retry the currently selected node with AI using a prominent action button or `Ctrl+R`

## Current UI structure

- Left: code object tree
- Center: merged / native / AI source views
- Right: disassembly / metadata / prompt / log

## AI fallback

The app does not send the whole `.pyc` by default.

It builds a prompt from the currently selected code object, including:

- qualified name
- object type
- names / varnames / consts previews
- native error
- disassembly

That keeps fallback reconstruction focused on the selected node instead of the whole file.

## pycdc integration

The app currently prefers local executables next to the application binary and will look for:

- `pycdc.exe`
- `pycdas.exe`

in the same directory as `pycdc-studio.exe`.

You can override them with environment variables:

- `PYCDC_STUDIO_PYCDC`
- `PYCDC_STUDIO_PYCDAS`

## Windows release workflow

The repository includes a GitHub Actions workflow that:

- installs Qt on `windows-latest`
- prepares the MSVC toolchain and installs Qt with `jurplel/install-qt-action`
- clones the official upstream `pycdc` repository
- builds `pycdc-studio`, `pycdc`, and `pycdas`
- bundles everything into a Windows zip package
- writes the bundled `pycdc` upstream repository and commit into `THIRD_PARTY_NOTICES.txt`
- publishes the Windows zip to GitHub Releases for pushed `v*` tags

## AI configuration

Before using AI fallback, open `Settings` and configure at least:

- `Base URL`
- `API Key`
- `Model`

The current client expects an **OpenAI-compatible API** endpoint.

The settings dialog stores provider configuration with `QSettings` and falls back to environment variables when a field is empty.

Supported environment variables:

- `PYCDC_STUDIO_AI_BASE_URL`
- `PYCDC_STUDIO_AI_API_KEY`
- `PYCDC_STUDIO_AI_MODEL`
- `PYCDC_STUDIO_AI_SYSTEM_PROMPT`

## Build

Requirements:

- Qt 6 Widgets
- Qt Network
- CMake
- C++17 compiler

Example:

```bash
cmake -S . -B build
cmake --build build
```

## Usage

1. Launch `pycdc-studio`
2. Open a `.pyc` / `.pyo` file, or drag a folder into the window
3. Open `Settings` from the top-level menu and configure your AI model if you want to use AI fallback
4. Select a file or code object in the tree
5. Inspect native output, disassembly, metadata, and prompt context
6. Use `Retry with AI` on the selected node if native decompilation is incomplete or wrong

## Test samples

Longer test cases are included under `test/`:

- `workflow_orchestrator.py`
- `async_batch_runner.py`
- `plugin_config_resolver.py`
- plus several smaller samples

The repository tracks the **source** samples only.

To generate local `.pyc` files for drag-and-drop testing:

```bash
python test/compile_test_samples.py
```

That script writes the compiled bytecode into `test/__pycache__/`.

## Notes

This project is still experimental.

The current `Merged` view is an honest mixed document:
- native source when available
- AI fallback patches appended per code object

It is not yet a full inline source merger.

## License

This project integrates with `pycdc` / `pycdas`, whose upstream project is licensed under **GPL-3.0**.

- `pycdc`: GPL-3.0
- release packages that bundle `pycdc` / `pycdas` should include the GPL-3.0 notice
- release packages also record the bundled upstream `pycdc` repository and commit in `THIRD_PARTY_NOTICES.txt`

See the upstream project for full license details:
- [zrax/pycdc](https://github.com/zrax/pycdc)
