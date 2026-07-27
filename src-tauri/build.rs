use std::env;
use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let app_dir = manifest_dir.parent().unwrap();
    let native_dir = app_dir.join("native");

    println!("cargo:rerun-if-changed={}", native_dir.display());

    let c_api = native_dir.join("c_api.cpp");
    let main_cpp = native_dir.join("main.cpp");

    if c_api.exists() {
        println!("cargo:warning=Building native CUDA shared library & CLI binary...");

        let mut base_flags = vec![
            "-O3",
            "-std=c++17",
            "--threads",
            "32",
            "-arch=sm_89",
            "--use_fast_math",
            "-extra-device-vectorization",
            "-Xptxas",
            "-O3",
            "-Xptxas",
            "-dlcm=ca",
            "-Xcompiler",
            "/O2,/Ob3,/arch:AVX2,/fp:fast,/MP,/Zc:preprocessor,/Zc:__cplusplus",
            "-Xlinker",
            "/OPT:REF,/OPT:ICF",
        ];

        let cl_path_buf;
        if cfg!(target_os = "windows") {
            let compiler = cc::Build::new().get_compiler();
            cl_path_buf = compiler.path().to_string_lossy().to_string();
            base_flags.push("-ccbin");
            base_flags.push(&cl_path_buf);
        }

        // 1. Build DLL (gpucompact_native.dll)
        let mut dll_args = base_flags.clone();
        dll_args.push("-shared");
        dll_args.extend_from_slice(&[
            "-o",
            "gpucompact_native.dll",
            "c_api.cpp",
            "archive_reader.cpp",
            "archive_writer.cpp",
            "benchmark.cpp",
            "sha256.cpp",
            "context.cu",
            "kernels.cu",
        ]);

        let status_dll = Command::new("nvcc")
            .current_dir(&native_dir)
            .args(&dll_args)
            .status();

        if let Ok(st) = status_dll {
            if !st.success() {
                println!("cargo:warning=nvcc DLL compilation returned status: {}", st);
            }
        }

        // 2. Build CLI Binary (gpucompact.exe)
        if main_cpp.exists() {
            let mut cli_args = base_flags.clone();
            cli_args.extend_from_slice(&[
                "-o",
                "gpucompact.exe",
                "main.cpp",
                "archive_reader.cpp",
                "archive_writer.cpp",
                "benchmark.cpp",
                "sha256.cpp",
                "context.cu",
                "kernels.cu",
            ]);

            let status_cli = Command::new("nvcc")
                .current_dir(&native_dir)
                .args(&cli_args)
                .status();

            if let Ok(st) = status_cli {
                if !st.success() {
                    println!("cargo:warning=nvcc CLI compilation returned status: {}", st);
                }
            }
        }
    }

    println!(
        "cargo:rustc-link-search=native={}",
        native_dir.display()
    );
    println!("cargo:rustc-link-lib=gpucompact_native");

    let dll_src = native_dir.join("gpucompact_native.dll");
    let cli_src = native_dir.join("gpucompact.exe");

    // Copy to manifest_dir (src-tauri/) for Tauri resource bundling
    if dll_src.exists() {
        let _ = fs::copy(&dll_src, manifest_dir.join("gpucompact_native.dll"));
    }
    if cli_src.exists() {
        let _ = fs::copy(&cli_src, manifest_dir.join("gpucompact.exe"));
    }

    if let Ok(out_dir) = env::var("OUT_DIR") {
        let mut target_dir = PathBuf::from(out_dir);
        target_dir.pop();
        target_dir.pop();
        target_dir.pop();

        if dll_src.exists() {
            let _ = fs::copy(&dll_src, target_dir.join("gpucompact_native.dll"));
        }
        if cli_src.exists() {
            let _ = fs::copy(&cli_src, target_dir.join("gpucompact.exe"));
        }
    }

    tauri_build::build();
}