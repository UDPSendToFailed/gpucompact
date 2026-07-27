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
#include "archive_format.h"
#include "context.cuh"
#include <cstdint>
#include <functional>
#include <string>

#pragma pack(push, 1)
struct BenchmarkFileResult {
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

using BenchmarkFileCallback = std::function<void(const BenchmarkFileResult &)>;

void run_benchmark(const std::string &bench_path, const LaunchConfig &config);

void run_benchmark_stream(const std::vector<std::string> &input_paths,
                          const LaunchConfig &config,
                          BenchmarkFileCallback file_cb);