# GPUCompact

A disgustingly fast GPU-based archiver.

---

## Benchmarks on GeForce RTX 4090 for the Silesia corpus

### Profile Summary

| Profile | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | Ratio |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **`best_speed`** | ~347.0 | ~368.4 | ~1204.7 | ~1326.2 | **3.02x** |
| **`speed`** | ~327.8 | ~347.2 | ~1111.6 | ~1209.8 | **3.25x** |
| **`balanced`** *(default)* | ~318.4 | ~336.5 | ~1097.5 | ~1164.2 | **3.35x** |
| **`ratio`** | ~202.3 | ~209.3 | ~636.5 | ~646.7 | **3.59x** |
| **`best_ratio`** | ~118.7 | ~122.5 | ~316.8 | ~302.0 | **3.71x** |

<details>
<summary><b>Detailed results</b></summary>

#### Default Profile (`balanced`) — Overall Ratio: 3.35x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 3.18x | 242.42 | 264.35 | 908.45 | 508.32 | PASS |
| `mozilla` | 48.85 | 2.55x | 273.44 | 277.78 | 1116.29 | 1181.95 | PASS |
| `mr` | 9.51 | 3.36x | 241.51 | 260.10 | 899.83 | 1037.87 | PASS |
| `nci` | 32.00 | 12.70x | 351.17 | 362.01 | 1503.06 | 1640.40 | PASS |
| `ooffice` | 5.87 | 1.95x | 234.24 | 262.90 | 749.20 | 1004.48 | PASS |
| `osdb` | 9.62 | 3.09x | 374.45 | 419.59 | 848.43 | 977.02 | PASS |
| `reymont` | 6.32 | 4.40x | 358.42 | 420.89 | 818.74 | 1016.53 | PASS |
| `samba` | 20.61 | 3.95x | 281.91 | 293.52 | 1230.66 | 1318.05 | PASS |
| `sao` | 6.92 | 1.29x | 370.56 | 439.93 | 785.31 | 1108.40 | PASS |
| `webster` | 39.54 | 4.17x | 474.93 | 492.04 | 1322.77 | 1404.76 | PASS |
| `x-ray` | 8.08 | 1.74x | 344.84 | 394.36 | 790.48 | 950.31 | PASS |
| `xml` | 5.10 | 8.85x | 271.64 | 319.24 | 963.24 | 1253.09 | PASS |

#### `best_speed` Profile — Overall Ratio: 3.02x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 2.87x | 311.03 | 341.37 | 1190.76 | 737.44 | PASS |
| `mozilla` | 48.85 | 2.33x | 314.15 | 319.79 | 1264.02 | 1351.48 | PASS |
| `mr` | 9.51 | 2.98x | 252.92 | 272.73 | 1007.68 | 1199.68 | PASS |
| `nci` | 32.00 | 9.65x | 317.05 | 326.12 | 1414.44 | 1522.32 | PASS |
| `ooffice` | 5.87 | 1.81x | 257.76 | 294.07 | 738.89 | 1008.76 | PASS |
| `osdb` | 9.62 | 2.79x | 414.72 | 471.30 | 1073.74 | 1304.72 | PASS |
| `reymont` | 6.32 | 3.80x | 415.50 | 501.93 | 892.85 | 1130.98 | PASS |
| `samba` | 20.61 | 3.53x | 310.38 | 323.58 | 1403.57 | 1528.07 | PASS |
| `sao` | 6.92 | 1.23x | 449.91 | 559.79 | 768.61 | 1131.03 | PASS |
| `webster` | 39.54 | 3.69x | 502.09 | 522.86 | 1477.77 | 1588.73 | PASS |
| `x-ray` | 8.08 | 1.65x | 460.63 | 547.37 | 896.96 | 1135.19 | PASS |
| `xml` | 5.10 | 7.29x | 284.66 | 338.29 | 930.33 | 1356.72 | PASS |

#### `best_ratio` Profile — Overall Ratio: 3.71x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 3.59x | 156.59 | 169.20 | 299.18 | 196.01 | PASS |
| `mozilla` | 48.85 | 2.73x | 94.41 | 95.27 | 288.97 | 228.00 | PASS |
| `mr` | 9.51 | 3.72x | 63.65 | 65.68 | 185.49 | 192.78 | PASS |
| `nci` | 32.00 | 19.33x | 190.17 | 195.68 | 496.27 | 515.29 | PASS |
| `ooffice` | 5.87 | 2.06x | 76.71 | 80.85 | 236.57 | 283.36 | PASS |
| `osdb` | 9.62 | 3.42x | 81.89 | 85.14 | 226.08 | 237.61 | PASS |
| `reymont` | 6.32 | 5.17x | 110.49 | 118.94 | 284.37 | 334.56 | PASS |
| `samba` | 20.61 | 4.42x | 128.52 | 132.30 | 366.37 | 386.60 | PASS |
| `sao` | 6.92 | 1.32x | 98.69 | 106.15 | 229.48 | 280.37 | PASS |
| `webster` | 39.54 | 4.90x | 200.38 | 205.22 | 435.43 | 450.18 | PASS |
| `x-ray` | 8.08 | 1.85x | 84.46 | 88.65 | 206.09 | 220.94 | PASS |
| `xml` | 5.10 | 11.18x | 160.83 | 179.90 | 346.56 | 439.39 | PASS |

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
- **NSIS Setup Installer**: `src-tauri/target/release/bundle/nsis/GPUCompact_1.1.0_x64-setup.exe`
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