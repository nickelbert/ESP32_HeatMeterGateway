import os
import shutil
import struct
import hashlib
Import("env")

def fix_esptool_padding_bug(firmware_path):
    """
    Checks for a known esptool bug where a segment ending exactly on a 64KB boundary
    (remainder == 0) gets erroneously padded with 36 zero bytes (0x24 - remainder).
    This pushes the segment into a new 64KB MMU page, causing the bootloader and ESP-IDF
    MMU mappings to become desynchronized, leading to corrupt DROM/string accesses and bootloops.
    """
    if not os.path.isfile(firmware_path):
        return

    with open(firmware_path, "rb") as f:
        data = bytearray(f.read())

    if len(data) < 24 or data[0] != 0xe9:
        return

    seg_count = data[1]
    offset = 24
    segments = []
    fixed = False

    for i in range(seg_count):
        if offset + 8 > len(data):
            return
        load_addr, length = struct.unpack("<II", data[offset:offset+8])
        seg_data = data[offset+8 : offset+8+length]
        offset += 8 + length

        seg_end_in_file = offset
        if (seg_end_in_file % 0x10000 == 0x24) and len(seg_data) >= 36 and seg_data[-36:] == b"\x00" * 36:
            print(f"[merge_bin] Bug detected: Segment {i} (0x{load_addr:08x}) has 36-byte bogus esptool padding at file offset 0x{seg_end_in_file:x}.")
            print(f"[merge_bin] Stripping 36-byte padding to prevent MMU page desynchronization.")
            seg_data = seg_data[:-36]
            fixed = True

        segments.append((load_addr, seg_data))

    if not fixed:
        return

    # Reconstruct sanitized binary
    new_bin = bytearray()
    new_bin.extend(data[:24])

    calc_checksum = 0xef
    for load_addr, seg_data in segments:
        new_bin.extend(struct.pack("<II", load_addr, len(seg_data)))
        new_bin.extend(seg_data)
        for b in seg_data:
            calc_checksum ^= b

    pad_len = 15 - (len(new_bin) % 16)
    if pad_len < 0:
        pad_len += 16
    new_bin.extend(b"\x00" * pad_len)
    new_bin.append(calc_checksum)

    has_hash = (data[23] == 1) or (len(data) % 16 == 0 and len(data) >= len(new_bin) + 32)
    if has_hash:
        digest = hashlib.sha256(new_bin).digest()
        new_bin.extend(digest)

    with open(firmware_path, "wb") as f:
        f.write(new_bin)

    print(f"[merge_bin] Successfully sanitized firmware binary: {firmware_path} ({len(new_bin)} bytes)\n")


def merge_bin_action(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    chip = env.BoardConfig().get("build.mcu", "esp32c3")
    flash_size = env.BoardConfig().get("upload.flash_size", "4MB")
    try:
        flash_mode = env.GetProjectOption("board_build.flash_mode")
    except Exception:
        flash_mode = env.BoardConfig().get("build.flash_mode", "dio")

    try:
        flash_freq_raw = str(env.GetProjectOption("board_build.f_flash")).rstrip("L")
    except Exception:
        flash_freq_raw = str(env.BoardConfig().get("build.f_flash", "40000000L")).rstrip("L")
    try:
        flash_freq = f"{int(int(flash_freq_raw) / 1000000)}m"
    except ValueError:
        flash_freq = "40m"

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

    # Automatically fix esptool 36-byte padding bug if present
    fix_esptool_padding_bug(firmware)

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