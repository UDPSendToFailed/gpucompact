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
#include <functional>
#include <string>
#include <vector>

#pragma pack(push, 1)
struct FileEntry {
  uint64_t path_offset;
  uint64_t file_size;
  uint32_t chunk_start;
  uint32_t chunk_count;
};

struct ChunkEntry {
  uint64_t payload_offset;
  uint32_t comp_size;
  uint32_t uncomp_size;
  uint32_t primary_idx;
  uint8_t is_raw;
  uint8_t reserved[3];
  uint64_t gpu_hash;
};

struct GTOCFooter {
  uint64_t gtoc_offset;
  uint32_t num_files;
  uint32_t num_chunks;
  uint32_t macro_mb;
  uint32_t mini_size;
  uint32_t L_state;
  char magic[4]; // "GTOC"
};
#pragma pack(pop)

struct TimingStats {
  float wall_ms = 0.0f;
  float gpu_ms = 0.0f;
};

using ProgressCallback =
    std::function<void(uint64_t processed, uint64_t total, uint64_t compressed,
                       double elapsed_sec)>;