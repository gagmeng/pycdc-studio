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

The app currently prefers local executables and will look for:

- `D:/code/pycdc/Release/pycdc.exe`
- `D:/code/pycdc/Release/pycdas.exe`

You can override them with environment variables:

- `PYCDC_STUDIO_PYCDC`
- `PYCDC_STUDIO_PYCDAS`

## AI configuration

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
3. Select a file or code object in the tree
4. Inspect native output, disassembly, metadata, and prompt context
5. Use `Retry with AI` on the selected node if native decompilation is incomplete or wrong

## Test samples

Longer test cases are included under `test/`:

- `workflow_orchestrator.py`
- `async_batch_runner.py`
- `plugin_config_resolver.py`
- plus several smaller samples

Compiled bytecode samples are generated under `test/__pycache__/` for drag-and-drop testing.

## Notes

This project is still experimental.

The current `Merged` view is an honest mixed document:
- native source when available
- AI fallback patches appended per code object

It is not yet a full inline source merger.
