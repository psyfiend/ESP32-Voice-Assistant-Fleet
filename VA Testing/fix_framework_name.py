import os
import shutil
from pathlib import Path

Import("env")

def fix_framework_script(source, target, env):
    # 1. Get the path to the framework package
    # We try both names just in case
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    if not framework_dir:
        framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs")
    
    if not framework_dir:
        print("FIX SCRIPT: Could not find framework dir!")
        return

    tools_dir = Path(framework_dir) / "tools"
    
    # 2. Define the two filenames
    file_pio = tools_dir / "platformio-build.py"
    file_arduino = tools_dir / "pioarduino-build.py"

    # 3. Check what we have and what we might need
    # If pioarduino-build.py exists but platformio-build.py is missing, copy it over.
    # This satisfies builders looking for the old name.
    if file_arduino.exists() and not file_pio.exists():
        print(f"FIX SCRIPT: Patching {file_arduino} -> {file_pio}")
        shutil.copy(file_arduino, file_pio)

    # If platformio-build.py exists but pioarduino-build.py is missing, copy it over.
    # This satisfies builders looking for the new name.
    if file_pio.exists() and not file_arduino.exists():
        print(f"FIX SCRIPT: Patching {file_pio} -> {file_arduino}")
        shutil.copy(file_pio, file_arduino)

# Run this check extremely early
env.AddPreAction("upload", fix_framework_script)
env.AddPreAction("buildprog", fix_framework_script)
# Also run it immediately on import so it fixes it before the builder crashes
fix_framework_script(None, None, env)