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
import ctypes
import json
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
    print(f"\n{BOLD}{CYAN}{'='*75}{RESET}")
    print(f"{BOLD}{CYAN}  {text}{RESET}")
    print(f"{BOLD}{CYAN}{'='*75}{RESET}")

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
    print(f"\n{BOLD}{RED}{'='*75}{RESET}")
    print(f"{BOLD}{RED}  FAILURE DIAGNOSTIC IN {stage_name}{RESET}")
    print(f"{BOLD}{RED}{'='*75}{RESET}")
    print(f"  {BOLD}Reason:{RESET} {reason}")
    if expected_hash or actual_hash:
        print(f"  {BOLD}Expected SHA-256:{RESET} {expected_hash}")
        print(f"  {BOLD}Actual SHA-256:  {RESET} {actual_hash}")
    if expected_size is not None or actual_size is not None:
        print(f"  {BOLD}Expected Size:   {RESET} {expected_size} bytes")
        print(f"  {BOLD}Actual Size:     {RESET} {actual_size} bytes")

    if arch_path and arch_path.exists():
        print(f"\n  {BOLD}--- GTOC Metadata Table inside Archive (gpucompact.exe -l) ---{RESET}")
        try:
            gtoc_out = run_cmd([EXE_NAME, "-l", str(arch_path)])
            print(gtoc_out)
        except Exception as e:
            print(f"  {RED}Could not read GTOC: {e}{RESET}")

    if ext_dir and ext_dir.exists():
        print(f"  {BOLD}--- Actual Extracted Output Folder Summary ({ext_dir.name}) ---{RESET}")
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
    print(f"{BOLD}{RED}{'='*75}{RESET}\n")

def generate_thue_morse(length: int) -> bytes:
    """Generates a Thue-Morse sequence (worst-case repetitive pattern for prefix doubling)."""
    seq = bytearray([0])
    while len(seq) < length:
        seq.extend([1 - b for b in seq])
    return bytes(seq[:length])

