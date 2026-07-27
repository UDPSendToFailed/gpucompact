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
#include "archive_writer.h"
#include "archive_reader.h"
#include "path_utils.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

inline FileEntry get_file_entry(const char *blob, size_t index) {
  FileEntry fe;
  std::memcpy(&fe, blob + index * sizeof(FileEntry), sizeof(FileEntry));
  return fe;
}

inline ChunkEntry get_chunk_entry(const char *blob, size_t index) {
  ChunkEntry ce;
  std::memcpy(&ce, blob + index * sizeof(ChunkEntry), sizeof(ChunkEntry));
  return ce;
}

static void default_cli_progress(uint64_t processed, uint64_t total,
                                 uint64_t compressed, double elapsed_sec) {
  double pct = (total > 0) ? ((double)processed / total) * 100.0 : 0.0;
  double speed_mbs = (elapsed_sec > 0)
                         ? ((double)processed / (1024.0 * 1024.0)) / elapsed_sec
                         : 0.0;
  double ratio = (compressed > 0) ? ((double)processed / compressed) : 1.0;

  std::cout << "\r  -> Processed: " << std::fixed << std::setprecision(1)
            << (processed / 1048576.0) << " MB / " << (total / 1048576.0)
            << " MB (" << (int)pct << "%) | " << std::setprecision(2)
            << speed_mbs << " MB/s | Ratio: " << ratio << "x   " << std::flush;
}

// -------------------------------------------------------------------------
// PARALLEL FILE PRE-FETCHING & STAGING INFRASTRUCTURE
// -------------------------------------------------------------------------

struct FileWorkItem {
  std::string full_path;
  std::string rel_path;
  uint64_t file_size;
  uint64_t global_offset;
  uint64_t path_offset;
};

struct FileSliceItem {
  std::string full_path;
  uint64_t file_offset;
  uint64_t chunk_buffer_offset;
  size_t read_bytes;
};

// Work-stealing parallel file fetcher across CPU worker threads
static void read_chunk_files_parallel(const std::vector<FileSliceItem> &slices,
                                      uint8_t *chunk_buffer,
                                      size_t num_threads = 16) {
  if (slices.empty())
    return;

  size_t total_slices = slices.size();
  size_t actual_threads =
      std::min(num_threads, std::max(size_t(1), total_slices));

  std::vector<std::thread> workers;
  workers.reserve(actual_threads);
  std::atomic<size_t> next_index{0};

  for (size_t t = 0; t < actual_threads; ++t) {
    workers.emplace_back([&]() {
      while (true) {
        size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
        if (idx >= total_slices)
          break;

        const auto &slice = slices[idx];
        // FIXED: Use utf8_to_path to support UTF-8/Unicode paths on Windows
        // std::ifstream
        std::ifstream fin(utf8_to_path(slice.full_path), std::ios::binary);
        if (fin.is_open()) {
          if (slice.file_offset > 0) {
            fin.seekg(slice.file_offset);
          }
          fin.read(reinterpret_cast<char *>(chunk_buffer +
                                            slice.chunk_buffer_offset),
                   slice.read_bytes);
        }
      }
    });
  }

  for (auto &w : workers) {
    if (w.joinable())
      w.join();
  }
}

static std::vector<FileSliceItem>
build_chunk_slices(const std::vector<FileWorkItem> &items, uint32_t chunk_idx,
                   size_t macro_bytes, uint64_t total_uncomp_bytes,
                   size_t &start_file_hint) {
  std::vector<FileSliceItem> slices;
  uint64_t chunk_start_global = (uint64_t)chunk_idx * macro_bytes;
  uint64_t chunk_end_global =
      std::min((uint64_t)(chunk_idx + 1) * macro_bytes, total_uncomp_bytes);

  for (size_t i = start_file_hint; i < items.size(); ++i) {
    const auto &item = items[i];
    uint64_t file_start_global = item.global_offset;
    uint64_t file_end_global = item.global_offset + item.file_size;

    if (file_start_global >= chunk_end_global) {
      break;
    }

    if (file_end_global <= chunk_start_global) {
      start_file_hint = i + 1;
      continue;
    }

    uint64_t overlap_start = std::max(file_start_global, chunk_start_global);
    uint64_t overlap_end = std::min(file_end_global, chunk_end_global);
    uint64_t read_bytes = overlap_end - overlap_start;

    FileSliceItem slice;
    slice.full_path = item.full_path;
    slice.file_offset = overlap_start - file_start_global;
    slice.chunk_buffer_offset = overlap_start - chunk_start_global;
    slice.read_bytes = (size_t)read_bytes;

    slices.push_back(slice);
  }

  return slices;
}

