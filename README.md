# GPUCompact

A disgustingly fast GPU-based archiver.

---

## Benchmarks on GeForce RTX 4090 for the Silesia corpus

### Profile Summary

| Profile | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | Ratio |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **`best_speed`** | ~701.3 | ~805.1 | ~2077.1 | ~2834.7 | **3.02x** |
| **`speed`** | ~659.7 | ~755.0 | ~1952.4 | ~2508.0 | **3.25x** |
| **`balanced`** *(default)* | ~639.4 | ~722.5 | ~1918.8 | ~2420.2 | **3.35x** |
| **`ratio`** | ~453.2 | ~493.3 | ~1197.5 | ~1318.3 | **3.60x** |
| **`best_ratio`** | ~272.5 | ~295.8 | ~694.8 | ~717.5 | **3.72x** |

<details>
<summary><b>Detailed results</b></summary>

#### Default Profile (`balanced`) — Overall Ratio: 3.35x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 3.18x | 610.08 | 762.46 | 1779.06 | 1601.16 | PASS |
| `mozilla` | 48.85 | 2.55x | 675.80 | 707.20 | 2047.61 | 2315.22 | PASS |
| `mr` | 9.51 | 3.36x | 545.90 | 656.45 | 1301.17 | 1603.32 | PASS |
| `nci` | 32.00 | 12.70x | 575.66 | 609.23 | 2886.80 | 3566.10 | PASS |
| `ooffice` | 5.87 | 1.95x | 464.89 | 615.62 | 917.25 | 1819.18 | PASS |
| `osdb` | 9.62 | 3.09x | 626.41 | 781.01 | 1578.63 | 2125.96 | PASS |
| `reymont` | 6.32 | 4.40x | 521.90 | 693.66 | 1432.18 | 2251.27 | PASS |
| `samba` | 20.61 | 3.95x | 585.46 | 638.16 | 2128.09 | 2472.80 | PASS |
| `sao` | 6.92 | 1.29x | 698.66 | 1017.26 | 1083.79 | 2053.32 | PASS |
| `webster` | 39.54 | 4.17x | 886.76 | 952.23 | 2747.69 | 3158.60 | PASS |
| `x-ray` | 8.08 | 1.75x | 677.69 | 894.86 | 1327.95 | 1999.34 | PASS |
| `xml` | 5.10 | 8.85x | 385.87 | 494.02 | 1220.15 | 1944.58 | PASS |

#### `best_speed` Profile — Overall Ratio: 3.02x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 2.87x | 620.49 | 780.09 | 1782.72 | 1780.12 | PASS |
| `mozilla` | 48.85 | 2.33x | 761.03 | 799.69 | 2447.84 | 2813.01 | PASS |
| `mr` | 9.51 | 2.98x | 597.51 | 741.92 | 1365.70 | 1729.08 | PASS |
| `nci` | 32.00 | 9.65x | 582.63 | 617.65 | 2864.96 | 3668.92 | PASS |
| `ooffice` | 5.87 | 1.81x | 548.95 | 775.79 | 1107.16 | 2590.82 | PASS |
| `osdb` | 9.62 | 2.79x | 711.75 | 924.10 | 1614.13 | 2636.78 | PASS |
| `reymont` | 6.32 | 3.80x | 632.85 | 893.59 | 1425.49 | 2560.66 | PASS |
| `samba` | 20.61 | 3.53x | 650.25 | 716.50 | 2467.31 | 2889.07 | PASS |
| `sao` | 6.92 | 1.23x | 769.37 | 1284.11 | 1183.78 | 2493.25 | PASS |
| `webster` | 39.54 | 3.69x | 978.81 | 1057.60 | 2921.80 | 3666.68 | PASS |
| `x-ray` | 8.08 | 1.65x | 829.62 | 1207.79 | 1310.28 | 2510.78 | PASS |
| `xml` | 5.10 | 7.29x | 406.70 | 529.86 | 1219.51 | 2592.21 | PASS |

