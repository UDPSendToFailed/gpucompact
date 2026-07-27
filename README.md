# GPUCompact

A disgustingly fast GPU-based archiver.

---

## Benchmarks on GeForce RTX 4090 for the Silesia corpus

### Profile Summary

| Profile | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | Ratio |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **`best_speed`** | ~287.6 | ~331.7 | ~697.5 | ~919.1 | **3.02x** |
| **`speed`** | ~252.0 | ~289.4 | ~624.2 | ~833.8 | **3.25x** |
| **`balanced`** *(default)* | ~254.3 | ~290.4 | ~641.8 | ~868.5 | **3.35x** |
| **`ratio`** | ~185.3 | ~201.5 | ~420.1 | ~507.0 | **3.59x** |
| **`best_ratio`** | ~113.8 | ~123.6 | ~256.2 | ~294.9 | **3.71x** |

<details>
<summary><b>Detailed results</b></summary>

#### Default Profile (`balanced`) — Overall Ratio: 3.35x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 3.18x | 216.92 | 244.97 | 602.81 | 451.24 | PASS |
| `mozilla` | 48.85 | 2.55x | 216.81 | 226.67 | 771.87 | 869.72 | PASS |
| `mr` | 9.51 | 3.36x | 193.07 | 216.94 | 573.09 | 842.64 | PASS |
| `nci` | 32.00 | 12.70x | 264.79 | 279.17 | 958.12 | 1146.01 | PASS |
| `ooffice` | 5.87 | 1.95x | 188.80 | 222.12 | 396.06 | 815.79 | PASS |
| `osdb` | 9.62 | 3.09x | 267.61 | 317.30 | 548.77 | 795.47 | PASS |
| `reymont` | 6.32 | 4.40x | 269.91 | 335.17 | 519.18 | 914.15 | PASS |
| `samba` | 20.61 | 3.95x | 216.99 | 231.51 | 771.34 | 949.87 | PASS |
| `sao` | 6.92 | 1.29x | 348.87 | 471.66 | 426.09 | 714.34 | PASS |
| `webster` | 39.54 | 4.17x | 344.14 | 367.82 | 859.20 | 995.14 | PASS |
| `x-ray` | 8.08 | 1.74x | 286.27 | 351.85 | 485.88 | 735.63 | PASS |
| `xml` | 5.10 | 8.85x | 203.95 | 245.79 | 460.28 | 976.97 | PASS |

#### `best_speed` Profile — Overall Ratio: 3.02x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 2.87x | 227.13 | 258.73 | 624.76 | 482.78 | PASS |
| `mozilla` | 48.85 | 2.33x | 237.18 | 249.73 | 870.86 | 1000.18 | PASS |
| `mr` | 9.51 | 2.98x | 215.44 | 243.67 | 641.96 | 1004.61 | PASS |
| `nci` | 32.00 | 9.65x | 270.28 | 285.35 | 992.17 | 1190.32 | PASS |
| `ooffice` | 5.87 | 1.81x | 210.45 | 258.29 | 453.91 | 849.72 | PASS |
| `osdb` | 9.62 | 2.79x | 336.58 | 408.87 | 598.24 | 904.65 | PASS |
| `reymont` | 6.32 | 3.80x | 303.52 | 387.68 | 541.24 | 1022.56 | PASS |
| `samba` | 20.61 | 3.53x | 229.22 | 246.49 | 820.25 | 1059.70 | PASS |
| `sao` | 6.92 | 1.23x | 387.50 | 552.71 | 448.80 | 797.63 | PASS |
| `webster` | 39.54 | 3.69x | 360.94 | 390.44 | 863.21 | 986.59 | PASS |
| `x-ray` | 8.08 | 1.65x | 342.39 | 442.78 | 521.38 | 811.18 | PASS |
| `xml` | 5.10 | 7.29x | 208.62 | 257.42 | 440.18 | 923.03 | PASS |

#### `best_ratio` Profile — Overall Ratio: 3.71x

| Filename | Size (MB) | Ratio | Comp. Wall (MB/s) | Comp. GPU (MB/s) | Decomp. Wall (MB/s) | Decomp. GPU (MB/s) | SHA |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `dickens` | 9.72 | 3.59x | 136.77 | 154.26 | 258.37 | 186.14 | PASS |
| `mozilla` | 48.85 | 2.73x | 89.81 | 92.02 | 265.64 | 214.22 | PASS |
| `mr` | 9.51 | 3.72x | 69.66 | 73.91 | 190.40 | 216.94 | PASS |
| `nci` | 32.00 | 19.33x | 189.82 | 200.38 | 471.57 | 522.67 | PASS |
| `ooffice` | 5.87 | 2.06x | 75.41 | 82.75 | 197.01 | 269.48 | PASS |
| `osdb` | 9.62 | 3.42x | 81.93 | 87.70 | 195.35 | 224.81 | PASS |
| `reymont` | 6.32 | 5.17x | 99.44 | 111.34 | 228.85 | 308.10 | PASS |
| `samba` | 20.61 | 4.42x | 118.36 | 124.88 | 317.80 | 355.57 | PASS |
| `sao` | 6.92 | 1.32x | 102.10 | 115.08 | 185.40 | 249.04 | PASS |
| `webster` | 39.54 | 4.90x | 162.84 | 170.02 | 364.85 | 389.23 | PASS |
| `x-ray` | 8.08 | 1.85x | 82.77 | 90.32 | 178.99 | 210.15 | PASS |
| `xml` | 5.10 | 11.18x | 138.09 | 163.96 | 267.30 | 422.53 | PASS |

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
- **NSIS Setup Installer**: `src-tauri/target/release/bundle/nsis/GPUCompact_1.0.0_x64-setup.exe`
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