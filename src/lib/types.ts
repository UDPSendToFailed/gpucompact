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
export interface ArchiveMetadata {
  archive_name: string;
  archive_size: number;
  macro_mb: number;
  mini_size: number;
  L_state: number;
  num_files: number;
  num_chunks: number;
  total_uncompressed_bytes: number;
  total_compressed_bytes: number;
  overall_ratio: number;
}

export interface ArchiveFileEntry {
  path: string;
  uncompressed_size: number;
  compressed_size: number;
  ratio: number;
  chunk_start: number;
  chunk_count: number;
}

export interface ArchiveInspectResult {
  metadata: ArchiveMetadata;
  files: ArchiveFileEntry[];
}

export interface ProgressPayload {
  processed: number;
  total: number;
  compressed: number;
  elapsed_sec: number;
  speed_mbs: number;
  ratio: number;
  percentage: number;
  eta_sec: number;
}

export interface CorruptionPayload {
  file_path: string;
  chunk_index: number;
  total_chunks: number;
  uncomp_bytes: number;
}

export interface CollisionScanResult {
  total_incoming_files: number;
  total_incoming_bytes: number;
  colliding_paths: string[];
  clean_paths: string[];
}

export interface BenchmarkRowPayload {
  filename: string;
  orig_bytes: number;
  comp_bytes: number;
  ratio: number;
  c_wall_mbs: number;
  c_gpu_mbs: number;
  d_wall_mbs: number;
  d_gpu_mbs: number;
  sha_passed: boolean;
  orig_sha: string;
  dec_sha: string;
}

export type ProfilePreset = 'balanced' | 'speed' | 'ratio' | 'best_speed' | 'best_ratio' | 'custom';
export type CollisionMode = 'replace' | 'rename' | 'skip';

export interface CustomLaunchConfig {
  macro_mb: number;
  mini_size: number;
  l_state: number;
  threads_comp: number;
  threads_decomp: number;
}