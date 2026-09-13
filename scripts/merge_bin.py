import os
import shutil
Import("env")

def merge_bin_action(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    chip = env.BoardConfig().get("build.mcu", "esp32c3")
    flash_size = env.BoardConfig().get("upload.flash_size", "4MB")
    flash_mode = env.BoardConfig().get("board_build.flash_mode", "dio")
    flash_freq_raw = str(env.BoardConfig().get("board_build.f_flash", "80000000L")).rstrip("L")
    try:
        flash_freq = f"{int(int(flash_freq_raw) / 1000000)}m"
    except ValueError:
        flash_freq = "80m"

    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    otadata = os.path.join(build_dir, "ota_data_initial.bin")
    firmware = os.path.join(build_dir, "firmware.bin")
    factory = os.path.join(build_dir, "firmware-factory.bin")

    # Pruefen, ob alle Teil-Binaries vorhanden sind
    required_files = [bootloader, partitions, otadata, firmware]
    for file_path in required_files:
        if not os.path.isfile(file_path):
            print(f"[merge_bin] Warning: Missing required binary for factory image: {file_path}")
            return

    # Pfad zu esptool.py ermitteln
    python_exe = env.subst("$PYTHONEXE")
    esptool_path = env.subst("$OBJCOPY")
    if not (esptool_path and os.path.isfile(esptool_path) and esptool_path.endswith(".py")):
        try:
            pkg_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")
            if pkg_dir and os.path.isfile(os.path.join(pkg_dir, "esptool.py")):
                esptool_path = os.path.join(pkg_dir, "esptool.py")
        except Exception:
            pass

    cmd = [
        f'"{python_exe}"',
        f'"{esptool_path}"',
        "--chip", chip,
        "merge_bin",
        "-o", f'"{factory}"',
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size,
        "0x0", f'"{bootloader}"',
        "0x8000", f'"{partitions}"',
        "0xe000", f'"{otadata}"',
        "0x10000", f'"{firmware}"'
    ]

    print(f"\n==================== [MERGE BIN] Generating Factory Image ====================")
    print(f"Target: {factory}")
    print(f"Chip: {chip} | Mode: {flash_mode} | Freq: {flash_freq} | Size: {flash_size}")
    cmd_str = " ".join(cmd)
    result = env.Execute(cmd_str)
    if result == 0:
        print(f"[merge_bin] Successfully created factory binary: {factory}")
        
        # Automatisch in docs/ fuer den lokalen Web-Flasher kopieren
        project_dir = env.subst("$PROJECT_DIR")
        docs_dir = os.path.join(project_dir, "docs")
        if os.path.isdir(docs_dir):
            pio_env = env.subst("$PIOENV")
            docs_target = os.path.join(docs_dir, f"firmware-{pio_env}-factory.bin")
            shutil.copyfile(factory, docs_target)
            print(f"[merge_bin] Copied to docs for web flasher: {docs_target}\n")
    else:
        print(f"[merge_bin] Error generating factory binary (exit code: {result})\n")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)