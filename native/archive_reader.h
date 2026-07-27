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
#include <functional>
#include <string>
#include <vector>

// 0 = Continue & Zero-Fill, 1 = Skip File, 2 = Abort Extraction
using CorruptionCallback =
    std::function<int(const std::string &file_path, uint32_t chunk_index,
                      uint32_t total_file_chunks, uint64_t uncomp_bytes)>;

TimingStats extract_archive(
    const std::string &input_path, const std::string &output_dir,
    LaunchConfig config,
    std::vector<DecompressionContext *> *external_contexts = nullptr,
    const std::string &extract_file = "", bool quiet = false,
    ProgressCallback progress_cb = nullptr,
    CorruptionCallback corruption_cb = nullptr,
    const std::string &collision_mode = "replace");

void inspect_archive_cli(const std::string &archive_path);