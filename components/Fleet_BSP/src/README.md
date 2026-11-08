# Why this (otherwise empty) directory exists

`Fleet_BSP` is header-only: everything lives in `../include/`. This directory holds no
source and never will — but it must exist, or the build fails with
`fatal error: bsp_loader.h: No such file or directory`.

`Fleet_BSP` ships a `library.properties`, so PlatformIO's LDF classifies it with
`ArduinoLibBuilder`. That class registers a library's `include/` directory *only if
`include/` and `src/` both exist* (`platformio/builder/tools/piolib.py`, in
`ArduinoLibBuilder.include_dir`). With no `src/`, it returns `None`, only the library
root is added to the include path, and neither the compiler nor VSCode IntelliSense
can find `bsp_loader.h`.

Git does not track empty directories, so a bare `src/` would not survive a clone.
This README exists to keep the directory tracked. Do not delete it.

An alternative is a `library.json` with `"build": {"includeDir": "include"}`, which routes
to `PlatformIOLibBuilder` instead. It is not used here, to avoid two manifests in one
library.
