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
#pragma once
#include <cstdint>

#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct CLaunchConfig {
  int macro_mb;
  int mini_size;
  int L;
  int threads_comp;
  int threads_decomp;
};

#pragma pack(push, 1)
struct CBenchmarkFileResult {
  char filename[256];
  uint64_t orig_bytes;
  uint64_t comp_bytes;
  double ratio;
  double c_wall_mbs;
  double c_gpu_mbs;
  double d_wall_mbs;
  double d_gpu_mbs;
  uint8_t sha_passed;
  char orig_sha[65];
  char dec_sha[65];
};
#pragma pack(pop)

typedef void (*CProgressCallback)(uint64_t processed, uint64_t total,
                                  uint64_t compressed, double elapsed_sec,
                                  void *user_data);

typedef int (*CCorruptionCallback)(const char *file_path, uint32_t chunk_index,
                                   uint32_t total_chunks, uint64_t uncomp_bytes,
                                   void *user_data);

typedef void (*CBenchmarkCallback)(const CBenchmarkFileResult *result,
                                   void *user_data);

DllExport int gpucompact_build_archive(const char **input_paths, int num_inputs,
                                       const char *output_path,
                                       CLaunchConfig config,
                                       CProgressCallback cb, void *user_data);

DllExport int
gpucompact_extract_archive(const char *input_path, const char *output_dir,
                           CLaunchConfig config, const char *extract_file,
                           const char *collision_mode, CProgressCallback cb,
                           void *user_data, CCorruptionCallback corruption_cb);

DllExport char *gpucompact_check_extract_collisions(const char *archive_path,
                                                    const char *output_dir,
                                                    const char *extract_file);

DllExport char *gpucompact_inspect_archive_json(const char *archive_path);
DllExport void gpucompact_free_string(char *str);

DllExport int gpucompact_append_to_archive(const char *archive_path,
                                           const char **input_paths,
                                           int num_inputs, CLaunchConfig config,
                                           const char *collision_mode,
                                           CProgressCallback cb,
                                           void *user_data);

DllExport int gpucompact_remove_from_archive(const char *archive_path,
                                             const char **files_to_remove,
                                             int num_files,
                                             CProgressCallback cb,
                                             void *user_data);

DllExport int gpucompact_run_benchmark_stream(const char **input_paths,
                                              int num_inputs,
                                              CLaunchConfig config,
                                              CBenchmarkCallback cb,
                                              void *user_data);

#ifdef __cplusplus
}
#endif