import os
import sys
import subprocess
import tempfile
from pathlib import Path

# Usage: python spirv_generate_interface.py <file_name>.glsl
# Compiles the interface GLSL to SPIR-V, reflects it, and outputs a .hpp
# to interfaces/headers/

_WIN = sys.platform == "win32"

GLSLC_PATH = os.environ.get(
    "GLSLC_PATH",
    r"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" if _WIN else "glslc",
)
REFLECT_PATH = os.environ.get(
    "SPIRV_REFLECT_PATH",
    r"build\Debug\bin\spirv_reflect_tool.exe" if _WIN else "build/Debug/bin/spirv_reflect_tool",
)
CLANG_FORMAT_PATH = os.environ.get(
    "CLANG_FORMAT_PATH",
    r"C:\Program Files\LLVM\bin\clang-format.exe" if _WIN else "clang-format",
)

IFACE_DIR = Path("lumina/data/static/shaders/interfaces")
OUT_DIR = IFACE_DIR / "headers"

STAGE_MAP = {
    ".vert": "vertex",
    ".frag": "fragment",
    ".global": "vertex",
}


def detect_stage(glsl_path: Path) -> str:
    # e.g. standard_lit.vert.glsl -> strip .glsl -> standard_lit.vert -> ext = .vert
    inner = glsl_path.stem  # strips .glsl
    stage = STAGE_MAP.get(Path(inner).suffix)
    if stage is None:
        print(f"Error: Could not determine shader stage from '{glsl_path.name}'")
        print("File must contain .vert., .frag., or .global. in its name.")
        sys.exit(1)
    return stage


def write_temp_wrapper(tmp_dir: Path, include_path: str, base_name: str, shader_stage: str) -> Path:
    # include_path is relative to IFACE_DIR (e.g. "stages/lit_mesh.vert.glsl"),
    # because that is the one directory passed to glslc as -I. Using the bare
    # filename would only resolve for interfaces sitting at the root.
    tmp_file = tmp_dir / f"{base_name}.glsl.reflect"
    stub = "void main() { gl_Position = vec4(0.0); }" if shader_stage == "vertex" else "void main() {}"
    tmp_file.write_text(
        f'#version 450\n#include "{include_path}"\n{stub}\n',
        encoding="utf-8",
    )
    return tmp_file


def main():
    if len(sys.argv) != 2:
        print("Usage: python spirv_generate_interface.py <path/to/file>.glsl")
        print("  path may be relative to the interfaces dir or to the repo root")
        print("Example: python spirv_generate_interface.py stages/lit_mesh.vert.glsl")
        sys.exit(1)

    glsl_path = Path(sys.argv[1])
    if glsl_path.suffix != ".glsl":
        print(f"Error: Expected a .glsl file, got '{glsl_path}'")
        sys.exit(1)

    # Accept either form: "stages/lit_mesh.vert.glsl" or the full path from the
    # repo root. Everything downstream needs the path relative to IFACE_DIR,
    # since that is what -I resolves against.
    if (IFACE_DIR / glsl_path).is_file():
        include_path = glsl_path.as_posix()
    elif glsl_path.is_file():
        include_path = glsl_path.resolve().relative_to(IFACE_DIR.resolve()).as_posix()
    else:
        print(f"Error: '{glsl_path}' not found under {IFACE_DIR} or as given")
        sys.exit(1)

    base_name = Path(include_path).stem  # e.g. lit_mesh.vert
    shader_stage = detect_stage(Path(include_path))

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    tmp_dir = Path(tempfile.gettempdir()) / "lumina_reflect_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    tmp_file = write_temp_wrapper(tmp_dir, include_path, base_name, shader_stage)
    spv_file = tmp_dir / f"{base_name}.spv"
    out_header = OUT_DIR / f"{base_name}.hpp"

    try:
        # Compile to SPIR-V with -O0 to prevent dead-code removal
        result = subprocess.run(
            [
                GLSLC_PATH,
                f"-fshader-stage={shader_stage}",
                "-O0",
                "-I", str(IFACE_DIR),
                "-o", str(spv_file),
                str(tmp_file),
            ],
            check=False,
        )
        if result.returncode != 0:
            print(f"Error: GLSL compilation failed for '{glsl_file}'")
            sys.exit(1)

        # Reflect SPIR-V to generate the .hpp header
        result = subprocess.run(
            [REFLECT_PATH, str(spv_file), str(out_header)],
            check=False,
        )
        if result.returncode != 0:
            print(f"Error: SPIR-V reflection failed for '{glsl_file}'")
            sys.exit(1)

    finally:
        tmp_file.unlink(missing_ok=True)
        spv_file.unlink(missing_ok=True)

    # Format the generated header
    subprocess.run([CLANG_FORMAT_PATH, "-i", str(out_header)], check=False)

    print(f"Generated {out_header}")


if __name__ == "__main__":
    main()
