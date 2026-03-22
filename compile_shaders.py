import os
import subprocess
from pathlib import Path


GLSLC_PATH = os.environ.get("GLSLC_PATH", "glslc")
SPV_DIR = Path("lumina/data/runtime/shaders")
SHADER_INTERFACE_DIR = Path("lumina/data/static/shaders/interfaces")
SHADER_SRC_DIR = Path("lumina/data/static/shaders/src")
CWD = Path.cwd()

def compile_shaders():
    print("Compiling shaders...")
    src_dir = CWD / SHADER_SRC_DIR
    out_dir = CWD / SPV_DIR
    interface_dir = CWD / SHADER_INTERFACE_DIR
    out_dir.mkdir(parents=True, exist_ok=True)

    for shader_file in src_dir.iterdir():
        if not shader_file.is_file():
            continue

        stage = None
        if shader_file.suffix == ".frag":
            stage = "fragment"
        elif shader_file.suffix == ".vert":
            stage = "vertex"

        if stage is None:
            continue

        print(f"Compiling shader: {shader_file.name}")
        output_file = out_dir / f"{shader_file.name}.spv"
        command = [
            GLSLC_PATH,
            f"-fshader-stage={stage}",
            "-I",
            str(interface_dir),
            "-o",
            str(output_file),
            str(shader_file),
        ]
        subprocess.run(command, check=True)

if __name__ == "__main__":
    compile_shaders()
    print("Shaders compiled successfully!")