// -------------------------------------------------------------------------
// CORE ARCHIVE CREATION (DOUBLE-BUFFERED & MULTI-THREADED)
// -------------------------------------------------------------------------

TimingStats build_archive_explicit(
    const std::vector<std::pair<std::string, std::string>> &files_to_pack,
    const std::string &output_path, const LaunchConfig &config,
    CompressionContext *external_ctx, bool quiet,
    ProgressCallback progress_cb) {
  auto t_start = std::chrono::high_resolution_clock::now();
  float gpu_ms = 0.0f;

  std::unique_ptr<CompressionContext> own_ctx;
  CompressionContext *ctx = external_ctx;
  if (!ctx) {
    own_ctx = std::make_unique<CompressionContext>(
        config.macro_mb * 1024 * 1024, config.mini_size, config.L);
    ctx = own_ctx.get();
  }

  size_t macro_bytes = config.macro_mb * 1024 * 1024;
  unsigned int worker_threads =
      std::max(4u, std::thread::hardware_concurrency());

  std::vector<FileWorkItem> items;
  items.reserve(files_to_pack.size());

  std::string string_table;
  string_table.reserve(files_to_pack.size() * 40);

  std::vector<FileEntry> file_entries;
  file_entries.reserve(files_to_pack.size());

  uint64_t total_uncomp_bytes = 0;

  for (const auto &[full_p, rel_p] : files_to_pack) {
    std::error_code ec;
    // FIXED: Use utf8_to_path for Windows Unicode file_size query
    uint64_t f_size = fs::file_size(utf8_to_path(full_p), ec);
    if (ec)
      continue;

    uint64_t p_off = string_table.size();
    string_table.append(rel_p);
    string_table.push_back('\0');

    FileWorkItem item;
    item.full_path = full_p;
    item.rel_path = rel_p;
    item.file_size = f_size;
    item.global_offset = total_uncomp_bytes;
    item.path_offset = p_off;

    items.push_back(item);

    uint32_t start_c = (uint32_t)(total_uncomp_bytes / macro_bytes);
    uint64_t end_off = total_uncomp_bytes + f_size;
    uint32_t end_c = (uint32_t)((end_off + macro_bytes - 1) / macro_bytes);
    uint32_t chunk_span = (end_c > start_c) ? (end_c - start_c) : 1u;

    FileEntry fe;
    fe.path_offset = p_off;
    fe.file_size = f_size;
    fe.chunk_start = start_c;
    fe.chunk_count = (f_size == 0) ? 0u : std::max(1u, chunk_span);
    file_entries.push_back(fe);

    total_uncomp_bytes += f_size;
  }

  std::ofstream fout(utf8_to_path(output_path), std::ios::binary);
  if (!fout.is_open())
    throw std::runtime_error("Cannot create archive file: " + output_path);

  fout.write("GCMP", 4);

  if (items.empty() || total_uncomp_bytes == 0) {
    uint64_t gtoc_offset = fout.tellp();
    fout.write(string_table.data(), string_table.size());
    fout.write((char *)file_entries.data(),
               file_entries.size() * sizeof(FileEntry));

    GTOCFooter footer;
    footer.gtoc_offset = gtoc_offset;
    footer.num_files = (uint32_t)file_entries.size();
    footer.num_chunks = 0;
    footer.macro_mb = config.macro_mb;
    footer.mini_size = config.mini_size;
    footer.L_state = config.L;
    std::memcpy(footer.magic, "GTOC", 4);

    fout.write((char *)&footer, sizeof(GTOCFooter));
    fout.close();

    auto t_end = std::chrono::high_resolution_clock::now();
    float wall_ms =
        std::chrono::duration<float, std::milli>(t_end - t_start).count();
    return TimingStats{wall_ms, 0.0f};
  }

  uint32_t total_chunks =
      (uint32_t)((total_uncomp_bytes + macro_bytes - 1) / macro_bytes);
  std::vector<ChunkEntry> chunk_entries;
  chunk_entries.reserve(total_chunks);

  uint8_t *staging_buf[2] = {nullptr, nullptr};
  cudaMallocHost(&staging_buf[0], macro_bytes);
  cudaMallocHost(&staging_buf[1], macro_bytes);

  uint64_t bytes_processed = 0, bytes_compressed = 0;
  size_t start_hint_0 = 0, start_hint_1 = 0;

  auto slices_curr = build_chunk_slices(items, 0, macro_bytes,
                                        total_uncomp_bytes, start_hint_0);
  read_chunk_files_parallel(slices_curr, staging_buf[0], worker_threads);

  start_hint_1 = start_hint_0;

  for (uint32_t c = 0; c < total_chunks; ++c) {
    uint32_t active_idx = c % 2;
    uint32_t next_idx = (c + 1) % 2;

    uint64_t chunk_len =
        std::min((uint64_t)macro_bytes,
                 total_uncomp_bytes - ((uint64_t)c * macro_bytes));

    std::future<void> next_fetch_future;
    if (c + 1 < total_chunks) {
      auto slices_next = build_chunk_slices(items, c + 1, macro_bytes,
                                            total_uncomp_bytes, start_hint_1);
      next_fetch_future = std::async(
          std::launch::async,
          [slices_next, buf = staging_buf[next_idx], worker_threads]() {
            read_chunk_files_parallel(slices_next, buf, worker_threads);
          });
    }

    ctx->bytes_read = (int)chunk_len;
    std::memcpy(ctx->host_in, staging_buf[active_idx], chunk_len);
    ctx->compress_chunk(config.threads_comp, config.mini_size);

    float ms = 0.0f;
    cudaEventElapsedTime(&ms, ctx->e_start, ctx->e_end);
    gpu_ms += ms;

    uint64_t chunk_start = fout.tellp();
    fout.write((char *)ctx->host_out, ctx->comp_size);

    ChunkEntry ce;
    ce.payload_offset = chunk_start;
    ce.comp_size = ctx->comp_size;
    ce.uncomp_size = (uint32_t)chunk_len;
    ce.primary_idx = ctx->primary_idx;
    ce.is_raw = (uint8_t)ctx->is_raw;
    ce.reserved[0] = ce.reserved[1] = ce.reserved[2] = 0;
    ce.gpu_hash = ctx->gpu_hash;
    chunk_entries.push_back(ce);

    bytes_processed += chunk_len;
    bytes_compressed += ctx->comp_size;

    if (progress_cb) {
      double elapsed = std::chrono::duration<double>(
                           std::chrono::high_resolution_clock::now() - t_start)
                           .count();
      progress_cb(bytes_processed, total_uncomp_bytes, bytes_compressed,
                  elapsed);
    } else if (!quiet) {
      default_cli_progress(
          bytes_processed, total_uncomp_bytes, bytes_compressed,
          std::chrono::duration<double>(
              std::chrono::high_resolution_clock::now() - t_start)
              .count());
    }

    if (next_fetch_future.valid()) {
      next_fetch_future.wait();
    }
  }

  cudaFreeHost(staging_buf[0]);
  cudaFreeHost(staging_buf[1]);

  uint64_t gtoc_offset = fout.tellp();
  fout.write(string_table.data(), string_table.size());
  fout.write((char *)file_entries.data(),
             file_entries.size() * sizeof(FileEntry));
  fout.write((char *)chunk_entries.data(),
             chunk_entries.size() * sizeof(ChunkEntry));

  GTOCFooter footer;
  footer.gtoc_offset = gtoc_offset;
  footer.num_files = (uint32_t)file_entries.size();
  footer.num_chunks = (uint32_t)chunk_entries.size();
  footer.macro_mb = config.macro_mb;
  footer.mini_size = config.mini_size;
  footer.L_state = config.L;
  std::memcpy(footer.magic, "GTOC", 4);

  fout.write((char *)&footer, sizeof(GTOCFooter));
  fout.close();

  auto t_end = std::chrono::high_resolution_clock::now();
  float wall_ms =
      std::chrono::duration<float, std::milli>(t_end - t_start).count();

  if (!quiet)
    std::cout << "\n" << std::endl;

  return TimingStats{wall_ms, gpu_ms};
}

