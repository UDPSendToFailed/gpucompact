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
#include <string>
#include <vector>

TimingStats build_archive_explicit(
    const std::vector<std::pair<std::string, std::string>> &files_to_pack,
    const std::string &output_path, const LaunchConfig &config,
    CompressionContext *external_ctx = nullptr, bool quiet = false,
    ProgressCallback progress_cb = nullptr);

TimingStats build_archive(const std::vector<std::string> &input_paths,
                          const std::string &output_path,
                          const LaunchConfig &config,
                          CompressionContext *ctx = nullptr, bool quiet = false,
                          ProgressCallback progress_cb = nullptr);

void append_to_archive(const std::string &archive_path,
                       const std::vector<std::string> &input_paths,
                       LaunchConfig config,
                       const std::string &collision_mode = "replace",
                       ProgressCallback progress_cb = nullptr);

void remove_from_archive(const std::string &archive_path,
                         const std::vector<std::string> &files_to_remove,
                         ProgressCallback progress_cb = nullptr,
                         bool allow_empty = false);