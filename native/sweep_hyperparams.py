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
import subprocess
import re
import sys
import itertools
from dataclasses import dataclass

@dataclass
class SweepResult:
    macro: int
    mini: int
    L: int
    ratio: float
    avg_c_wall: float
    avg_c_gpu: float
    avg_d_wall: float
    avg_d_gpu: float
    status: str

# Paths
EXE_PATH = r".\gpucompact.exe"
SILESIA_DIR = r"..\..\silesia"

# Hyperparameter Grid Search
# - macro: 1 to 16 MB
# - mini: Must be power-of-two between 128 and 16384
# - L: Must be power-of-two between 256 and 8192
grid = {
    'macro': [2, 4, 8, 16],
    'mini': [256, 512, 1024, 2048, 4096, 8192],
    'L': [512, 1024, 2048, 4096]
}

keys, values = zip(*grid.items())
combinations = [dict(zip(keys, v)) for v in itertools.product(*values)]

print("==================================================================")
print(f"GPUCOMPACT HYPERPARAMETER SWEEP ({len(combinations)} Configurations)")
print("==================================================================")

results = []

for i, params in enumerate(combinations):
    print(f"\n[{i+1}/{len(combinations)}] Testing: macro={params['macro']}MB, mini={params['mini']}B, L={params['L']}")
    
    cmd = [
        EXE_PATH,
        "--bench", SILESIA_DIR,
        "--macro", str(params['macro']),
        "--mini", str(params['mini']),
        "--L", str(params['L'])
    ]
    
    try:
        # 45-second timeout to allow full Silesia corpus runs per config
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=45)
        
        if res.returncode != 0:
            print(f"  -> [FAILED] Exit Code {res.returncode}")
            results.append(SweepResult(**params, ratio=0.0, avg_c_wall=0.0, avg_c_gpu=0.0, avg_d_wall=0.0, avg_d_gpu=0.0, status="FAILED"))
            continue
            
        # Parse overall benchmark ratio
        ratio_match = re.search(r'\*\* OVERALL BENCHMARK RATIO:\s+([\d\.]+)x', res.stdout)
        
        c_wall_speeds = []
        c_gpu_speeds = []
        d_wall_speeds = []
        d_gpu_speeds = []
        sha_failed = False
        
        # Parse table rows
        # Format: | Filename | Orig(MB) | Ratio | C_Wall(MB/s) | C_GPU(MB/s) | D_Wall(MB/s) | D_GPU(MB/s) | SHA |
        for line in res.stdout.splitlines():
            if "|" in line and ("PASS" in line or "FAIL" in line):
                cols = [c.strip() for c in line.split("|") if c.strip()]
                if len(cols) >= 8:
                    if "FAIL" in cols[7]:
                        sha_failed = True
                        break
                    try:
                        c_wall = float(cols[3])
                        c_gpu  = float(cols[4])
                        d_wall = float(cols[5])
                        d_gpu  = float(cols[6])
                        if c_wall > 0: c_wall_speeds.append(c_wall)
                        if c_gpu > 0: c_gpu_speeds.append(c_gpu)
                        if d_wall > 0: d_wall_speeds.append(d_wall)
                        if d_gpu > 0: d_gpu_speeds.append(d_gpu)
                    except ValueError:
                        pass
        
        if sha_failed:
            print("  -> [FAILED] SHA256 Verification Failed on one or more files!")
            results.append(SweepResult(**params, ratio=0.0, avg_c_wall=0.0, avg_c_gpu=0.0, avg_d_wall=0.0, avg_d_gpu=0.0, status="SHA_FAIL"))
        elif ratio_match and c_wall_speeds and d_wall_speeds:
            ratio = float(ratio_match.group(1))
            avg_cw = sum(c_wall_speeds) / len(c_wall_speeds)
            avg_cg = sum(c_gpu_speeds) / len(c_gpu_speeds)
            avg_dw = sum(d_wall_speeds) / len(d_wall_speeds)
            avg_dg = sum(d_gpu_speeds) / len(d_gpu_speeds)
            
            print(f"  -> PASS | Ratio: {ratio:.2f}x | C_Wall: {avg_cw:.1f} MB/s | D_Wall: {avg_dw:.1f} MB/s | D_GPU: {avg_dg:.1f} MB/s")
            results.append(SweepResult(**params, ratio=ratio, avg_c_wall=avg_cw, avg_c_gpu=avg_cg, avg_d_wall=avg_dw, avg_d_gpu=avg_dg, status="PASS"))
        else:
            print("  -> [FAILED] Could not parse output properly.")
            results.append(SweepResult(**params, ratio=0.0, avg_c_wall=0.0, avg_c_gpu=0.0, avg_d_wall=0.0, avg_d_gpu=0.0, status="PARSE_ERROR"))
            
    except subprocess.TimeoutExpired:
        print("  -> [TIMEOUT] Process hung and was automatically killed.")
        results.append(SweepResult(**params, ratio=0.0, avg_c_wall=0.0, avg_c_gpu=0.0, avg_d_wall=0.0, avg_d_gpu=0.0, status="TIMEOUT"))
    except Exception as e:
        print(f"  -> [ERROR] {str(e)}")
        results.append(SweepResult(**params, ratio=0.0, avg_c_wall=0.0, avg_c_gpu=0.0, avg_d_wall=0.0, avg_d_gpu=0.0, status="ERROR"))

