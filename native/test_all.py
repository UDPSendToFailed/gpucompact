# GPUCompact
# Copyright (C) 2026 UDPSendToFailed
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
import os
import sys
import shutil
import hashlib
import subprocess
from pathlib import Path

# ANSI Formatting
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
BOLD = "\033[1m"
RESET = "\033[0m"

EXE_NAME = "gpucompact.exe"
TEMP_DIR = Path("./temp_test_run").resolve()
DATA_DIR = (TEMP_DIR / "source_data").resolve()

def print_header(text):
    print(f"\n{BOLD}{CYAN}{'='*70}{RESET}")
    print(f"{BOLD}{CYAN}  {text}{RESET}")
    print(f"{BOLD}{CYAN}{'='*70}{RESET}")

def assert_sandbox(target_path: Path):
    """Safety Guard: Guarantees operations never escape TEMP_DIR."""
    resolved = target_path.resolve()
    try:
        resolved.relative_to(TEMP_DIR)
    except ValueError:
        raise SecurityError(f"CRITICAL SAFETY TRAP: Attempted operation outside sandbox! Target: {resolved}")

class SecurityError(Exception):
    pass

def compute_sha256(file_path: Path) -> str:
    assert_sandbox(file_path)
    hasher = hashlib.sha256()
    with open(file_path, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    return hasher.hexdigest().upper()

def run_cmd(cmd_args):
    res = subprocess.run(cmd_args, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if res.returncode != 0:
        print(f"\n{RED}[COMMAND ERROR] Command failed:{RESET} {' '.join(cmd_args)}")
        print(f"{RED}STDOUT:{RESET}\n{res.stdout}")
        print(f"{RED}STDERR:{RESET}\n{res.stderr}")
        raise RuntimeError(f"Command exit code {res.returncode}")
    return res.stdout

def print_stage_failure(stage_name: str, arch_path: Path, ext_dir: Path, reason: str, 
                        expected_hash: str = None, actual_hash: str = None, 
                        expected_size: int = None, actual_size: int = None):
    """Prints a non-spammy, highly detailed diagnostic breakdown when a test fails."""
    print(f"\n{BOLD}{RED}{'='*70}{RESET}")
    print(f"{BOLD}{RED}  FAILURE DIAGNOSTIC IN {stage_name}{RESET}")
    print(f"{BOLD}{RED}{'='*70}{RESET}")
    print(f"  {BOLD}Reason:{RESET} {reason}")
    if expected_hash or actual_hash:
        print(f"  {BOLD}Expected SHA-256:{RESET} {expected_hash}")
        print(f"  {BOLD}Actual SHA-256:  {RESET} {actual_hash}")
    if expected_size is not None or actual_size is not None:
        print(f"  {BOLD}Expected Size:   {RESET} {expected_size} bytes")
        print(f"  {BOLD}Actual Size:     {RESET} {actual_size} bytes")

    # 1. Print Archive Listing from GTOC
    print(f"\n  {BOLD}--- GTOC Metadata Table inside Archive (gpucompact.exe -l) ---{RESET}")
    try:
        gtoc_out = run_cmd([EXE_NAME, "-l", str(arch_path)])
        print(gtoc_out)
    except Exception as e:
        print(f"  {RED}Could not read GTOC: {e}{RESET}")

    # 2. Print Summary of Extracted Directory
    print(f"  {BOLD}--- Actual Extracted Output Folder Summary ({ext_dir.name}) ---{RESET}")
    if ext_dir.exists():
        files = [p for p in ext_dir.rglob("*") if p.is_file()]
        if files:
            print(f"  Extracted contains {len(files)} file(s). First 10 items:")
            for p in files[:10]:
                rel = p.relative_to(ext_dir).as_posix()
                print(f"    • {rel} ({p.stat().st_size} bytes)")
            if len(files) > 10:
                print(f"    ... and {len(files) - 10} more files.")
        else:
            print(f"  {YELLOW}Directory exists but contains ZERO files!{RESET}")
    else:
        print(f"  {RED}Extracted directory DOES NOT EXIST on disk!{RESET}")

    print(f"{BOLD}{RED}{'='*70}{RESET}\n")

def generate_chaotic_test_dataset() -> dict[str, str]:
    print(f"Generating test vectors in {DATA_DIR}...")
    if DATA_DIR.exists():
        assert_sandbox(DATA_DIR)
        shutil.rmtree(DATA_DIR)
    DATA_DIR.mkdir(parents=True, exist_ok=True)

    test_files = {}
    root_prefix = DATA_DIR.name

    empty_p = DATA_DIR / "0_empty.bin"
    empty_p.write_bytes(b"")
    test_files[f"{root_prefix}/0_empty.bin"] = compute_sha256(empty_p)

    one_p = DATA_DIR / "1_byte.bin"
    one_p.write_bytes(b"\x42")
    test_files[f"{root_prefix}/1_byte.bin"] = compute_sha256(one_p)

    zeros_p = DATA_DIR / "2mb_zeros.bin"
    zeros_p.write_bytes(b"\x00" * (2 * 1024 * 1024))
    test_files[f"{root_prefix}/2mb_zeros.bin"] = compute_sha256(zeros_p)

    ones_p = DATA_DIR / "2mb_ones.bin"
    ones_p.write_bytes(b"\xFF" * (2 * 1024 * 1024))
    test_files[f"{root_prefix}/2mb_ones.bin"] = compute_sha256(ones_p)

    rand_p = DATA_DIR / "2mb_random.bin"
    rand_p.write_bytes(os.urandom(2 * 1024 * 1024))
    test_files[f"{root_prefix}/2mb_random.bin"] = compute_sha256(rand_p)

    text_sample = ("#include <iostream>\nint main() { std::cout << \"GPUCompact Test Stream!\"; return 0; }\n" * 100).encode("utf-8")
    text_data = (text_sample * ((2 * 1024 * 1024) // len(text_sample) + 1))[:2 * 1024 * 1024]
    text_p = DATA_DIR / "2mb_code.txt"
    text_p.write_bytes(text_data)
    test_files[f"{root_prefix}/2mb_code.txt"] = compute_sha256(text_p)

    utf8_dir = DATA_DIR / "utf8_test" / "sub_f"
    utf8_dir.mkdir(parents=True, exist_ok=True)
    utf8_p = utf8_dir / "Képernyőfelvétel_日本語_test.txt"
    utf8_p.write_text("Hungary: Árvíztűrő tükörfúrógép | CJK: 漢字 テスト | Universal UTF-8 Validation", encoding="utf-8")
    test_files[f"{root_prefix}/utf8_test/sub_f/Képernyőfelvétel_日本語_test.txt"] = compute_sha256(utf8_p)

    micro_dir = DATA_DIR / "micro_cluster"
    micro_dir.mkdir(parents=True, exist_ok=True)
    for i in range(200):
        sub_d = micro_dir / f"folder_{i % 5}"
        sub_d.mkdir(exist_ok=True)
        mf_p = sub_d / f"micro_file_{i}.json"
        mf_content = f'{{"id": {i}, "data": "{ "X" * (i * 10) }"}}\n'.encode("utf-8")
        mf_p.write_bytes(mf_content)
        rel_key = f"{root_prefix}/micro_cluster/folder_{i % 5}/micro_file_{i}.json"
        test_files[rel_key] = compute_sha256(mf_p)

    print(f"  -> Generated {len(test_files)} test items (~8.5 MB total payload).")
    return test_files

def verify_extracted_directory(arch_p: Path, extracted_dir: Path, expected_hashes: dict[str, str], filter_prefix: str = "", stage_name: str = "EXTRACT") -> int:
    assert_sandbox(extracted_dir)
    verified_count = 0
    for rel_path, expected_hash in expected_hashes.items():
        if filter_prefix and not rel_path.startswith(filter_prefix):
            continue

        target_p = extracted_dir / Path(rel_path)
        assert_sandbox(target_p)
        if not target_p.exists():
            print_stage_failure(stage_name, arch_p, extracted_dir, 
                                reason=f"Extracted file is MISSING on disk: {rel_path}", 
                                expected_hash=expected_hash, actual_hash="FILE_NOT_FOUND")
            raise FileNotFoundError(f"Missing extracted file: {target_p}")

        actual_hash = compute_sha256(target_p)
        if actual_hash != expected_hash:
            print_stage_failure(stage_name, arch_p, extracted_dir, 
                                reason=f"Extracted file SHA-256 MISMATCH: {rel_path}", 
                                expected_hash=expected_hash, actual_hash=actual_hash,
                                expected_size=target_p.stat().st_size, actual_size=target_p.stat().st_size)
            raise ValueError(f"SHA-256 MISMATCH for {rel_path}!")

        verified_count += 1
    return verified_count

def main():
    exe_path = Path(EXE_NAME).resolve()
    if not exe_path.exists():
        print(f"{RED}[FATAL ERROR] {EXE_NAME} not found in current directory!{RESET}")
        sys.exit(1)

    print(f"{BOLD}GPUCOMPACT AUTOMATED REGRESSION & INTEGRITY TEST SUITE{RESET}")
    print(f"Sandbox Target: {TEMP_DIR}")

    print_header("STAGE 1: GENERATING CHAOTIC TEST DATASET")
    reference_hashes = generate_chaotic_test_dataset()

    passed_tests = 0
    failed_tests = 0

    try:
        # -----------------------------------------------------------------
        # STAGE 2: ALL PROFILES
        # -----------------------------------------------------------------
        print_header("STAGE 2: TESTING ALL PROFILE PRESETS (COMPRESS -> EXTRACT -> SHA)")
        profiles = ["best_speed", "speed", "balanced", "ratio", "best_ratio"]

        for prof in profiles:
            arch_p = TEMP_DIR / f"archive_{prof}.gcmp"
            ext_dir = TEMP_DIR / f"extracted_{prof}"

            assert_sandbox(arch_p)
            assert_sandbox(ext_dir)

            print(f"Testing Profile [{prof}]...", end="", flush=True)

            run_cmd([str(exe_path), "-c", "-i", str(DATA_DIR), "-o", str(arch_p), "-f", "--profile", prof])
            run_cmd([str(exe_path), "-d", "-i", str(arch_p), "-o", str(ext_dir), "--collision", "replace"])

            v_count = verify_extracted_directory(arch_p, ext_dir, reference_hashes, stage_name=f"STAGE 2 ({prof})")
            print(f" {GREEN}[PASS]{RESET} ({v_count} files verified 100% SHA match)")
            passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 3: SELECTIVE SINGLE-FILE EXTRACTION
        # -----------------------------------------------------------------
        print_header("STAGE 3: SELECTIVE SINGLE-FILE EXTRACTION")
        single_target = "source_data/utf8_test/sub_f/Képernyőfelvétel_日本語_test.txt"
        arch_p = TEMP_DIR / "archive_balanced.gcmp"
        ext_single_dir = TEMP_DIR / "extracted_single_file"

        assert_sandbox(ext_single_dir)

        print(f"Extracting single nested item: '{single_target}'...", end="", flush=True)
        run_cmd([
            str(exe_path), "-d", "-i", str(arch_p), "-o", str(ext_single_dir),
            "--extract", single_target, "--collision", "replace"
        ])

        extracted_file_p = ext_single_dir / "Képernyőfelvétel_日本語_test.txt"
        assert_sandbox(extracted_file_p)
        if not extracted_file_p.exists():
            print_stage_failure("STAGE 3 (Selective Single File)", arch_p, ext_single_dir, 
                                reason=f"Single extracted file not found at {extracted_file_p}",
                                expected_hash=reference_hashes[single_target], actual_hash="FILE_NOT_FOUND")
            raise FileNotFoundError(f"Single extracted file not found at {extracted_file_p}")

        single_hash = compute_sha256(extracted_file_p)
        expected_hash = reference_hashes[single_target]
        if single_hash != expected_hash:
            print_stage_failure("STAGE 3 (Selective Single File)", arch_p, ext_single_dir,
                                reason=f"Single extracted file SHA-256 mismatch",
                                expected_hash=expected_hash, actual_hash=single_hash,
                                expected_size=100, actual_size=extracted_file_p.stat().st_size)
            raise ValueError("Single file SHA mismatch!")

        print(f" {GREEN}[PASS]{RESET} (Bit-exact SHA-256 match)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 4: SELECTIVE SUB-FOLDER EXTRACTION
        # -----------------------------------------------------------------
        print_header("STAGE 4: SELECTIVE SUB-FOLDER EXTRACTION")
        subfolder_target = "source_data/micro_cluster"
        ext_sub_dir = TEMP_DIR / "extracted_subfolder"

        assert_sandbox(ext_sub_dir)

        print(f"Extracting subfolder tree: '{subfolder_target}'...", end="", flush=True)
        run_cmd([
            str(exe_path), "-d", "-i", str(arch_p), "-o", str(ext_sub_dir),
            "--extract", subfolder_target, "--collision", "replace"
        ])

        v_count = verify_extracted_directory(arch_p, ext_sub_dir, reference_hashes, 
                                             filter_prefix="source_data/micro_cluster", stage_name="STAGE 4 (Subfolder Extract)")
        print(f" {GREEN}[PASS]{RESET} ({v_count} sub-files verified 100% SHA match)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 5: ARCHIVE APPENDING & COLLISION PROTOCOLS
        # -----------------------------------------------------------------
        print_header("STAGE 5: ARCHIVE APPENDING & COLLISION PROTOCOLS")
        append_arch = TEMP_DIR / "append_base.gcmp"
        assert_sandbox(append_arch)

        print("Creating base archive from folder...", end="", flush=True)
        run_cmd([str(exe_path), "-c", "-i", str(DATA_DIR), "-o", str(append_arch), "-f"])
        print(f" {GREEN}[DONE]{RESET}")

        extra_file = TEMP_DIR / "extra_test_file.txt"
        extra_file.write_text("Extra appended payload test data\n", encoding="utf-8")
        extra_hash = compute_sha256(extra_file)
        extra_size = extra_file.stat().st_size

        print("Appending new item...", end="", flush=True)
        run_cmd([str(exe_path), "-i", str(append_arch), "-a", str(extra_file), "--collision", "replace"])
        print(f" {GREEN}[DONE]{RESET}")

        ext_append_dir = TEMP_DIR / "extracted_append"
        assert_sandbox(ext_append_dir)
        print("Extracting appended archive...", end="", flush=True)
        run_cmd([str(exe_path), "-d", "-i", str(append_arch), "-o", str(ext_append_dir), "--collision", "replace"])

        # Verify all base files
        verify_extracted_directory(append_arch, ext_append_dir, reference_hashes, stage_name="STAGE 5 (Appended Base Files)")

        # Verify extra appended file
        ext_extra_p = ext_append_dir / "extra_test_file.txt"
        if not ext_extra_p.exists():
            print_stage_failure("STAGE 5 (Archive Appending)", append_arch, ext_append_dir,
                                reason=f"Appended file 'extra_test_file.txt' was NOT created on disk!",
                                expected_hash=extra_hash, actual_hash="FILE_NOT_FOUND",
                                expected_size=extra_size, actual_size=0)
            raise FileNotFoundError("Appended file missing from extracted output!")

        actual_extra_hash = compute_sha256(ext_extra_p)
        actual_extra_size = ext_extra_p.stat().st_size
        if actual_extra_hash != extra_hash:
            print_stage_failure("STAGE 5 (Archive Appending)", append_arch, ext_append_dir,
                                reason=f"Appended file 'extra_test_file.txt' SHA-256 mismatch!",
                                expected_hash=extra_hash, actual_hash=actual_extra_hash,
                                expected_size=extra_size, actual_size=actual_extra_size)
            raise ValueError("Appended file SHA mismatch!")

        print(f" {GREEN}[PASS]{RESET} (Appended archive contents verified 100% SHA match)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 6: HIERARCHICAL DIRECTORY DELETION (-r)
        # -----------------------------------------------------------------
        print_header("STAGE 6: HIERARCHICAL DIRECTORY DELETION (-r)")
        del_arch = TEMP_DIR / "deletion_test.gcmp"
        assert_sandbox(del_arch)
        shutil.copy(arch_p, del_arch)

        print("Deleting 'micro_cluster' and 'utf8_test' from archive...", end="", flush=True)
        run_cmd([str(exe_path), "-i", str(del_arch), "-r", "source_data/micro_cluster", "source_data/utf8_test"])
        print(f" {GREEN}[DONE]{RESET}")

        ext_del_dir = TEMP_DIR / "extracted_deletion"
        assert_sandbox(ext_del_dir)
        print("Extracting remaining files from pruned archive...", end="", flush=True)
        run_cmd([str(exe_path), "-d", "-i", str(del_arch), "-o", str(ext_del_dir), "--collision", "replace"])

        if (ext_del_dir / "source_data" / "micro_cluster").exists() or (ext_del_dir / "source_data" / "utf8_test").exists():
            print_stage_failure("STAGE 6 (Deletion)", del_arch, ext_del_dir, reason="Removed subdirectories still exist in extracted output!")
            raise RuntimeError("Deletion failed!")

        verify_extracted_directory(del_arch, ext_del_dir, reference_hashes, filter_prefix="source_data/2mb_zeros.bin", stage_name="STAGE 6 (Pruned Extract)")
        verify_extracted_directory(del_arch, ext_del_dir, reference_hashes, filter_prefix="source_data/2mb_random.bin", stage_name="STAGE 6 (Pruned Extract)")
        print(f" {GREEN}[PASS]{RESET} (Target directories removed cleanly, remaining files verified)")
        passed_tests += 1

    except Exception as e:
        print(f"\n{RED}{BOLD}[TEST FAILED] {e}{RESET}")
        failed_tests += 1

    if failed_tests == 0:
        print_header("CLEANUP & FINAL SUMMARY")
        if TEMP_DIR.exists():
            assert_sandbox(TEMP_DIR)
            try:
                shutil.rmtree(TEMP_DIR)
                print(f"Cleaned up sandbox directory {TEMP_DIR}")
            except Exception as e:
                print(f"Warning during cleanup: {e}")

        print(f"\n{BOLD}{GREEN}🏆 ALL {passed_tests} REGRESSION TESTS PASSED 100% BIT-EXACT!{RESET}\n")
        sys.exit(0)
    else:
        print(f"\n{BOLD}{RED}❌ REGRESSION SUITE FAILED ({failed_tests} failure(s) detected){RESET}")
        print(f"{YELLOW}SANDBOX PRESERVED FOR INSPECTION: {TEMP_DIR}{RESET}\n")
        sys.exit(1)

if __name__ == "__main__":
    main()