TimingStats build_archive(const std::vector<std::string> &input_paths,
                          const std::string &output_path,
                          const LaunchConfig &config,
                          CompressionContext *external_ctx, bool quiet,
                          ProgressCallback progress_cb) {
  std::vector<std::pair<std::string, std::string>> files_to_pack;
  for (const auto &t_str : input_paths) {
    fs::path t = utf8_to_path(t_str);
    if (!fs::exists(t)) {
      throw std::runtime_error("Input path does not exist: " + t_str);
    }
    if (fs::is_regular_file(t)) {
      files_to_pack.push_back({path_to_utf8(t), path_to_utf8(t.filename())});
    } else if (fs::is_directory(t)) {
      std::string base_parent = path_to_utf8(t.parent_path());
      for (const auto &entry : fs::recursive_directory_iterator(t)) {
        if (entry.is_regular_file()) {
          std::string full_p = path_to_utf8(entry.path());
          std::string rel_p =
              path_to_utf8(fs::relative(entry.path(), t.parent_path()));
          for (auto &c : rel_p)
            if (c == '\\')
              c = '/';
          files_to_pack.push_back({full_p, rel_p});
        }
      }
    }
  }

  if (files_to_pack.empty()) {
    throw std::runtime_error("No valid regular files found to compress.");
  }

  return build_archive_explicit(files_to_pack, output_path, config,
                                external_ctx, quiet, progress_cb);
}

