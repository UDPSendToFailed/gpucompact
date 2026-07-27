/*
 * GPUCompact
 * Copyright (C) 2026 UDPSendToFailed
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_double, c_int, c_void};
use std::path::{Path, PathBuf};
use std::sync::{Condvar, LazyLock, Mutex};
use tauri::{AppHandle, Emitter};

#[repr(C)]
#[derive(Debug, Copy, Clone, Serialize, Deserialize)]
pub struct CLaunchConfig {
    pub macro_mb: c_int,
    pub mini_size: c_int,
    pub l_state: c_int,
    pub threads_comp: c_int,
    pub threads_decomp: c_int,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct ProgressPayload {
    pub processed: u64,
    pub total: u64,
    pub compressed: u64,
    pub elapsed_sec: f64,
    pub speed_mbs: f64,
    pub ratio: f64,
    pub percentage: f64,
    pub eta_sec: f64,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct CorruptionPayload {
    pub file_path: String,
    pub chunk_index: u32,
    pub total_chunks: u32,
    pub uncomp_bytes: u64,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct CollisionScanResult {
    pub total_incoming_files: u64,
    pub total_incoming_bytes: u64,
    pub colliding_paths: Vec<String>,
    pub clean_paths: Vec<String>,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct BenchmarkRowPayload {
    pub filename: String,
    pub orig_bytes: u64,
    pub comp_bytes: u64,
    pub ratio: f64,
    pub c_wall_mbs: f64,
    pub c_gpu_mbs: f64,
    pub d_wall_mbs: f64,
    pub d_gpu_mbs: f64,
    pub sha_passed: bool,
    pub orig_sha: String,
    pub dec_sha: String,
}

#[repr(C, packed)]
pub struct CBenchmarkFileResult {
    pub filename: [c_char; 256],
    pub orig_bytes: u64,
    pub comp_bytes: u64,
    pub ratio: f64,
    pub c_wall_mbs: f64,
    pub c_gpu_mbs: f64,
    pub d_wall_mbs: f64,
    pub d_gpu_mbs: f64,
    pub sha_passed: u8,
    pub orig_sha: [c_char; 65],
    pub dec_sha: [c_char; 65],
}

static CORRUPTION_SIGNAL: LazyLock<(Mutex<Option<i32>>, Condvar)> =
    LazyLock::new(|| (Mutex::new(None), Condvar::new()));

type CProgressCallback = extern "C" fn(
    processed: u64,
    total: u64,
    compressed: u64,
    elapsed_sec: c_double,
    user_data: *mut c_void,
);

type CCorruptionCallback = extern "C" fn(
    file_path: *const c_char,
    chunk_index: u32,
    total_chunks: u32,
    uncomp_bytes: u64,
    user_data: *mut c_void,
) -> c_int;

type CBenchmarkCallback = extern "C" fn(
    res_ptr: *const CBenchmarkFileResult,
    user_data: *mut c_void,
);

#[link(name = "gpucompact_native", kind = "dylib")]
unsafe extern "C" {
    fn gpucompact_build_archive(
        input_paths: *const *const c_char,
        num_inputs: c_int,
        output_path: *const c_char,
        config: CLaunchConfig,
        cb: Option<CProgressCallback>,
        user_data: *mut c_void,
    ) -> c_int;

    fn gpucompact_extract_archive(
        input_path: *const c_char,
        output_dir: *const c_char,
        config: CLaunchConfig,
        extract_file: *const c_char,
        collision_mode: *const c_char,
        cb: Option<CProgressCallback>,
        user_data: *mut c_void,
        corruption_cb: Option<CCorruptionCallback>,
    ) -> c_int;

    fn gpucompact_check_extract_collisions(
        archive_path: *const c_char,
        output_dir: *const c_char,
        extract_file: *const c_char,
    ) -> *mut c_char;

    fn gpucompact_inspect_archive_json(archive_path: *const c_char) -> *mut c_char;
    fn gpucompact_free_string(str_ptr: *mut c_char);

    fn gpucompact_append_to_archive(
        archive_path: *const c_char,
        input_paths: *const *const c_char,
        num_inputs: c_int,
        config: CLaunchConfig,
        collision_mode: *const c_char,
        cb: Option<CProgressCallback>,
        user_data: *mut c_void,
    ) -> c_int;

    fn gpucompact_remove_from_archive(
        archive_path: *const c_char,
        files_to_remove: *const *const c_char,
        num_files: c_int,
        cb: Option<CProgressCallback>,
        user_data: *mut c_void,
    ) -> c_int;

    fn gpucompact_run_benchmark_stream(
        input_paths: *const *const c_char,
        num_inputs: c_int,
        config: CLaunchConfig,
        cb: Option<CBenchmarkCallback>,
        user_data: *mut c_void,
    ) -> c_int;
}

extern "C" fn progress_trampoline(
    processed: u64,
    total: u64,
    compressed: u64,
    elapsed_sec: c_double,
    user_data: *mut c_void,
) {
    if user_data.is_null() {
        return;
    }
    let app_handle = unsafe { &*(user_data as *const AppHandle) };

    let pct = if total > 0 {
        (processed as f64 / total as f64) * 100.0
    } else {
        0.0
    };
    let speed = if elapsed_sec > 0.0 {
        (processed as f64 / (1024.0 * 1024.0)) / elapsed_sec
    } else {
        0.0
    };
    let ratio = if compressed > 0 {
        processed as f64 / compressed as f64
    } else {
        1.0
    };
    let bytes_left = if total > processed {
        total - processed
    } else {
        0
    };
    let eta = if speed > 0.0 {
        (bytes_left as f64 / (1024.0 * 1024.0)) / speed
    } else {
        0.0
    };

    let payload = ProgressPayload {
        processed,
        total,
        compressed,
        elapsed_sec,
        speed_mbs: speed,
        ratio,
        percentage: pct,
        eta_sec: eta,
    };

    let _ = app_handle.emit("progress", payload);
}

extern "C" fn corruption_trampoline(
    file_path: *const c_char,
    chunk_index: u32,
    total_chunks: u32,
    uncomp_bytes: u64,
    user_data: *mut c_void,
) -> c_int {
    if user_data.is_null() || file_path.is_null() {
        return 0;
    }
    let app_handle = unsafe { &*(user_data as *const AppHandle) };
    let path_str = unsafe { CStr::from_ptr(file_path) }
        .to_string_lossy()
        .into_owned();

    let payload = CorruptionPayload {
        file_path: path_str,
        chunk_index,
        total_chunks,
        uncomp_bytes,
    };

    let _ = app_handle.emit("corruption_detected", payload);

    let (lock, cvar): &(Mutex<Option<i32>>, Condvar) = &*CORRUPTION_SIGNAL;
    let mut guard = lock.lock().unwrap();
    *guard = None;

    while guard.is_none() {
        guard = cvar.wait(guard).unwrap();
    }

    guard.take().unwrap_or(0)
}

extern "C" fn benchmark_trampoline(
    res_ptr: *const CBenchmarkFileResult,
    user_data: *mut c_void,
) {
    if user_data.is_null() || res_ptr.is_null() {
        return;
    }
    let app_handle = unsafe { &*(user_data as *const AppHandle) };
    let res = unsafe { &*res_ptr };

    let filename = unsafe { CStr::from_ptr(res.filename.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    let orig_sha = unsafe { CStr::from_ptr(res.orig_sha.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    let dec_sha = unsafe { CStr::from_ptr(res.dec_sha.as_ptr()) }
        .to_string_lossy()
        .into_owned();

    let payload = BenchmarkRowPayload {
        filename,
        orig_bytes: res.orig_bytes,
        comp_bytes: res.comp_bytes,
        ratio: res.ratio,
        c_wall_mbs: res.c_wall_mbs,
        c_gpu_mbs: res.c_gpu_mbs,
        d_wall_mbs: res.d_wall_mbs,
        d_gpu_mbs: res.d_gpu_mbs,
        sha_passed: res.sha_passed != 0,
        orig_sha,
        dec_sha,
    };

    let _ = app_handle.emit("benchmark_row", payload);
}

#[tauri::command]
fn resolve_corruption(choice: String) -> Result<(), String> {
    let code = match choice.as_str() {
        "skip" => 1,
        "cancel" => 2,
        _ => 0,
    };

    let (lock, cvar): &(Mutex<Option<i32>>, Condvar) = &*CORRUPTION_SIGNAL;
    let mut guard = lock.lock().unwrap();
    *guard = Some(code);
    cvar.notify_all();
    Ok(())
}

fn collect_disk_relative_paths(
    path: &Path,
    base_parent: &Path,
    acc: &mut Vec<(PathBuf, String, u64)>,
) {
    if path.is_file() {
        if let Ok(rel) = path.strip_prefix(base_parent) {
            let rel_str = rel.to_string_lossy().replace('\\', "/");
            let size = std::fs::metadata(path).map(|m| m.len()).unwrap_or(0);
            acc.push((path.to_path_buf(), rel_str, size));
        }
    } else if path.is_dir() {
        if let Ok(entries) = std::fs::read_dir(path) {
            for entry in entries.flatten() {
                collect_disk_relative_paths(&entry.path(), base_parent, acc);
            }
        }
    }
}

#[tauri::command]
async fn check_append_collisions(
    archive_path: String,
    inputs: Vec<String>,
) -> Result<CollisionScanResult, String> {
    tokio::task::spawn_blocking(move || {
        let c_path = CString::new(archive_path).map_err(|e| e.to_string())?;
        let existing_set: HashSet<String> = unsafe {
            let json_ptr = gpucompact_inspect_archive_json(c_path.as_ptr());
            if json_ptr.is_null() {
                return Err("Failed to inspect archive for collisions.".to_string());
            }
            let c_str = CStr::from_ptr(json_ptr);
            let str_slice = String::from_utf8_lossy(c_str.to_bytes());
            let parsed: serde_json::Value =
                serde_json::from_str(&str_slice).map_err(|e| e.to_string())?;
            gpucompact_free_string(json_ptr);

            let mut set = HashSet::new();
            if let Some(files) = parsed.get("files").and_then(|f| f.as_array()) {
                for f in files {
                    if let Some(p) = f.get("path").and_then(|p| p.as_str()) {
                        set.insert(p.replace('\\', "/"));
                    }
                }
            }
            set
        };

        let mut collected: Vec<(PathBuf, String, u64)> = Vec::new();
        for inp in inputs {
            let path = Path::new(&inp);
            if !path.exists() {
                continue;
            }
            let base_parent = path.parent().unwrap_or(path);
            collect_disk_relative_paths(path, base_parent, &mut collected);
        }

        let mut colliding_paths = Vec::new();
        let mut clean_paths = Vec::new();
        let mut total_incoming_bytes = 0u64;

        for (_full_p, rel_p, size) in collected {
            total_incoming_bytes += size;
            if existing_set.contains(&rel_p) {
                colliding_paths.push(rel_p);
            } else {
                clean_paths.push(rel_p);
            }
        }

        Ok(CollisionScanResult {
            total_incoming_files: (colliding_paths.len() + clean_paths.len()) as u64,
            total_incoming_bytes,
            colliding_paths,
            clean_paths,
        })
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
fn inspect_archive(archive_path: String) -> Result<serde_json::Value, String> {
    let c_path = CString::new(archive_path).map_err(|e| e.to_string())?;
    unsafe {
        let json_ptr = gpucompact_inspect_archive_json(c_path.as_ptr());
        if json_ptr.is_null() {
            return Err("Failed to inspect GPUCompact archive.".to_string());
        }
        let c_str = CStr::from_ptr(json_ptr);
        let str_slice = String::from_utf8_lossy(c_str.to_bytes());
        let parsed: serde_json::Value =
            serde_json::from_str(&str_slice).map_err(|e| e.to_string())?;
        gpucompact_free_string(json_ptr);
        Ok(parsed)
    }
}

#[tauri::command]
async fn compress_archive(
    app: AppHandle,
    inputs: Vec<String>,
    output: String,
    profile: String,
    custom_config: Option<CLaunchConfig>,
) -> Result<String, String> {
    let mut config = CLaunchConfig {
        macro_mb: 4,
        mini_size: 512,
        l_state: 1024,
        threads_comp: 32,
        threads_decomp: 64,
    };

    if profile == "custom" {
        if let Some(c) = custom_config {
            config = c;
        }
    } else {
        match profile.as_str() {
            "ratio" => {
                config.macro_mb = 4;
                config.mini_size = 2048;
                config.l_state = 2048;
                config.threads_comp = 32;
                config.threads_decomp = 64;
            }
            "speed" => {
                config.macro_mb = 4;
                config.mini_size = 512;
                config.l_state = 512;
                config.threads_comp = 32;
                config.threads_decomp = 32;
            }
            "best_ratio" => {
                config.macro_mb = 8;
                config.mini_size = 8192;
                config.l_state = 2048;
                config.threads_comp = 32;
                config.threads_decomp = 64;
            }
            "best_speed" => {
                config.macro_mb = 4;
                config.mini_size = 256;
                config.l_state = 512;
                config.threads_comp = 32;
                config.threads_decomp = 32;
            }
            _ => {
                config.macro_mb = 4;
                config.mini_size = 512;
                config.l_state = 1024;
                config.threads_comp = 32;
                config.threads_decomp = 64;
            }
        }
    }

    tokio::task::spawn_blocking(move || {
        let app_handle = app.clone();
        let app_ptr = &app_handle as *const AppHandle as *mut c_void;

        let c_inputs: Vec<CString> = inputs
            .into_iter()
            .map(|s| CString::new(s).unwrap())
            .collect();
        let ptrs: Vec<*const c_char> = c_inputs.iter().map(|s| s.as_ptr()).collect();
        let c_output = CString::new(output).unwrap();

        let res = unsafe {
            gpucompact_build_archive(
                ptrs.as_ptr(),
                ptrs.len() as c_int,
                c_output.as_ptr(),
                config,
                Some(progress_trampoline),
                app_ptr,
            )
        };

        if res == 0 {
            Ok("Compression completed successfully.".to_string())
        } else {
            Err("Compression failed in GPU engine.".to_string())
        }
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn check_extract_collisions(
    archive_path: String,
    output_dir: String,
    extract_file: Option<String>,
) -> Result<CollisionScanResult, String> {
    tokio::task::spawn_blocking(move || {
        let c_path = CString::new(archive_path).map_err(|e| e.to_string())?;
        let c_out = CString::new(output_dir).map_err(|e| e.to_string())?;
        let c_ext = extract_file.map(|s| CString::new(s).unwrap());
        let ext_ptr = c_ext.as_ref().map_or(std::ptr::null(), |s| s.as_ptr());

        unsafe {
            let json_ptr =
                gpucompact_check_extract_collisions(c_path.as_ptr(), c_out.as_ptr(), ext_ptr);
            if json_ptr.is_null() {
                return Err("Failed to scan extraction collisions.".to_string());
            }
            let c_str = CStr::from_ptr(json_ptr);
            let str_slice = String::from_utf8_lossy(c_str.to_bytes());
            let parsed: serde_json::Value =
                serde_json::from_str(&str_slice).map_err(|e| e.to_string())?;
            gpucompact_free_string(json_ptr);

            let colliding_paths: Vec<String> = parsed["colliding_files"]
                .as_array()
                .map(|arr| {
                    arr.iter()
                        .filter_map(|v| v.as_str().map(|s| s.to_string()))
                        .collect()
                })
                .unwrap_or_default();

            Ok(CollisionScanResult {
                total_incoming_files: colliding_paths.len() as u64,
                total_incoming_bytes: 0,
                colliding_paths,
                clean_paths: vec![],
            })
        }
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn decompress_archive(
    app: AppHandle,
    archive_path: String,
    output_dir: String,
    extract_file: Option<String>,
    collision_mode: Option<String>,
) -> Result<String, String> {
    let config = CLaunchConfig {
        macro_mb: 4,
        mini_size: 1024,
        l_state: 2048,
        threads_comp: 6,
        threads_decomp: 56,
    };

    tokio::task::spawn_blocking(move || {
        let app_handle = app.clone();
        let app_ptr = &app_handle as *const AppHandle as *mut c_void;

        let c_input = CString::new(archive_path).unwrap();
        let c_out = CString::new(output_dir).unwrap();
        let c_ext = extract_file.map(|s| CString::new(s).unwrap());
        let ext_ptr = c_ext.as_ref().map_or(std::ptr::null(), |s| s.as_ptr());
        let cmode =
            CString::new(collision_mode.unwrap_or_else(|| "replace".to_string())).unwrap();

        let res = unsafe {
            gpucompact_extract_archive(
                c_input.as_ptr(),
                c_out.as_ptr(),
                config,
                ext_ptr,
                cmode.as_ptr(),
                Some(progress_trampoline),
                app_ptr,
                Some(corruption_trampoline),
            )
        };

        if res == 0 {
            Ok("Decompression completed successfully.".to_string())
        } else {
            Err("Decompression failed in GPU engine.".to_string())
        }
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn append_archive(
    app: AppHandle,
    archive_path: String,
    inputs: Vec<String>,
    collision_mode: String,
) -> Result<String, String> {
    let config = CLaunchConfig {
        macro_mb: 4,
        mini_size: 1024,
        l_state: 2048,
        threads_comp: 6,
        threads_decomp: 56,
    };

    tokio::task::spawn_blocking(move || {
        let app_handle = app.clone();
        let app_ptr = &app_handle as *const AppHandle as *mut c_void;

        let c_arch = CString::new(archive_path).unwrap();
        let c_mode = CString::new(collision_mode).unwrap();
        let c_inputs: Vec<CString> = inputs
            .into_iter()
            .map(|s| CString::new(s).unwrap())
            .collect();
        let ptrs: Vec<*const c_char> = c_inputs.iter().map(|s| s.as_ptr()).collect();

        let res = unsafe {
            gpucompact_append_to_archive(
                c_arch.as_ptr(),
                ptrs.as_ptr(),
                ptrs.len() as c_int,
                config,
                c_mode.as_ptr(),
                Some(progress_trampoline),
                app_ptr,
            )
        };

        if res == 0 {
            Ok("Append completed successfully.".to_string())
        } else {
            Err("Append failed in GPU engine.".to_string())
        }
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn remove_archive(
    app: AppHandle,
    archive_path: String,
    files_to_remove: Vec<String>,
) -> Result<String, String> {
    tokio::task::spawn_blocking(move || {
        let app_handle = app.clone();
        let app_ptr = &app_handle as *const AppHandle as *mut c_void;

        let c_arch = CString::new(archive_path).unwrap();
        let c_files: Vec<CString> = files_to_remove
            .into_iter()
            .map(|s| CString::new(s).unwrap())
            .collect();
        let ptrs: Vec<*const c_char> = c_files.iter().map(|s| s.as_ptr()).collect();

        let res = unsafe {
            gpucompact_remove_from_archive(
                c_arch.as_ptr(),
                ptrs.as_ptr(),
                ptrs.len() as c_int,
                Some(progress_trampoline),
                app_ptr,
            )
        };

        if res == 0 {
            Ok("Removal completed successfully.".to_string())
        } else {
            Err("Removal failed in GPU engine.".to_string())
        }
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
async fn run_gui_benchmark(
    app: AppHandle,
    inputs: Vec<String>,
    profile: String,
    custom_config: Option<CLaunchConfig>,
) -> Result<String, String> {
    let mut config = CLaunchConfig {
        macro_mb: 4,
        mini_size: 512,
        l_state: 1024,
        threads_comp: 32,
        threads_decomp: 64,
    };

    if profile == "custom" {
        if let Some(c) = custom_config {
            config = c;
        }
    } else {
        match profile.as_str() {
            "ratio" => {
                config.macro_mb = 4;
                config.mini_size = 2048;
                config.l_state = 2048;
                config.threads_comp = 32;
                config.threads_decomp = 64;
            }
            "speed" => {
                config.macro_mb = 4;
                config.mini_size = 512;
                config.l_state = 512;
                config.threads_comp = 32;
                config.threads_decomp = 32;
            }
            "best_ratio" => {
                config.macro_mb = 8;
                config.mini_size = 8192;
                config.l_state = 2048;
                config.threads_comp = 32;
                config.threads_decomp = 64;
            }
            "best_speed" => {
                config.macro_mb = 4;
                config.mini_size = 256;
                config.l_state = 512;
                config.threads_comp = 32;
                config.threads_decomp = 32;
            }
            _ => {
                config.macro_mb = 4;
                config.mini_size = 512;
                config.l_state = 1024;
                config.threads_comp = 32;
                config.threads_decomp = 64;
            }
        }
    }

    tokio::task::spawn_blocking(move || {
        let app_handle = app.clone();
        let app_ptr = &app_handle as *const AppHandle as *mut c_void;

        let c_inputs: Vec<CString> = inputs
            .into_iter()
            .map(|s| CString::new(s).unwrap())
            .collect();
        let ptrs: Vec<*const c_char> = c_inputs.iter().map(|s| s.as_ptr()).collect();

        let res = unsafe {
            gpucompact_run_benchmark_stream(
                ptrs.as_ptr(),
                ptrs.len() as c_int,
                config,
                Some(benchmark_trampoline),
                app_ptr,
            )
        };

        if res == 0 {
            Ok("Benchmark run completed.".to_string())
        } else {
            Err("Benchmark run failed in GPU engine.".to_string())
        }
    })
    .await
    .map_err(|e| e.to_string())?
}

#[tauri::command]
fn get_initial_cli_file() -> Option<String> {
    let args: Vec<String> = std::env::args().collect();
    for arg in args.into_iter().skip(1) {
        if arg.to_lowercase().ends_with(".gcmp") {
            let p = std::path::Path::new(&arg);
            if p.exists() {
                return Some(arg);
            }
        }
    }
    None
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .invoke_handler(tauri::generate_handler![
            inspect_archive,
            check_append_collisions,
            check_extract_collisions,
            resolve_corruption,
            compress_archive,
            decompress_archive,
            append_archive,
            remove_archive,
            run_gui_benchmark,
            get_initial_cli_file
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}