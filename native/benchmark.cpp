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
#include "benchmark.h"
#include "archive_reader.h"
#include "archive_writer.h"
#include "sha256.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
// clang-format on
#endif

namespace fs = std::filesystem;

static void safe_remove_file(const fs::path &p) {
  for (int retry = 0; retry < 20; retry++) {
    try {
      if (fs::exists(p))
        fs::remove(p);
      return;
    } catch (const fs::filesystem_error &) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

static void safe_remove_directory(const fs::path &p) {
  for (int retry = 0; retry < 20; retry++) {
    try {
      if (fs::exists(p))
        fs::remove_all(p);
      return;
    } catch (const fs::filesystem_error &) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

void run_benchmark_stream(const std::vector<std::string> &input_paths,
                          const LaunchConfig &config,
                          BenchmarkFileCallback file_cb) {
#ifdef _WIN32
  // Elevate thread scheduling priority to prevent OS background process
  // throttling
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif

  std::vector<std::string> files;
  for (const auto &p : input_paths) {
    if (fs::is_regular_file(p)) {
      files.push_back(p);
    } else if (fs::is_directory(p)) {
      for (const auto &entry : fs::recursive_directory_iterator(p)) {
        if (entry.is_regular_file())
          files.push_back(entry.path().string());
      }
    }
  }

  if (files.empty())
    return;

  CompressionContext comp_ctx(config.macro_mb * 1024 * 1024, config.mini_size,
                              config.L);
  std::vector<DecompressionContext *> decomp_contexts;
  for (int i = 0; i < 3; i++) {
    decomp_contexts.push_back(new DecompressionContext(
        config.macro_mb * 1024 * 1024, config.mini_size, config.L));
  }

  // Pre-benchmark CUDA Warmup: Initialize driver, stream state & CUB kernels
  int dummy_n = std::min(1024 * 1024, config.macro_mb * 1024 * 1024);
  for (int i = 0; i < dummy_n; i++)
    comp_ctx.host_in[i] = (unsigned char)(i % 255);
  comp_ctx.bytes_read = dummy_n;
  comp_ctx.compress_chunk(config.threads_comp, config.mini_size);
  cudaDeviceSynchronize();

  decomp_contexts[0]->uncomp_size = dummy_n;
  decomp_contexts[0]->comp_size = comp_ctx.comp_size;
  decomp_contexts[0]->is_raw = comp_ctx.is_raw;
  decomp_contexts[0]->primary_idx = comp_ctx.primary_idx;
  decomp_contexts[0]->num_chunks = comp_ctx.num_chunks;
  decomp_contexts[0]->total_words = comp_ctx.total_words;
  decomp_contexts[0]->gpu_hash = comp_ctx.gpu_hash;

  std::memcpy(decomp_contexts[0]->host_in, comp_ctx.host_out,
              comp_ctx.comp_size);
  decomp_contexts[0]->decompress_chunk(config.threads_decomp, config.mini_size);
  cudaDeviceSynchronize();

  for (const auto &file_path : files) {
    uint64_t orig_size = fs::file_size(file_path);
    if (orig_size == 0)
      continue;

    std::string full_filename = fs::path(file_path).filename().string();
    std::string temp_out = "temp_bench_" + full_filename + ".gcmp";
    std::string temp_dec = "temp_dec_bench_" + full_filename;

    try {
      TimingStats c_stats =
          build_archive({file_path}, temp_out, config, &comp_ctx, true);
      uint64_t comp_size = fs::file_size(temp_out);

      TimingStats d_stats = extract_archive(temp_out, temp_dec, config,
                                            &decomp_contexts, "", true);

      std::string dec_file_path;
      if (fs::exists(temp_dec)) {
        for (const auto &entry : fs::recursive_directory_iterator(temp_dec)) {
          if (entry.is_regular_file()) {
            dec_file_path = entry.path().string();
            break;
          }
        }
      }

      std::string orig_sha = compute_sha256_file(file_path);
      std::string dec_sha =
          !dec_file_path.empty() ? compute_sha256_file(dec_file_path) : "";
      bool sha_passed =
          (!orig_sha.empty() && !dec_sha.empty() && orig_sha == dec_sha);

      double orig_mb = (double)orig_size / 1048576.0;
      double c_wall_speed =
          (c_stats.wall_ms > 0) ? (orig_mb / (c_stats.wall_ms / 1000.0)) : 0;
      double c_gpu_speed =
          (c_stats.gpu_ms > 0) ? (orig_mb / (c_stats.gpu_ms / 1000.0)) : 0;
      double d_wall_speed =
          (d_stats.wall_ms > 0) ? (orig_mb / (d_stats.wall_ms / 1000.0)) : 0;
      double d_gpu_speed =
          (d_stats.gpu_ms > 0) ? (orig_mb / (d_stats.gpu_ms / 1000.0)) : 0;
      double ratio = (comp_size > 0) ? ((double)orig_size / comp_size) : 1.0;

      BenchmarkFileResult res;
      std::memset(&res, 0, sizeof(BenchmarkFileResult));
      std::strncpy(res.filename, full_filename.c_str(),
                   sizeof(res.filename) - 1);
      res.orig_bytes = orig_size;
      res.comp_bytes = comp_size;
      res.ratio = ratio;
      res.c_wall_mbs = c_wall_speed;
      res.c_gpu_mbs = c_gpu_speed;
      res.d_wall_mbs = d_wall_speed;
      res.d_gpu_mbs = d_gpu_speed;
      res.sha_passed = sha_passed;
      std::strncpy(res.orig_sha, orig_sha.c_str(), sizeof(res.orig_sha) - 1);
      std::strncpy(res.dec_sha, dec_sha.c_str(), sizeof(res.dec_sha) - 1);

      if (file_cb)
        file_cb(res);

    } catch (const std::exception &e) {
      std::cerr << "[BENCHMARK ERROR] " << full_filename << ": " << e.what()
                << std::endl;
    }

    safe_remove_file(temp_out);
    safe_remove_directory(temp_dec);
  }

  for (auto *ctx : decomp_contexts)
    delete ctx;
}

void run_benchmark(const std::string &bench_path, const LaunchConfig &config) {
  std::cout << "\n============================================================="
               "======================================"
            << std::endl;
  std::cout << "Executing Benchmark on '" << bench_path << "'" << std::endl;
  std::cout << "==============================================================="
               "===================================="
            << std::endl;
  std::cout << "| Filename        | Orig(MB)  | Ratio   | C_Wall(MB/s) | "
               "C_GPU(MB/s)  | D_Wall(MB/s) | D_GPU(MB/s)  | SHA    |"
            << std::endl;
  std::cout << "---------------------------------------------------------------"
               "------------------------------------"
            << std::endl;

  uint64_t total_orig = 0, total_comp = 0;

  run_benchmark_stream(
      {bench_path}, config, [&](const BenchmarkFileResult &res) {
        total_orig += res.orig_bytes;
        total_comp += res.comp_bytes;

        std::cout << "| " << std::left << std::setw(15)
                  << std::string(res.filename).substr(0, 15) << " | "
                  << std::right << std::setw(9) << std::fixed
                  << std::setprecision(2) << (res.orig_bytes / 1048576.0)
                  << " | " << std::setw(6) << std::setprecision(2) << res.ratio
                  << "x"
                  << " | " << std::setw(12) << std::setprecision(2)
                  << res.c_wall_mbs << " | " << std::setw(12)
                  << std::setprecision(2) << res.c_gpu_mbs << " | "
                  << std::setw(12) << std::setprecision(2) << res.d_wall_mbs
                  << " | " << std::setw(12) << std::setprecision(2)
                  << res.d_gpu_mbs << " | "
                  << (res.sha_passed ? "PASS   " : "FAIL   ") << " |"
                  << std::endl;
      });

  std::cout << "==============================================================="
               "===================================================="
            << std::endl;
  std::cout << "** OVERALL BENCHMARK RATIO: " << std::fixed
            << std::setprecision(2) << ((double)total_orig / total_comp)
            << "x **\n"
            << std::endl;
}