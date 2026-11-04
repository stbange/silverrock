#!/usr/bin/env python3
"""Layer dependency check: a module may include only headers from its own or lower layers, and
only platform/, render/gl/ and app/ may include SDL or OpenGL headers.

Layers (low to high): core < world < sim < input < game < render < ui < render/gl < platform < app
"""
import os
import re
import sys

ORDER = ["core", "world", "sim", "input", "game", "render", "ui", "render/gl", "platform", "app"]
ALLOWED_SYSTEM = {"render/gl", "platform", "app"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
FORBIDDEN = ("SDL", "GL/", "GLES", "EGL", "emscripten")


def module_of(path):
    rel = os.path.relpath(path, "src").replace(os.sep, "/")
    if rel.startswith("render/gl/"):
        return "render/gl"
    return rel.split("/")[0]


def main(root="."):
    os.chdir(root)
    errors = []
    for base, _, files in os.walk("src"):
        for name in files:
            if not name.endswith((".c", ".h")):
                continue
            path = os.path.join(base, name)
            mod = module_of(path)
            level = ORDER.index(mod)
            with open(path, encoding="utf-8") as f:
                for no, line in enumerate(f, 1):
                    m = INCLUDE.match(line)
                    if not m:
                        continue
                    inc = m.group(1)
                    if inc.startswith(FORBIDDEN) and mod not in ALLOWED_SYSTEM:
                        errors.append(f"{path}:{no}: {mod} may not include {inc}")
                        continue
                    if "/" in inc and not inc.startswith(FORBIDDEN):
                        target = "render/gl" if inc.startswith("render/gl/") else inc.split("/")[0]
                        if target in ORDER and ORDER.index(target) > level:
                            errors.append(f"{path}:{no}: {mod} may not include higher layer {inc}")
    for e in errors:
        print(e)
    print(f"layer check: {len(errors)} violation(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
