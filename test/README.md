# Test Samples

This directory keeps the **source** test samples under version control.

The generated `.pyc` files under `__pycache__/` are intentionally **not**
tracked in git.

To generate local bytecode files for drag-and-drop testing:

```bash
python test/compile_test_samples.py
```

That will compile every `*.py` file in this directory into `test/__pycache__/`.