void remove_from_archive(const std::string &archive_path,
                         const std::vector<std::string> &files_to_remove,
                         ProgressCallback progress_cb, bool allow_empty) {
  std::ifstream fin(utf8_to_path(archive_path), std::ios::binary);
  if (!fin.is_open())
    throw std::runtime_error("Cannot open archive: " + archive_path);

  fin.seekg(-((int)sizeof(GTOCFooter)), std::ios::end);
  GTOCFooter footer;
  fin.read((char *)&footer, sizeof(GTOCFooter));

  uint64_t total_file_size = fs::file_size(utf8_to_path(archive_path));
  uint64_t gtoc_size =
      total_file_size - sizeof(GTOCFooter) - footer.gtoc_offset;

  fin.seekg(footer.gtoc_offset);
  std::vector<char> gtoc_blob(gtoc_size);
  fin.read(gtoc_blob.data(), gtoc_size);

  uint64_t fe_size = footer.num_files * sizeof(FileEntry);
  uint64_t ce_size = footer.num_chunks * sizeof(ChunkEntry);
  uint64_t str_size = gtoc_size - fe_size - ce_size;

  const char *string_table = gtoc_blob.data();
  const char *fe_blob = gtoc_blob.data() + str_size;
  const char *ce_blob = gtoc_blob.data() + str_size + fe_size;

  auto t_start = std::chrono::high_resolution_clock::now();

  std::unordered_set<std::string> remove_set;
  remove_set.reserve(files_to_remove.size());

  for (const auto &pat : files_to_remove) {
    std::string norm = normalize_archive_path(pat);
    if (!norm.empty()) {
      remove_set.insert(norm);
    }
  }

  if (remove_set.empty())
    return;

  std::vector<std::pair<FileEntry, std::string>> surviving_files;
  std::set<uint32_t> surviving_chunks;

  uint64_t total_archive_uncomp_bytes = 0;
  for (uint32_t i = 0; i < footer.num_files; i++) {
    FileEntry fe = get_file_entry(fe_blob, i);
    total_archive_uncomp_bytes += fe.file_size;
  }

  for (uint32_t i = 0; i < footer.num_files; i++) {
    FileEntry fe = get_file_entry(fe_blob, i);
    std::string rel_p = ensure_utf8(string_table + fe.path_offset);
    std::string norm_p = normalize_archive_path(rel_p);

    bool should_remove = false;
    std::string curr = norm_p;
    while (!curr.empty()) {
      if (remove_set.find(curr) != remove_set.end()) {
        should_remove = true;
        break;
      }
      size_t pos = curr.find_last_of('/');
      if (pos == std::string::npos) {
        break;
      }
      curr = curr.substr(0, pos);
    }

    if (!should_remove) {
      surviving_files.push_back({fe, rel_p});
      for (uint32_t c = fe.chunk_start; c < fe.chunk_start + fe.chunk_count;
           c++) {
        surviving_chunks.insert(c);
      }
    }

    if (progress_cb && (i % 500 == 0 || i == footer.num_files - 1)) {
      double elapsed = std::chrono::duration<double>(
                           std::chrono::high_resolution_clock::now() - t_start)
                           .count();
      uint64_t est_processed = (uint64_t)((double)(i + 1) / footer.num_files *
                                          total_archive_uncomp_bytes * 0.05);
      progress_cb(est_processed,
                  total_archive_uncomp_bytes > 0 ? total_archive_uncomp_bytes
                                                 : 1,
                  0, elapsed);
    }
  }

  if (surviving_files.empty() && !allow_empty)
    throw std::runtime_error("Cannot delete all files from archive.");

  std::string tmp_path = archive_path + ".tmp_remove";
  std::ofstream fout(utf8_to_path(tmp_path), std::ios::binary);
  fout.write("GCMP", 4);

  std::unordered_map<uint32_t, uint32_t> chunk_id_map;
  std::vector<ChunkEntry> new_chunk_entries;
  uint64_t total_uncomp_bytes = 0;
  for (uint32_t old_c_idx : surviving_chunks) {
    ChunkEntry ce = get_chunk_entry(ce_blob, old_c_idx);
    total_uncomp_bytes += ce.uncomp_size;
  }

  uint64_t bytes_processed = 0;
  uint64_t bytes_compressed = 0;
  uint32_t new_c_idx = 0;

  for (uint32_t old_c_idx : surviving_chunks) {
    ChunkEntry ce = get_chunk_entry(ce_blob, old_c_idx);
    chunk_id_map[old_c_idx] = new_c_idx++;

    fin.seekg(ce.payload_offset);
    std::vector<char> buf(ce.comp_size);
    fin.read(buf.data(), ce.comp_size);

    uint64_t new_off = fout.tellp();
    fout.write(buf.data(), ce.comp_size);

    ChunkEntry new_ce = ce;
    new_ce.payload_offset = new_off;
    new_chunk_entries.push_back(new_ce);

    bytes_processed += ce.uncomp_size;
    bytes_compressed += ce.comp_size;

    if (progress_cb) {
      double elapsed = std::chrono::duration<double>(
                           std::chrono::high_resolution_clock::now() - t_start)
                           .count();
      progress_cb(bytes_processed, total_uncomp_bytes, bytes_compressed,
                  elapsed);
    }
  }

  std::string new_string_table;
  std::vector<FileEntry> new_file_entries;

  for (const auto &[fe, rel_p] : surviving_files) {
    uint64_t p_off = new_string_table.size();
    new_string_table.append(rel_p);
    new_string_table.push_back('\0');

    FileEntry new_fe = fe;
    new_fe.path_offset = p_off;
    auto it = chunk_id_map.find(fe.chunk_start);
    new_fe.chunk_start = (it != chunk_id_map.end()) ? it->second : 0;
    new_file_entries.push_back(new_fe);
  }

  uint64_t new_gtoc_off = fout.tellp();
  fout.write(new_string_table.data(), new_string_table.size());
  fout.write((char *)new_file_entries.data(),
             new_file_entries.size() * sizeof(FileEntry));
  fout.write((char *)new_chunk_entries.data(),
             new_chunk_entries.size() * sizeof(ChunkEntry));

  GTOCFooter new_footer = footer;
  new_footer.gtoc_offset = new_gtoc_off;
  new_footer.num_files = (uint32_t)new_file_entries.size();
  new_footer.num_chunks = (uint32_t)new_chunk_entries.size();
  fout.write((char *)&new_footer, sizeof(GTOCFooter));

  fin.close();
  fout.close();

  std::error_code ec;
  fs::remove(utf8_to_path(archive_path), ec);
  fs::rename(utf8_to_path(tmp_path), utf8_to_path(archive_path), ec);
  if (ec) {
    fs::copy_file(utf8_to_path(tmp_path), utf8_to_path(archive_path),
                  fs::copy_options::overwrite_existing, ec);
    fs::remove(utf8_to_path(tmp_path), ec);
  }
}

