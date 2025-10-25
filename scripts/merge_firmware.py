import os
import sys
import subprocess
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()

def post_build_action(source, target, env):
    # Resolve paths properly
    build_dir = os.path.abspath(env.subst("$BUILD_DIR"))
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware = os.path.join(build_dir, "firmware.bin")
    factory = os.path.join(build_dir, "factory.bin")

    # Path to PlatformIO's bundled esptool
    esptool_path = os.path.join(
        env.PioPlatform().get_package_dir("tool-esptoolpy"),
        "esptool.py"
    )

    # Print debug info
    print("🔧 Merging images into factory.bin ...")
    print(f"Using esptool at: {esptool_path}")
    print(f"Bootloader: {bootloader}")
    print(f"Partitions: {partitions}")
    print(f"Firmware:   {firmware}")

    # Construct full command
    cmd = [
        sys.executable, esptool_path,
        "--chip", "esp32s3",
        "merge_bin",
        "-o", factory,
        "0x0000", bootloader,
        "0x8000", partitions,
        "0x10000", firmware
    ]

    # Run merge
    subprocess.run(cmd, check=True)
    print(f"✅ Successfully created {factory}")

# Register post-build action
env.AddPostAction("$BUILD_DIR/firmware.bin", post_build_action)
