import argparse
import os
import shutil
import subprocess
from pathlib import Path


GLSLC_PATH = os.environ.get("GLSLC_PATH", "glslc")
SPV_DIR = Path("lumina/data/runtime/shaders")
SHADER_INTERFACE_DIR = Path("lumina/data/static/shaders/interfaces")
SHADER_SRC_DIR = Path("lumina/data/static/shaders/src")
CWD = Path.cwd()


def compile_shaders(copy_to=None):
    print("Compiling shaders...")
    src_dir = CWD / SHADER_SRC_DIR
    out_dir = CWD / SPV_DIR
    interface_dir = CWD / SHADER_INTERFACE_DIR
    out_dir.mkdir(parents=True, exist_ok=True)
    if copy_to is not None:
        copy_to.mkdir(parents=True, exist_ok=True)

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

        if copy_to is not None:
            destination = copy_to / output_file.name
            shutil.copy(output_file, destination)
            print(f"Deployed shader: {destination}")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compile Lumina's GLSL shaders to SPIR-V."
    )
    parser.add_argument(
        "--copy-to",
        type=Path,
        metavar="DIR",
        help=f"Additional directory to copy the compiled .spv files into, on "
        f"top of {SPV_DIR}. Point it at a build's shader directory "
        f"(e.g. build/Debug/bin/shaders) to update a running engine "
        f"without a rebuild.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    compile_shaders(args.copy_to)
    print("Shaders compiled successfully!")
