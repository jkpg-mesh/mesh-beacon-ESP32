# This needs to be worked on and tested more thoroughly

import os
import sys
import subprocess
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()

def post_build_action(source, target, env):
    build_dir = os.path.abspath(env.subst("$BUILD_DIR"))
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    app = os.path.join(build_dir, "firmware.bin")
    factory = os.path.join(build_dir, "factory.bin")

    esptool_path = os.path.join(
        env.PioPlatform().get_package_dir("tool-esptoolpy"),
        "esptool.py"
    )
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")

    # Try to locate boot_app0.bin automatically
    candidates = [
        os.path.join(framework_dir, "tools", "boot_app0.bin"),
        os.path.join(framework_dir, "tools", "esp32", "boot_app0.bin"),
        os.path.join(framework_dir, "tools", "sdk", "boot_app0.bin"),
        os.path.join(framework_dir, "tools", "partitions", "boot_app0.bin")
    ]
    boot_app0 = next((f for f in candidates if os.path.exists(f)), None)

    if not boot_app0:
        raise FileNotFoundError("Could not find boot_app0.bin in Arduino-ESP32 framework tools folder")

    print("🔧 Merging images into factory.bin ...")
    print(f"Bootloader: {bootloader}")
    print(f"Boot App0:  {boot_app0}")
    print(f"Partitions: {partitions}")
    print(f"Firmware:   {app}")

    cmd = [
        sys.executable, esptool_path,
        "--chip", "esp32s3",
        "merge_bin",
        "-o", factory,
        "0x0000", bootloader,
        "0xE000", boot_app0,
        "0x8000", partitions,
        "0x10000", app
    ]

    subprocess.run(cmd, check=True)
    print(f"✅ Successfully created {factory}")

env.AddPostAction("$BUILD_DIR/firmware.bin", post_build_action)