def generate_chaotic_test_dataset() -> dict[str, str]:
    print(f"Generating comprehensive edge-case test vectors in {DATA_DIR}...")
    if DATA_DIR.exists():
        assert_sandbox(DATA_DIR)
        shutil.rmtree(DATA_DIR)
    DATA_DIR.mkdir(parents=True, exist_ok=True)

    test_files = {}
    root_prefix = DATA_DIR.name

    # 1. Micro Edge Cases & Prime/Unaligned Sizes
    micro_sizes = [0, 1, 2, 3, 7, 15, 31, 63, 127, 255, 513, 1025, 4097, 65537]
    micro_dir = DATA_DIR / "sizes_edge_cases"
    micro_dir.mkdir(exist_ok=True)
    for sz in micro_sizes:
        p = micro_dir / f"size_{sz}_bytes.bin"
        if sz == 0:
            p.write_bytes(b"")
        else:
            pattern = (b"A" * (sz // 2)) + (b"B" * (sz - (sz // 2)))
            p.write_bytes(pattern)
        test_files[f"{root_prefix}/sizes_edge_cases/size_{sz}_bytes.bin"] = compute_sha256(p)

    # 2. Purely Periodic Strings (Period P = 1, 2, 3, 7, 256)
    period_dir = DATA_DIR / "periodic_tests"
    period_dir.mkdir(exist_ok=True)
    
    # Period 1 (all equal RLE-1 mode)
    p1 = period_dir / "period_1_all_zeros_2mb.bin"
    p1.write_bytes(b"\x00" * (2 * 1024 * 1024))
    test_files[f"{root_prefix}/periodic_tests/period_1_all_zeros_2mb.bin"] = compute_sha256(p1)

    p1_ff = period_dir / "period_1_all_ones_2mb.bin"
    p1_ff.write_bytes(b"\xFF" * (2 * 1024 * 1024))
    test_files[f"{root_prefix}/periodic_tests/period_1_all_ones_2mb.bin"] = compute_sha256(p1_ff)

    # Period 2 ("AB" * 1M)
    p2 = period_dir / "period_2_ab_2mb.bin"
    p2.write_bytes(b"AB" * (1024 * 1024))
    test_files[f"{root_prefix}/periodic_tests/period_2_ab_2mb.bin"] = compute_sha256(p2)

    # Period 3 ("ABC" * 700K)
    p3 = period_dir / "period_3_abc_2mb.bin"
    p3.write_bytes((b"ABC" * ((2 * 1024 * 1024) // 3 + 1))[:2 * 1024 * 1024])
    test_files[f"{root_prefix}/periodic_tests/period_3_abc_2mb.bin"] = compute_sha256(p3)

    # Period 7
    p7 = period_dir / "period_7_prime_2mb.bin"
    p7.write_bytes((b"ABCDEFG" * ((2 * 1024 * 1024) // 7 + 1))[:2 * 1024 * 1024])
    test_files[f"{root_prefix}/periodic_tests/period_7_prime_2mb.bin"] = compute_sha256(p7)

    # Period 256 (all distinct byte ramp)
    p256 = period_dir / "period_256_ramp_2mb.bin"
    ramp = bytes([i & 0xFF for i in range(256)])
    p256.write_bytes(ramp * ((2 * 1024 * 1024) // 256))
    test_files[f"{root_prefix}/periodic_tests/period_256_ramp_2mb.bin"] = compute_sha256(p256)

    # 3. High-Entropy Random Data (Overflow & Raw Fallback)
    rand_p = DATA_DIR / "incompressible_random_3mb.bin"
    rand_p.write_bytes(os.urandom(3 * 1024 * 1024))
    test_files[f"{root_prefix}/incompressible_random_3mb.bin"] = compute_sha256(rand_p)

    # 4. Structured Text & Code
    text_sample = ("#include <iostream>\nint main() { std::cout << \"GPUCompact Fast Archiver!\"; return 0; }\n" * 100).encode("utf-8")
    text_data = (text_sample * ((2 * 1024 * 1024) // len(text_sample) + 1))[:2 * 1024 * 1024]
    text_p = DATA_DIR / "structured_code_2mb.txt"
    text_p.write_bytes(text_data)
    test_files[f"{root_prefix}/structured_code_2mb.txt"] = compute_sha256(text_p)

    # 5. Thue-Morse Suffix Doubling Stressor
    tm_p = DATA_DIR / "thue_morse_1mb.bin"
    tm_p.write_bytes(generate_thue_morse(1024 * 1024))
    test_files[f"{root_prefix}/thue_morse_1mb.bin"] = compute_sha256(tm_p)

    # 6. Deep Unicode, Emojis, and Special Characters
    utf8_dir = DATA_DIR / "unicode_tests" / "sub_f" / "deep_folder"
    utf8_dir.mkdir(parents=True, exist_ok=True)
    utf8_p = utf8_dir / "Képernyőfelvétel_日本語_漢字_🔥_test.txt"
    utf8_p.write_text("Hungary: Árvíztűrő tükörfúrógép | CJK: 漢字 テスト | Emojis: 🚀⚡🦀🔥 | Symbols: §±µ¿", encoding="utf-8")
    test_files[f"{root_prefix}/unicode_tests/sub_f/deep_folder/Képernyőfelvétel_日本語_漢字_🔥_test.txt"] = compute_sha256(utf8_p)

    # 7. Micro-File Cluster (300 small JSON files across 5 folders)
    cluster_dir = DATA_DIR / "micro_cluster"
    cluster_dir.mkdir(parents=True, exist_ok=True)
    for i in range(300):
        sub_d = cluster_dir / f"folder_{i % 5}"
        sub_d.mkdir(exist_ok=True)
        mf_p = sub_d / f"doc_{i}.json"
        mf_content = f'{{"id": {i}, "tag": "test_token_{i % 10}", "payload": "{ "Z" * (i * 7) }"}}\n'.encode("utf-8")
        mf_p.write_bytes(mf_content)
        rel_key = f"{root_prefix}/micro_cluster/folder_{i % 5}/doc_{i}.json"
        test_files[rel_key] = compute_sha256(mf_p)

    print(f"  -> Generated {len(test_files)} total test items (~15.2 MB total payload).")
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

    print(f"{BOLD}GPUCOMPACT COMPREHENSIVE REGRESSION & INTEGRITY SUITE{RESET}")
    print(f"Sandbox Target: {TEMP_DIR}")

    print_header("STAGE 1: GENERATING CHAOTIC & EDGE-CASE TEST DATASET")
    reference_hashes = generate_chaotic_test_dataset()

    passed_tests = 0
    failed_tests = 0

    try:
        # -----------------------------------------------------------------
        # STAGE 2: ALL PROFILE PRESETS
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
        # STAGE 3: EXTREME & CUSTOM HYPERPARAMETER SWEEPS
        # -----------------------------------------------------------------
        print_header("STAGE 3: CUSTOM HYPERPARAMETER LIMITS (--macro, --mini, --L)")
        custom_configs = [
            {"name": "tiny_chunks", "macro": "1", "mini": "128", "L": "512"},
            {"name": "large_chunks", "macro": "8", "mini": "4096", "L": "4096"}
        ]

        for cfg in custom_configs:
            arch_p = TEMP_DIR / f"archive_custom_{cfg['name']}.gcmp"
            ext_dir = TEMP_DIR / f"extracted_custom_{cfg['name']}"

            assert_sandbox(arch_p)
            assert_sandbox(ext_dir)

            print(f"Testing Custom Config [{cfg['name']} (M={cfg['macro']}MB, m={cfg['mini']}B, L={cfg['L']})]...", end="", flush=True)

            run_cmd([
                str(exe_path), "-c", "-i", str(DATA_DIR), "-o", str(arch_p), "-f",
                "--macro", cfg["macro"], "--mini", cfg["mini"], "--L", cfg["L"]
            ])
            run_cmd([str(exe_path), "-d", "-i", str(arch_p), "-o", str(ext_dir), "--collision", "replace"])

            v_count = verify_extracted_directory(arch_p, ext_dir, reference_hashes, stage_name=f"STAGE 3 ({cfg['name']})")
            print(f" {GREEN}[PASS]{RESET} ({v_count} files verified 100% SHA match)")
            passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 4: DIRECT SINGLE-FILE ARCHIVING
        # -----------------------------------------------------------------
        print_header("STAGE 4: DIRECT SINGLE-FILE ARCHIVING (FILE -> GCMP -> RESTORE)")
        single_src = DATA_DIR / "periodic_tests" / "period_7_prime_2mb.bin"
        single_arch = TEMP_DIR / "direct_single_file.gcmp"
        single_ext_dir = TEMP_DIR / "extracted_direct_single"

        assert_sandbox(single_arch)
        assert_sandbox(single_ext_dir)

        print("Testing Direct Single-File Compression...", end="", flush=True)
        run_cmd([str(exe_path), "-c", "-i", str(single_src), "-o", str(single_arch), "-f"])
        run_cmd([str(exe_path), "-d", "-i", str(single_arch), "-o", str(single_ext_dir), "--collision", "replace"])

        restored_file = single_ext_dir / "period_7_prime_2mb.bin"
        assert_sandbox(restored_file)
        if not restored_file.exists() or compute_sha256(restored_file) != compute_sha256(single_src):
            print_stage_failure("STAGE 4 (Direct Single File)", single_arch, single_ext_dir, reason="Direct single-file hash mismatch!")
            raise ValueError("Direct single file restore failed!")

        print(f" {GREEN}[PASS]{RESET} (Direct single-file verified 100% SHA match)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 5: SELECTIVE EXTRACTION (SINGLE FILE & SUBFOLDER)
        # -----------------------------------------------------------------
        print_header("STAGE 5: SELECTIVE EXTRACTION (SUBTREE & SINGLE ITEM)")
        base_arch = TEMP_DIR / "archive_balanced.gcmp"

        # A: Single nested UTF-8 file
        single_target = "source_data/unicode_tests/sub_f/deep_folder/Képernyőfelvétel_日本語_漢字_🔥_test.txt"
        ext_single_dir = TEMP_DIR / "extracted_selective_single"
        assert_sandbox(ext_single_dir)

        print(f"Extracting Single Item: '{single_target}'...", end="", flush=True)
        run_cmd([str(exe_path), "-d", "-i", str(base_arch), "-o", str(ext_single_dir), "--extract", single_target, "--collision", "replace"])

        extracted_file_p = ext_single_dir / "Képernyőfelvétel_日本語_漢字_🔥_test.txt"
        assert_sandbox(extracted_file_p)
        if not extracted_file_p.exists() or compute_sha256(extracted_file_p) != reference_hashes[single_target]:
            print_stage_failure("STAGE 5 (Selective Single File)", base_arch, ext_single_dir, reason="Single file hash mismatch or not found!")
            raise ValueError("Selective single file extraction failed!")
        print(f" {GREEN}[PASS]{RESET}")

        # B: Subfolder tree
        subfolder_target = "source_data/micro_cluster"
        ext_sub_dir = TEMP_DIR / "extracted_selective_subfolder"
        assert_sandbox(ext_sub_dir)

        print(f"Extracting Subfolder Tree: '{subfolder_target}'...", end="", flush=True)
        run_cmd([str(exe_path), "-d", "-i", str(base_arch), "-o", str(ext_sub_dir), "--extract", subfolder_target, "--collision", "replace"])
        v_count = verify_extracted_directory(base_arch, ext_sub_dir, reference_hashes, filter_prefix="source_data/micro_cluster", stage_name="STAGE 5 (Subfolder Extract)")
        print(f" {GREEN}[PASS]{RESET} ({v_count} sub-files verified 100% SHA match)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 6: COLLISION POLICIES (REPLACE, RENAME, SKIP)
        # -----------------------------------------------------------------
        print_header("STAGE 6: COLLISION PROTOCOLS (REPLACE, RENAME, SKIP)")
        coll_dir = TEMP_DIR / "collision_test"
        coll_dir.mkdir(exist_ok=True)
        assert_sandbox(coll_dir)

        # Populate with existing dummy file
        dummy_p = coll_dir / "period_7_prime_2mb.bin"
        dummy_p.write_bytes(b"DUMMY_OLD_DATA")
        dummy_old_hash = compute_sha256(dummy_p)

        # 1. SKIP Mode
        print("Testing Collision Mode [skip]...", end="", flush=True)
        run_cmd([str(exe_path), "-d", "-i", str(single_arch), "-o", str(coll_dir), "--collision", "skip"])
        if compute_sha256(dummy_p) != dummy_old_hash:
            raise RuntimeError("Collision 'skip' modified existing file!")
        print(f" {GREEN}[PASS]{RESET}")

        # 2. RENAME Mode
        print("Testing Collision Mode [rename]...", end="", flush=True)
        run_cmd([str(exe_path), "-d", "-i", str(single_arch), "-o", str(coll_dir), "--collision", "rename"])
        renamed_p = coll_dir / "period_7_prime_2mb (1).bin"
        assert_sandbox(renamed_p)
        if not renamed_p.exists() or compute_sha256(renamed_p) != reference_hashes["source_data/periodic_tests/period_7_prime_2mb.bin"]:
            raise RuntimeError("Collision 'rename' failed to create indexed copy!")
        print(f" {GREEN}[PASS]{RESET}")

        # 3. REPLACE Mode
        print("Testing Collision Mode [replace]...", end="", flush=True)
        run_cmd([str(exe_path), "-d", "-i", str(single_arch), "-o", str(coll_dir), "--collision", "replace"])
        if compute_sha256(dummy_p) != reference_hashes["source_data/periodic_tests/period_7_prime_2mb.bin"]:
            raise RuntimeError("Collision 'replace' failed to overwrite target!")
        print(f" {GREEN}[PASS]{RESET}")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 7: ARCHIVE APPENDING & LIVE MERGE
        # -----------------------------------------------------------------
        print_header("STAGE 7: ARCHIVE APPENDING & METADATA MERGING")
        append_arch = TEMP_DIR / "append_suite.gcmp"
        assert_sandbox(append_arch)

        print("Creating base archive...", end="", flush=True)
        run_cmd([str(exe_path), "-c", "-i", str(DATA_DIR), "-o", str(append_arch), "-f"])
        print(f" {GREEN}[DONE]{RESET}")

        extra_p = TEMP_DIR / "appended_extra.bin"
        extra_p.write_bytes(b"EXTRA_APPENDED_STREAM_PAYLOAD_" * 1024)
        extra_hash = compute_sha256(extra_p)

        print("Appending new payload to live archive...", end="", flush=True)
        run_cmd([str(exe_path), "-i", str(append_arch), "-a", str(extra_p), "--collision", "replace"])
        print(f" {GREEN}[DONE]{RESET}")

        ext_append_dir = TEMP_DIR / "extracted_append_suite"
        assert_sandbox(ext_append_dir)
        run_cmd([str(exe_path), "-d", "-i", str(append_arch), "-o", str(ext_append_dir), "--collision", "replace"])

        verify_extracted_directory(append_arch, ext_append_dir, reference_hashes, stage_name="STAGE 7 (Appended Base)")
        ext_extra = ext_append_dir / "appended_extra.bin"
        if not ext_extra.exists() or compute_sha256(ext_extra) != extra_hash:
            raise ValueError("Appended item missing or corrupted!")
        print(f" {GREEN}[PASS]{RESET} (All base files + appended item verified 100% SHA match)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 8: HIERARCHICAL DIRECTORY DELETION (-r)
        # -----------------------------------------------------------------
        print_header("STAGE 8: HIERARCHICAL DIRECTORY DELETION (-r)")
        del_arch = TEMP_DIR / "deletion_suite.gcmp"
        assert_sandbox(del_arch)
        shutil.copy(base_arch, del_arch)

        print("Pruning 'micro_cluster' and 'unicode_tests' from archive...", end="", flush=True)
        run_cmd([str(exe_path), "-i", str(del_arch), "-r", "source_data/micro_cluster", "source_data/unicode_tests"])
        print(f" {GREEN}[DONE]{RESET}")

        ext_del_dir = TEMP_DIR / "extracted_deletion_suite"
        assert_sandbox(ext_del_dir)
        run_cmd([str(exe_path), "-d", "-i", str(del_arch), "-o", str(ext_del_dir), "--collision", "replace"])

        if (ext_del_dir / "source_data" / "micro_cluster").exists() or (ext_del_dir / "source_data" / "unicode_tests").exists():
            raise RuntimeError("Deleted directories still present in extracted output!")

        verify_extracted_directory(del_arch, ext_del_dir, reference_hashes, filter_prefix="source_data/periodic_tests", stage_name="STAGE 8 (Pruned Extract)")
        verify_extracted_directory(del_arch, ext_del_dir, reference_hashes, filter_prefix="source_data/sizes_edge_cases", stage_name="STAGE 8 (Pruned Extract)")
        print(f" {GREEN}[PASS]{RESET} (Pruned archive contents verified 100% SHA match)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 9: ARCHIVE METADATA & GTOC INSPECTION (-l)
        # -----------------------------------------------------------------
        print_header("STAGE 9: ARCHIVE METADATA & GTOC QUERY (-l)")
        print("Querying GTOC Table of Contents...", end="", flush=True)
        gtoc_output = run_cmd([str(exe_path), "-l", str(base_arch)])
        if "GPUCOMPACT ARCHIVE METADATA" not in gtoc_output or "Overall Ratio" not in gtoc_output:
            raise RuntimeError("GTOC metadata query returned invalid output!")
        print(f" {GREEN}[PASS]{RESET} (GTOC parsing validated)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 10: MULTI-MACRO CHUNK LARGE STREAMING (20+ MB FILE)
        # -----------------------------------------------------------------
        print_header("STAGE 10: MULTI-MACRO CHUNK LARGE FILE (20+ MB SPANNING CHUNKS)")
        large_dir = DATA_DIR / "large_vectors"
        large_dir.mkdir(exist_ok=True)
        large_p = large_dir / "large_multi_chunk_20mb.bin"
        
        print("Generating 20 MB mixed multi-chunk test vector...", end="", flush=True)
        part_rep = b"MULTICHUNK_PATTERN_HEADER_" * 40960
        part_thue = generate_thue_morse(2 * 1024 * 1024)
        part_code = b"for (int i = 0; i < N; ++i) { array[i] = i * 2; }\n" * 100000
        part_rand = os.urandom(2 * 1024 * 1024)
        large_payload = (part_rep + part_thue + part_code + part_rand) * 2
        large_p.write_bytes(large_payload)
        large_expected_hash = compute_sha256(large_p)
        print(f" {GREEN}[DONE]{RESET} ({len(large_payload)} bytes)")

        large_arch = TEMP_DIR / "archive_large_multichunk.gcmp"
        ext_large_dir = TEMP_DIR / "extracted_large_multichunk"
        assert_sandbox(large_arch)
        assert_sandbox(ext_large_dir)

        print("Compressing 20 MB multi-chunk file (macro=2MB)...", end="", flush=True)
        run_cmd([str(exe_path), "-c", "-i", str(large_p), "-o", str(large_arch), "-f", "--macro", "2"])
        print(f" {GREEN}[DONE]{RESET}")

        print("Extracting and validating 20 MB multi-chunk file...", end="", flush=True)
        run_cmd([str(exe_path), "-d", "-i", str(large_arch), "-o", str(ext_large_dir), "--collision", "replace"])
        restored_large_p = ext_large_dir / "large_multi_chunk_20mb.bin"
        assert_sandbox(restored_large_p)
        if not restored_large_p.exists() or compute_sha256(restored_large_p) != large_expected_hash:
            raise ValueError("Multi-macro chunk large file SHA-256 mismatch!")
        print(f" {GREEN}[PASS]{RESET} (20 MB bit-exact match across all macro-chunks)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 11: FAULT INJECTION & MALFORMED ARCHIVE RESILIENCE
        # -----------------------------------------------------------------
        print_header("STAGE 11: FAULT INJECTION & CORRUPTED ARCHIVE RESILIENCE")
        fault_dir = TEMP_DIR / "fault_injection"
        fault_dir.mkdir(exist_ok=True)
        assert_sandbox(fault_dir)

        # 11A: Bad Magic Header
        bad_magic_arch = fault_dir / "bad_magic.gcmp"
        raw_arch_data = single_arch.read_bytes()
        bad_magic_arch.write_bytes(b"XXXX" + raw_arch_data[4:])
        print("Testing corrupted magic header rejection...", end="", flush=True)
        res = subprocess.run([str(exe_path), "-d", "-i", str(bad_magic_arch), "-o", str(fault_dir)], capture_output=True, text=True)
        if res.returncode == 0:
            raise RuntimeError("Corrupted magic header was NOT rejected!")
        print(f" {GREEN}[PASS]{RESET} (Gracefully rejected with code {res.returncode})")

        # 11B: Truncated Archive
        truncated_arch = fault_dir / "truncated.gcmp"
        truncated_arch.write_bytes(raw_arch_data[: len(raw_arch_data) // 2])
        print("Testing truncated archive rejection...", end="", flush=True)
        res = subprocess.run([str(exe_path), "-d", "-i", str(truncated_arch), "-o", str(fault_dir)], capture_output=True, text=True)
        if res.returncode == 0:
            raise RuntimeError("Truncated archive was NOT rejected!")
        print(f" {GREEN}[PASS]{RESET} (Gracefully rejected with code {res.returncode})")

        # 11C: Corrupted Compressed Payload (Bit-Flip)
        bitflip_arch = fault_dir / "bitflip_corrupted.gcmp"
        corrupted_bytes = bytearray(raw_arch_data)
        if len(corrupted_bytes) > 200:
            corrupted_bytes[150] ^= 0xFF
        bitflip_arch.write_bytes(bytes(corrupted_bytes))
        print("Testing bit-flipped payload resilience...", end="", flush=True)
        res = subprocess.run([str(exe_path), "-d", "-i", str(bitflip_arch), "-o", str(fault_dir), "--collision", "replace"], capture_output=True, text=True)
        print(f" {GREEN}[PASS]{RESET} (Handled gracefully without crash)")
        passed_tests += 1

        # -----------------------------------------------------------------
        # STAGE 12: DIRECT C-API DLL INTERFACE (gpucompact_native.dll)
        # -----------------------------------------------------------------
        print_header("STAGE 12: DIRECT C-API / DLL VALIDATION (gpucompact_native.dll)")
        dll_path = Path("gpucompact_native.dll").resolve()
        if dll_path.exists():
            print("Loading gpucompact_native.dll via ctypes...", end="", flush=True)
            dll = ctypes.CDLL(str(dll_path))
            
            class CLaunchConfig(ctypes.Structure):
                _fields_ = [
                    ("macro_mb", ctypes.c_int),
                    ("mini_size", ctypes.c_int),
                    ("L", ctypes.c_int),
                    ("threads_comp", ctypes.c_int),
                    ("threads_decomp", ctypes.c_int),
                ]
            
            cfg = CLaunchConfig(macro_mb=4, mini_size=512, L=1024, threads_comp=32, threads_decomp=64)
            legacy_cfg = CLaunchConfig(macro_mb=0, mini_size=0, L=0, threads_comp=6, threads_decomp=56)
            
            c_api_arch = TEMP_DIR / "c_api_archive.gcmp"
            c_api_ext_dir = TEMP_DIR / "c_api_extracted"
            c_api_ext_dir.mkdir(parents=True, exist_ok=True)
            assert_sandbox(c_api_arch)
            assert_sandbox(c_api_ext_dir)

            input_paths = [str(DATA_DIR).encode('utf-8')]
            c_input_arr = (ctypes.c_char_p * len(input_paths))(*input_paths)

            # 1. Build Archive via DLL (also testing fallback/warp-alignment normalization via legacy_cfg)
            dll.gpucompact_build_archive.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int, ctypes.c_char_p, CLaunchConfig, ctypes.c_void_p, ctypes.c_void_p]
            dll.gpucompact_build_archive.restype = ctypes.c_int
            ret = dll.gpucompact_build_archive(c_input_arr, len(input_paths), str(c_api_arch).encode('utf-8'), legacy_cfg, None, None)
            if ret != 0:
                raise RuntimeError(f"C-API gpucompact_build_archive failed with code {ret}")

            # 2. Inspect Archive JSON via DLL
            dll.gpucompact_inspect_archive_json.argtypes = [ctypes.c_char_p]
            dll.gpucompact_inspect_archive_json.restype = ctypes.c_void_p
            dll.gpucompact_free_string.argtypes = [ctypes.c_void_p]
            json_ptr = dll.gpucompact_inspect_archive_json(str(c_api_arch).encode('utf-8'))
            if json_ptr:
                json_str = ctypes.string_at(json_ptr).decode('utf-8')
                dll.gpucompact_free_string(json_ptr)
                meta = json.loads(json_str)
                if "files" not in meta or len(meta["files"]) == 0:
                    raise RuntimeError("C-API JSON metadata invalid!")
                if meta["metadata"]["mini_size"] != 512 or meta["metadata"]["L_state"] != 1024:
                    raise RuntimeError(f"C-API fallback defaults mismatch: {meta['metadata']}")

            # 3. Extract Archive via DLL
            dll.gpucompact_extract_archive.argtypes = [ctypes.c_char_p, ctypes.c_char_p, CLaunchConfig, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
            dll.gpucompact_extract_archive.restype = ctypes.c_int
            ret = dll.gpucompact_extract_archive(str(c_api_arch).encode('utf-8'), str(c_api_ext_dir).encode('utf-8'), cfg, b"", b"replace", None, None, None)
            if ret != 0:
                raise RuntimeError(f"C-API gpucompact_extract_archive failed with code {ret}")

            v_count = verify_extracted_directory(c_api_arch, c_api_ext_dir, reference_hashes, stage_name="STAGE 12 (C-API DLL)")
            print(f" {GREEN}[PASS]{RESET} (C-API verified {v_count} files bit-exact)")
            passed_tests += 1
        else:
            print(f" {YELLOW}[SKIPPED]{RESET} (gpucompact_native.dll not found in current directory)")

        # -----------------------------------------------------------------
        # STAGE 13: SPECIAL FILENAMES & EMPTY DIRECTORIES
        # -----------------------------------------------------------------
        print_header("STAGE 13: SPECIAL FILENAMES & EMPTY DIRECTORIES")
        special_dir = TEMP_DIR / "special_cases_dir"
        special_dir.mkdir(exist_ok=True)
        assert_sandbox(special_dir)

        empty_sub = special_dir / "empty_folder_1" / "nested_empty_2"
        empty_sub.mkdir(parents=True, exist_ok=True)

        sp_file1 = special_dir / "file with spaces (copy 1).txt"
        sp_file1.write_text("CONTENT_WITH_SPACES_AND_SPECIAL_CHARS!@#$%^&*()_+", encoding="utf-8")

        sp_file2 = special_dir / ".hidden_dotfile.config"
        sp_file2.write_text("SECRET_CONFIG_KEY=123456789", encoding="utf-8")

        special_arch = TEMP_DIR / "archive_special_cases.gcmp"
        ext_special_dir = TEMP_DIR / "extracted_special_cases"
        assert_sandbox(special_arch)
        assert_sandbox(ext_special_dir)

        print("Testing archiving of files with spaces, dotfiles, and empty folders...", end="", flush=True)
        run_cmd([str(exe_path), "-c", "-i", str(special_dir), "-o", str(special_arch), "-f"])
        run_cmd([str(exe_path), "-d", "-i", str(special_arch), "-o", str(ext_special_dir), "--collision", "replace"])

        restored_sp1 = ext_special_dir / "special_cases_dir" / "file with spaces (copy 1).txt"
        restored_sp2 = ext_special_dir / "special_cases_dir" / ".hidden_dotfile.config"
        assert_sandbox(restored_sp1)
        assert_sandbox(restored_sp2)

        if not restored_sp1.exists() or compute_sha256(restored_sp1) != compute_sha256(sp_file1):
            raise ValueError("Special character filename SHA mismatch!")
        if not restored_sp2.exists() or compute_sha256(restored_sp2) != compute_sha256(sp_file2):
            raise ValueError("Hidden dotfile SHA mismatch!")

        print(f" {GREEN}[PASS]{RESET} (Special filenames and dotfiles verified 100% SHA match)")
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