void append_to_archive(const std::string &archive_path,
                       const std::vector<std::string> &input_paths,
                       LaunchConfig config, const std::string &collision_mode,
                       ProgressCallback progress_cb) {
  fs::path arch_abs = fs::absolute(utf8_to_path(archive_path));
  std::vector<std::string> valid_inputs;
  for (const auto &p : input_paths) {
    fs::path p_path = utf8_to_path(p);
    if (!fs::exists(p_path) || fs::absolute(p_path) == arch_abs)
      continue;
    valid_inputs.push_back(p);
  }
  if (valid_inputs.empty())
    return;

  std::vector<std::pair<std::string, std::string>> expanded_incoming;
  for (const auto &t_str : valid_inputs) {
    fs::path t = utf8_to_path(t_str);
    if (fs::is_regular_file(t)) {
      expanded_incoming.push_back(
          {path_to_utf8(t), path_to_utf8(t.filename())});
    } else if (fs::is_directory(t)) {
      for (const auto &entry : fs::recursive_directory_iterator(t)) {
        if (entry.is_regular_file()) {
          std::string full_p = path_to_utf8(entry.path());
          std::string rel_p =
              path_to_utf8(fs::relative(entry.path(), t.parent_path()));
          for (auto &c : rel_p)
            if (c == '\\')
              c = '/';
          expanded_incoming.push_back({full_p, rel_p});
        }
      }
    }
  }

  if (expanded_incoming.empty())
    return;

  std::ifstream fin(utf8_to_path(archive_path), std::ios::binary);
  if (!fin.is_open())
    throw std::runtime_error("Cannot open archive: " + archive_path);

  fin.seekg(-((int)sizeof(GTOCFooter)), std::ios::end);
  GTOCFooter ft_orig;
  fin.read((char *)&ft_orig, sizeof(GTOCFooter));

  if (std::string(ft_orig.magic, 4) != "GTOC") {
    fin.close();
    throw std::runtime_error("Corrupted archive GTOC footer: " + archive_path);
  }

  uint64_t total_sz = fs::file_size(utf8_to_path(archive_path));
  uint64_t gtoc_sz = total_sz - sizeof(GTOCFooter) - ft_orig.gtoc_offset;

  fin.seekg(ft_orig.gtoc_offset);
  std::vector<char> gtoc_blob(gtoc_sz);
  fin.read(gtoc_blob.data(), gtoc_sz);
  fin.close();

  config.macro_mb = ft_orig.macro_mb;
  config.mini_size = ft_orig.mini_size;
  config.L = ft_orig.L_state;

  uint64_t fe_sz = ft_orig.num_files * sizeof(FileEntry);
  uint64_t ce_sz = ft_orig.num_chunks * sizeof(ChunkEntry);
  uint64_t str_sz = gtoc_sz - fe_sz - ce_sz;

  const char *string_table = gtoc_blob.data();
  const char *fe_blob = gtoc_blob.data() + str_sz;
  const char *ce_blob = gtoc_blob.data() + str_sz + fe_sz;

  std::set<std::string> existing_set;
  for (uint32_t i = 0; i < ft_orig.num_files; i++) {
    FileEntry fe = get_file_entry(fe_blob, i);
    std::string rel_p(string_table + fe.path_offset);
    for (char &c : rel_p)
      if (c == '\\')
        c = '/';
    existing_set.insert(rel_p);
  }

  std::vector<std::string> files_to_remove;
  std::vector<std::pair<std::string, std::string>> final_files_to_pack;

  for (auto &[full_p, rel_p] : expanded_incoming) {
    for (char &c : rel_p)
      if (c == '\\')
        c = '/';

    if (existing_set.count(rel_p)) {
      if (collision_mode == "skip") {
        continue;
      } else if (collision_mode == "replace") {
        files_to_remove.push_back(rel_p);
        final_files_to_pack.push_back({full_p, rel_p});
      } else if (collision_mode == "rename") {
        fs::path p_obj = utf8_to_path(rel_p);
        std::string base = path_to_utf8(p_obj.stem());
        std::string ext = path_to_utf8(p_obj.extension());
        std::string parent_dir = path_to_utf8(p_obj.parent_path());
        for (char &c : parent_dir)
          if (c == '\\')
            c = '/';

        int counter = 1;
        std::string new_rel =
            parent_dir.empty()
                ? (base + " (" + std::to_string(counter) + ")" + ext)
                : (parent_dir + "/" + base + " (" + std::to_string(counter) +
                   ")" + ext);

        while (existing_set.count(new_rel)) {
          counter++;
          new_rel = parent_dir.empty()
                        ? (base + " (" + std::to_string(counter) + ")" + ext)
                        : (parent_dir + "/" + base + " (" +
                           std::to_string(counter) + ")" + ext);
        }
        existing_set.insert(new_rel);
        final_files_to_pack.push_back({full_p, new_rel});
      }
    } else {
      existing_set.insert(rel_p);
      final_files_to_pack.push_back({full_p, rel_p});
    }
  }

  if (final_files_to_pack.empty())
    return;

  if (!files_to_remove.empty()) {
    remove_from_archive(archive_path, files_to_remove, nullptr, true);
  }

  std::ifstream f_read(utf8_to_path(archive_path), std::ios::binary);
  f_read.seekg(-((int)sizeof(GTOCFooter)), std::ios::end);
  GTOCFooter ft_cur;
  f_read.read((char *)&ft_cur, sizeof(GTOCFooter));

  uint64_t cur_gtoc_sz = fs::file_size(utf8_to_path(archive_path)) -
                         sizeof(GTOCFooter) - ft_cur.gtoc_offset;
  f_read.seekg(ft_cur.gtoc_offset);
  std::vector<char> cur_gtoc(cur_gtoc_sz);
  f_read.read(cur_gtoc.data(), cur_gtoc_sz);
  f_read.close();

  uint64_t cur_fe_sz = ft_cur.num_files * sizeof(FileEntry);
  uint64_t cur_ce_sz = ft_cur.num_chunks * sizeof(ChunkEntry);
  uint64_t cur_str_sz = cur_gtoc_sz - cur_fe_sz - cur_ce_sz;

  std::string cur_str_tbl(cur_gtoc.data(), cur_str_sz);
  const char *cur_fe_ptr = cur_gtoc.data() + cur_str_sz;
  const char *cur_ce_ptr = cur_gtoc.data() + cur_str_sz + cur_fe_sz;

  std::vector<FileEntry> merged_fe;
  merged_fe.reserve(ft_cur.num_files + final_files_to_pack.size());
  for (uint32_t i = 0; i < ft_cur.num_files; i++)
    merged_fe.push_back(get_file_entry(cur_fe_ptr, i));

  std::vector<ChunkEntry> merged_ce;
  merged_ce.reserve(ft_cur.num_chunks + 32);
  for (uint32_t i = 0; i < ft_cur.num_chunks; i++)
    merged_ce.push_back(get_chunk_entry(cur_ce_ptr, i));

  std::fstream f_append(utf8_to_path(archive_path),
                        std::ios::in | std::ios::out | std::ios::binary);
  f_append.seekp(ft_cur.gtoc_offset);

  CompressionContext comp_ctx(config.macro_mb * 1024 * 1024, config.mini_size,
                              config.L);
  size_t macro_bytes = config.macro_mb * 1024 * 1024;
  unsigned int worker_threads =
      std::max(4u, std::thread::hardware_concurrency());

  std::vector<FileWorkItem> items;
  items.reserve(final_files_to_pack.size());

  uint64_t append_global_bytes = 0;

  for (const auto &[full_p, rel_p] : final_files_to_pack) {
    std::error_code ec;
    // FIXED: Use utf8_to_path for Windows Unicode file_size query
    uint64_t f_size = fs::file_size(utf8_to_path(full_p), ec);
    if (ec)
      continue;

    uint64_t p_off = cur_str_tbl.size();
    cur_str_tbl.append(rel_p);
    cur_str_tbl.push_back('\0');

    FileWorkItem item;
    item.full_path = full_p;
    item.rel_path = rel_p;
    item.file_size = f_size;
    item.global_offset = append_global_bytes;
    item.path_offset = p_off;

    items.push_back(item);

    uint32_t start_c =
        ft_cur.num_chunks + (uint32_t)(append_global_bytes / macro_bytes);
    uint64_t end_off = append_global_bytes + f_size;
    uint32_t end_c = ft_cur.num_chunks +
                     (uint32_t)((end_off + macro_bytes - 1) / macro_bytes);
    uint32_t chunk_span = (end_c > start_c) ? (end_c - start_c) : 1u;

    FileEntry fe;
    fe.path_offset = p_off;
    fe.file_size = f_size;
    fe.chunk_start = start_c;
    fe.chunk_count = (f_size == 0) ? 0u : std::max(1u, chunk_span);
    merged_fe.push_back(fe);

    append_global_bytes += f_size;
  }

  if (!items.empty()) {
    uint32_t total_append_chunks =
        (uint32_t)((append_global_bytes + macro_bytes - 1) / macro_bytes);

    uint8_t *staging_buf[2] = {nullptr, nullptr};
    cudaMallocHost(&staging_buf[0], macro_bytes);
    cudaMallocHost(&staging_buf[1], macro_bytes);

    auto t_start = std::chrono::high_resolution_clock::now();
    uint64_t bytes_processed = 0, bytes_compressed = 0;
    size_t start_hint_0 = 0, start_hint_1 = 0;

    auto slices_curr = build_chunk_slices(items, 0, macro_bytes,
                                          append_global_bytes, start_hint_0);
    read_chunk_files_parallel(slices_curr, staging_buf[0], worker_threads);
    start_hint_1 = start_hint_0;

    for (uint32_t c = 0; c < total_append_chunks; ++c) {
      uint32_t active_idx = c % 2;
      uint32_t next_idx = (c + 1) % 2;

      uint64_t chunk_len =
          std::min((uint64_t)macro_bytes,
                   append_global_bytes - ((uint64_t)c * macro_bytes));

      std::future<void> next_fetch_future;
      if (c + 1 < total_append_chunks) {
        auto slices_next = build_chunk_slices(
            items, c + 1, macro_bytes, append_global_bytes, start_hint_1);
        next_fetch_future = std::async(
            std::launch::async,
            [slices_next, buf = staging_buf[next_idx], worker_threads]() {
              read_chunk_files_parallel(slices_next, buf, worker_threads);
            });
      }

      comp_ctx.bytes_read = (int)chunk_len;
      std::memcpy(comp_ctx.host_in, staging_buf[active_idx], chunk_len);
      comp_ctx.compress_chunk(config.threads_comp, config.mini_size);

      uint64_t chunk_start = f_append.tellp();
      f_append.write((char *)comp_ctx.host_out, comp_ctx.comp_size);

      ChunkEntry ce;
      ce.payload_offset = chunk_start;
      ce.comp_size = comp_ctx.comp_size;
      ce.uncomp_size = (uint32_t)chunk_len;
      ce.primary_idx = comp_ctx.primary_idx;
      ce.is_raw = (uint8_t)comp_ctx.is_raw;
      ce.reserved[0] = ce.reserved[1] = ce.reserved[2] = 0;
      ce.gpu_hash = comp_ctx.gpu_hash;
      merged_ce.push_back(ce);

      bytes_processed += chunk_len;
      bytes_compressed += comp_ctx.comp_size;

      if (progress_cb) {
        double elapsed =
            std::chrono::duration<double>(
                std::chrono::high_resolution_clock::now() - t_start)
                .count();
        progress_cb(bytes_processed, append_global_bytes, bytes_compressed,
                    elapsed);
      }

      if (next_fetch_future.valid()) {
        next_fetch_future.wait();
      }
    }

    cudaFreeHost(staging_buf[0]);
    cudaFreeHost(staging_buf[1]);
  }

  uint64_t new_gtoc_offset = f_append.tellp();
  f_append.write(cur_str_tbl.data(), cur_str_tbl.size());
  f_append.write((char *)merged_fe.data(),
                 merged_fe.size() * sizeof(FileEntry));
  f_append.write((char *)merged_ce.data(),
                 merged_ce.size() * sizeof(ChunkEntry));

  GTOCFooter new_footer = ft_cur;
  new_footer.gtoc_offset = new_gtoc_offset;
  new_footer.num_files = (uint32_t)merged_fe.size();
  new_footer.num_chunks = (uint32_t)merged_ce.size();
  f_append.write((char *)&new_footer, sizeof(GTOCFooter));
  f_append.close();
}