#### `best_ratio` Profile — Overall Ratio: 3.72x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 3.59x | 327.37 | 395.45 | 634.86 | 489.26 | PASS |
| `mozilla` | 48.85 | 2.73x | 223.11 | 228.35 | 596.71 | 490.41 | PASS |
| `mr` | 9.51 | 3.72x | 156.13 | 170.26 | 414.61 | 454.76 | PASS |
| `nci` | 32.00 | 19.33x | 370.48 | 394.46 | 1352.18 | 1523.08 | PASS |
| `ooffice` | 5.87 | 2.06x | 175.41 | 201.63 | 402.59 | 562.18 | PASS |
| `osdb` | 9.62 | 3.43x | 161.57 | 176.47 | 436.69 | 482.11 | PASS |
| `reymont` | 6.32 | 5.17x | 219.08 | 258.32 | 513.87 | 728.23 | PASS |
| `samba` | 20.61 | 4.43x | 315.38 | 341.65 | 850.83 | 964.00 | PASS |
| `sao` | 6.92 | 1.33x | 231.03 | 282.57 | 435.20 | 670.13 | PASS |
| `webster` | 39.54 | 4.90x | 549.51 | 592.45 | 1216.43 | 1347.06 | PASS |
| `x-ray` | 8.08 | 1.85x | 198.07 | 226.79 | 452.74 | 531.05 | PASS |
| `xml` | 5.10 | 11.18x | 320.85 | 405.81 | 655.04 | 1021.00 | PASS |

</details>

---

## How to Build

### Prerequisites
- **NVIDIA CUDA Toolkit 13.3** (`nvcc`)
- **Node.js 24** & **npm**
- **Rust Toolchain** (2024 edition support)
- **Visual Studio 2022** (C++ Desktop Development workload)

### Build Desktop GUI & Setup Installer
```powershell
# Install frontend dependencies
npm install

# Build app, native CUDA DLL, CLI binary, and NSIS installer
npm run tauri build
```
Artifacts generated:
- **NSIS Setup Installer**: `src-tauri/target/release/bundle/nsis/GPUCompact_1.2.0_x64-setup.exe`
- **Desktop Executable**: `src-tauri/target/release/gpucompact-app.exe`
- **CLI Executable**: `src-tauri/target/release/gpucompact.exe`
- **Native Shared Library**: `src-tauri/target/release/gpucompact_native.dll`

### Build Native CLI Tool Only
> **Note**: On Windows, run raw `nvcc` commands inside **x64 Native Tools Command Prompt for VS 2022** so `cl.exe` is in your system `PATH`.

```powershell
cd native
nvcc -O3 -std=c++17 --threads 32 -arch=sm_89 --use_fast_math -extra-device-vectorization -Xptxas -O3 -Xptxas -dlcm=ca -Xcompiler "/O2,/Ob3,/arch:AVX2,/fp:fast,/MP,/Zc:preprocessor,/Zc:__cplusplus" -Xlinker "/OPT:REF,/OPT:ICF" -o gpucompact.exe main.cpp archive_reader.cpp archive_writer.cpp benchmark.cpp sha256.cpp context.cu kernels.cu
```

---

## Disclaimer

This is a personal project I made because I was bored. I take no responsibility for data loss, archive corruption, melted GPUs, or blown up SSDs because they couldn't keep up with the speed. It might or might not work with your graphics card, it's only been tested on an RTX 4090. The project contains AI-assisted code and comes without any form of warranty.

Also, if you are a corporation, the repository is licensed under AGPLv3. Incorporating, bundling, or hosting this codebase inside proprietary, closed-source software or services without open-sourcing your surrounding stack under AGPLv3 is strictly prohibited by law. If you want to use this in a closed-source setup without copyleft obligations, you need my explicit, written permission and a separate commercial license. If you are just an individual developer testing or forking cool open-source projects, feel free to do whatever you want with it under the terms of the AGPLv3.