# Analysis & Leaderboards
print("\n==================================================================")
print("SWEEP COMPLETE - TOP RESULTS")
print("==================================================================")

valid_results = [r for r in results if r.status == "PASS"]

if not valid_results:
    print("No valid passing results found. Check executable path or build.")
    sys.exit(1)

# 1. Best Ratio
best_ratio = max(valid_results, key=lambda x: x.ratio)
print(f"\n🏆 BEST COMPRESSION RATIO:")
print(f"   Ratio: {best_ratio.ratio:.3f}x | C_Wall: {best_ratio.avg_c_wall:.1f} MB/s | D_Wall: {best_ratio.avg_d_wall:.1f} MB/s")
print(f"   Params: --macro {best_ratio.macro} --mini {best_ratio.mini} --L {best_ratio.L}")

# 2. Best Decompression Speed
best_decomp = max(valid_results, key=lambda x: x.avg_d_wall)
print(f"\n🏆 BEST DECOMPRESSION SPEED (D_Wall):")
print(f"   D_Wall: {best_decomp.avg_d_wall:.1f} MB/s | D_GPU: {best_decomp.avg_d_gpu:.1f} MB/s | Ratio: {best_decomp.ratio:.3f}x")
print(f"   Params: --macro {best_decomp.macro} --mini {best_decomp.mini} --L {best_decomp.L}")

# 3. Best Compression Speed
best_comp = max(valid_results, key=lambda x: x.avg_c_wall)
print(f"\n🏆 BEST COMPRESSION SPEED (C_Wall):")
print(f"   C_Wall: {best_comp.avg_c_wall:.1f} MB/s | C_GPU: {best_comp.avg_c_gpu:.1f} MB/s | Ratio: {best_comp.ratio:.3f}x")
print(f"   Params: --macro {best_comp.macro} --mini {best_comp.mini} --L {best_comp.L}")

# 4. Best Balanced
best_balanced = max(valid_results, key=lambda x: (x.ratio**1.5) * (x.avg_d_wall**0.5) * (x.avg_c_wall**0.3))
print(f"\n🏆 BEST BALANCED PROFILE:")
print(f"   Ratio: {best_balanced.ratio:.3f}x | C_Wall: {best_balanced.avg_c_wall:.1f} MB/s | D_Wall: {best_balanced.avg_d_wall:.1f} MB/s")
print(f"   Params: --macro {best_balanced.macro} --mini {best_balanced.mini} --L {best_balanced.L}")

print("==================================================================")