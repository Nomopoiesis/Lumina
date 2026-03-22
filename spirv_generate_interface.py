import os
import sys
import subprocess
import tempfile
from pathlib import Path

# Usage: python spirv_generate_interface.py <file_name>.glsl
# Compiles the interface GLSL to SPIR-V, reflects it, and outputs a .hpp
# to interfaces/headers/

GLSLC_PATH = os.environ.get("GLSLC_PATH", r"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe")
REFLECT_PATH = os.environ.get("SPIRV_REFLECT_PATH", r"build\Debug\bin\spirv_reflect_tool.exe")
CLANG_FORMAT_PATH = os.environ.get("CLANG_FORMAT_PATH", r"C:\Program Files\LLVM\bin\clang-format.exe")

IFACE_DIR = Path("lumina/data/shaders/interfaces")
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


def write_temp_wrapper(tmp_dir: Path, glsl_file: str, shader_stage: str) -> Path:
    tmp_file = tmp_dir / f"{glsl_file}.reflect"
    stub = "void main() { gl_Position = vec4(0.0); }" if shader_stage == "vertex" else "void main() {}"
    tmp_file.write_text(
        f'#version 450\n#include "{glsl_file}"\n{stub}\n',
        encoding="utf-8",
    )
    return tmp_file


def main():
    if len(sys.argv) != 2:
        print("Usage: python spirv_generate_interface.py <file_name>.glsl")
        print("Example: python spirv_generate_interface.py standard_lit.vert.glsl")
        sys.exit(1)

    glsl_path = Path(sys.argv[1])
    if glsl_path.suffix != ".glsl":
        print(f"Error: Expected a .glsl file, got '{glsl_path}'")
        sys.exit(1)

    glsl_file = glsl_path.name          # e.g. simple_model_input.vert.glsl
    base_name = glsl_path.stem          # e.g. simple_model_input.vert
    shader_stage = detect_stage(glsl_path)

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    tmp_dir = Path(tempfile.gettempdir()) / "lumina_reflect_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    tmp_file = write_temp_wrapper(tmp_dir, glsl_file, shader_stage)
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
