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
#include "archive_reader.h"
#include "path_utils.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace fs = std::filesystem;

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

template <typename T> class WorkQueue {
private:
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cv_push_;
  std::condition_variable cv_pop_;
  size_t capacity_;
  bool shutdown_ = false;

public:
  WorkQueue(size_t capacity) : capacity_(capacity) {}

  void cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = true;
    cv_push_.notify_all();
    cv_pop_.notify_all();
  }

  bool push(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_push_.wait(lock,
                  [this] { return queue_.size() < capacity_ || shutdown_; });
    if (shutdown_)
      return false;
    queue_.push(item);
    cv_pop_.notify_one();
    return true;
  }

  bool pop(T &item) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_pop_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
    if (queue_.empty() || shutdown_)
      return false;
    item = queue_.front();
    queue_.pop();
    cv_pop_.notify_one();
    return true;
  }
};

static std::string sanitize_relative_path(std::string rel_path) {
#ifdef _WIN32
  for (char &c : rel_path) {
    if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
        c == '|')
      c = '_';
  }
#endif
  return rel_path;
}

static fs::path get_safe_abs_path(const fs::path &p) {
  fs::path abs_p = fs::absolute(p);
#ifdef _WIN32
  std::wstring wstr = abs_p.wstring();
  for (wchar_t &wc : wstr) {
    if (wc == L'/')
      wc = L'\\';
  }
  if (wstr.length() >= 240 && wstr.rfind(L"\\\\?\\", 0) != 0) {
    return fs::path(L"\\\\?\\" + wstr);
  }
  return fs::path(wstr);
#else
  return abs_p;
#endif
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

void inspect_archive_cli(const std::string &archive_path) {
  std::ifstream fin(archive_path, std::ios::binary);
  if (!fin.is_open())
    throw std::runtime_error("Cannot open file: " + archive_path);

  char magic[4];
  fin.read(magic, 4);
  if (std::string(magic, 4) != "GCMP")
    throw std::runtime_error("Invalid archive magic header.");

  fin.seekg(-((int)sizeof(GTOCFooter)), std::ios::end);
  GTOCFooter footer;
  fin.read((char *)&footer, sizeof(GTOCFooter));
  if (std::string(footer.magic, 4) != "GTOC")
    throw std::runtime_error("Corrupted archive GTOC footer.");

  uint64_t total_file_size = fs::file_size(archive_path);
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

  uint64_t total_uncomp = 0, total_comp = 0;
  for (uint32_t i = 0; i < footer.num_files; i++) {
    FileEntry fe = get_file_entry(fe_blob, i);
    total_uncomp += fe.file_size;
  }
  for (uint32_t i = 0; i < footer.num_chunks; i++) {
    ChunkEntry ce = get_chunk_entry(ce_blob, i);
    total_comp += ce.comp_size;
  }

  double overall_ratio =
      (total_comp > 0) ? ((double)total_uncomp / total_comp) : 1.0;

  std::cout << "\n============================================================="
               "==================="
            << std::endl;
  std::cout << "GPUCOMPACT ARCHIVE METADATA: "
            << fs::path(archive_path).filename().string() << std::endl;
  std::cout << "==============================================================="
               "================="
            << std::endl;
  std::cout << "Archive Size        : " << std::fixed << std::setprecision(2)
            << (total_file_size / 1048576.0) << " MB (" << total_file_size
            << " bytes)" << std::endl;
  std::cout << "Format Hyperparams  : Macro=" << footer.macro_mb
            << " MB, Mini=" << footer.mini_size << " B, L=" << footer.L_state
            << std::endl;
  std::cout << "Total Files         : " << footer.num_files << std::endl;
  std::cout << "Total Macro-Chunks  : " << footer.num_chunks << std::endl;
  std::cout << "Uncompressed Total  : " << (total_uncomp / 1048576.0) << " MB ("
            << total_uncomp << " bytes)" << std::endl;
  std::cout << "Compressed Total    : " << (total_comp / 1048576.0) << " MB ("
            << total_comp << " bytes)" << std::endl;
  std::cout << "Overall Ratio       : " << overall_ratio << "x" << std::endl;
  std::cout << "---------------------------------------------------------------"
               "-----------------"
            << std::endl;
  std::cout << "| " << std::left << std::setw(35) << "Filename"
            << " | " << std::setw(15) << "Original Size"
            << " | " << std::setw(15) << "Compressed"
            << " | " << std::setw(8) << "Ratio" << " |" << std::endl;
  std::cout << "---------------------------------------------------------------"
               "-----------------"
            << std::endl;

  for (uint32_t i = 0; i < footer.num_files; i++) {
    FileEntry fe = get_file_entry(fe_blob, i);
    std::string rel_p = ensure_utf8(string_table + fe.path_offset);

    uint64_t f_uncomp = fe.file_size;
    double f_comp_weighted = 0.0;

    uint64_t bytes_remaining = f_uncomp;
    for (uint32_t c = fe.chunk_start;
         c < fe.chunk_start + fe.chunk_count && c < footer.num_chunks; c++) {
      ChunkEntry ce = get_chunk_entry(ce_blob, c);
      if (ce.uncomp_size == 0)
        continue;

      uint64_t bytes_in_chunk =
          std::min(bytes_remaining, (uint64_t)ce.uncomp_size);
      double weight = (double)bytes_in_chunk / ce.uncomp_size;
      f_comp_weighted += ce.comp_size * weight;
      bytes_remaining -= bytes_in_chunk;
    }

    uint64_t f_comp_display = (uint64_t)f_comp_weighted;
    double ratio =
        (f_comp_display > 0) ? ((double)f_uncomp / f_comp_display) : 1.0;

    // FIXED: Print full rel_p without truncating to 32 chars
    std::cout << "| " << std::left << std::setw(50) << rel_p << " | "
              << std::right << std::setw(12) << std::fixed
              << std::setprecision(2) << (f_uncomp / 1048576.0) << " MB"
              << " | " << std::setw(12) << (f_comp_display / 1048576.0) << " MB"
              << " | " << std::setw(7) << ratio << "x |" << std::endl;
  }
  std::cout << "==============================================================="
               "================="
            << std::endl;
}

TimingStats extract_archive(
    const std::string &input_path, const std::string &output_dir,
    LaunchConfig config, std::vector<DecompressionContext *> *external_contexts,
    const std::string &extract_file, bool quiet, ProgressCallback progress_cb,
    CorruptionCallback corruption_cb, const std::string &collision_mode) {
  auto t_start = std::chrono::high_resolution_clock::now();
  float gpu_ms = 0.0f;
  uint32_t corrupted_chunk_count = 0;

  std::ifstream fin(input_path, std::ios::binary);
  if (!fin.is_open())
    throw std::runtime_error("Cannot open archive: " + input_path);

  char magic[4];
  fin.read(magic, 4);
  if (std::string(magic, 4) != "GCMP")
    throw std::runtime_error("Invalid archive magic header.");

  fin.seekg(-((int)sizeof(GTOCFooter)), std::ios::end);
  GTOCFooter footer;
  fin.read((char *)&footer, sizeof(GTOCFooter));
  if (std::string(footer.magic, 4) != "GTOC")
    throw std::runtime_error("Corrupted archive GTOC footer.");

  config.macro_mb = footer.macro_mb;
  config.mini_size = footer.mini_size;
  config.L = footer.L_state;

  uint64_t macro_bytes = (uint64_t)config.macro_mb * 1024 * 1024;

  fin.seekg(footer.gtoc_offset);
  uint64_t gtoc_size = (uint64_t)fs::file_size(input_path) -
                       sizeof(GTOCFooter) - footer.gtoc_offset;
  std::vector<char> gtoc_blob(gtoc_size);
  fin.read(gtoc_blob.data(), gtoc_size);

  uint64_t fe_size = footer.num_files * sizeof(FileEntry);
  uint64_t ce_size = footer.num_chunks * sizeof(ChunkEntry);
  uint64_t str_size = gtoc_size - fe_size - ce_size;

  const char *string_table = gtoc_blob.data();
  const char *fe_blob = gtoc_blob.data() + str_size;
  const char *ce_blob = gtoc_blob.data() + str_size + fe_size;

  std::vector<std::unique_ptr<DecompressionContext>> own_contexts_storage;
  std::vector<DecompressionContext *> contexts;

  if (external_contexts && !external_contexts->empty()) {
    contexts = *external_contexts;
  } else {
    for (int i = 0; i < 3; i++) {
      auto ctx = std::make_unique<DecompressionContext>(
          config.macro_mb * 1024 * 1024, config.mini_size, config.L);
      contexts.push_back(ctx.get());
      own_contexts_storage.push_back(std::move(ctx));
    }
  }

  fs::create_directories(output_dir);

  std::vector<std::string> target_patterns;
  if (!extract_file.empty()) {
    std::stringstream ss(extract_file);
    std::string token;
    while (std::getline(ss, token, '|')) {
      if (token.empty())
        continue;
      std::string norm = normalize_archive_path(token);
      if (!norm.empty())
        target_patterns.push_back(norm);
    }
  }

  struct MatchedFileItem {
    FileEntry fe;
    uint32_t gtoc_index;
    uint64_t global_offset;
    uint64_t start_chunk_offset;
  };

  std::vector<MatchedFileItem> matched_items;
  uint64_t current_global_off = 0;

  std::vector<uint64_t> chunk_base_offsets(footer.num_chunks, 0);
  uint64_t running_chunk_off = 0;
  for (uint32_t c = 0; c < footer.num_chunks; c++) {
    ChunkEntry ce = get_chunk_entry(ce_blob, c);
    chunk_base_offsets[c] = running_chunk_off;
    running_chunk_off += ce.uncomp_size;
  }

  for (uint32_t i = 0; i < footer.num_files; i++) {
    FileEntry fe = get_file_entry(fe_blob, i);
    std::string rel_p = ensure_utf8(string_table + fe.path_offset);
    std::string norm_p = normalize_archive_path(rel_p);

    uint64_t file_off = current_global_off;
    current_global_off += fe.file_size;

    bool matched = false;
    if (target_patterns.empty()) {
      matched = true;
    } else {
      for (const auto &pat : target_patterns) {
        if (norm_p == pat || norm_p.rfind(pat + "/", 0) == 0) {
          matched = true;
          break;
        }
      }
    }

    if (matched) {
      uint64_t chunk_base = (fe.chunk_start < footer.num_chunks)
                                ? chunk_base_offsets[fe.chunk_start]
                                : 0;
      uint64_t start_chunk_off =
          (file_off >= chunk_base) ? (file_off - chunk_base) : 0;
      matched_items.push_back({fe, i, file_off, start_chunk_off});
    }
  }

  if (!target_patterns.empty() && matched_items.empty()) {
    throw std::runtime_error("Specified item(s) not found inside archive.");
  }

  uint64_t total_target_bytes = 0;
  for (const auto &mitem : matched_items)
    total_target_bytes += mitem.fe.file_size;

  bool is_selective_single_file =
      (matched_items.size() == 1 && !target_patterns.empty());

  if (is_selective_single_file) {
    const auto &mitem = matched_items[0];
    const FileEntry &target_fe = mitem.fe;
    std::string rel_p = ensure_utf8(string_table + target_fe.path_offset);
    std::string safe_rel = sanitize_relative_path(rel_p);

    uint64_t start_chunk_offset = mitem.start_chunk_offset;

    uint32_t c_start = target_fe.chunk_start;
    uint32_t c_count = target_fe.chunk_count;
    uint64_t bytes_left = target_fe.file_size;
    uint64_t bytes_processed = 0, bytes_compressed = 0;

    fs::path out_file_path = get_safe_abs_path(
        utf8_to_path(output_dir) / utf8_to_path(safe_rel).filename());

    if (fs::exists(out_file_path)) {
      if (collision_mode == "skip") {
        return TimingStats{0.0f, 0.0f};
      } else if (collision_mode == "rename") {
        fs::path p_obj = utf8_to_path(out_file_path.string());
        std::string base = path_to_utf8(p_obj.stem());
        std::string ext = path_to_utf8(p_obj.extension());
        fs::path parent_p = out_file_path.parent_path();
        int counter = 1;
        fs::path new_p =
            parent_p /
            utf8_to_path(base + " (" + std::to_string(counter) + ")" + ext);
        while (fs::exists(new_p)) {
          counter++;
          new_p = parent_p / utf8_to_path(base + " (" +
                                          std::to_string(counter) + ")" + ext);
        }
        out_file_path = new_p;
      }
    }

    if (out_file_path.has_parent_path()) {
      fs::create_directories(out_file_path.parent_path());
    }

    std::ofstream fout(out_file_path, std::ios::binary);
    if (!fout.is_open())
      throw std::runtime_error("Cannot create output file: " +
                               out_file_path.string());

    DecompressionContext *ctx = contexts[0];

    for (uint32_t c = c_start;
         (bytes_left > 0 || target_fe.file_size == 0) && c < footer.num_chunks;
         c++) {
      if (target_fe.file_size == 0)
        break;

      ChunkEntry ce = get_chunk_entry(ce_blob, c);
      if (ce.comp_size == 0)
        continue;

      fin.seekg(ce.payload_offset);
      fin.read((char *)ctx->host_in, ce.comp_size);

      ctx->comp_size = ce.comp_size;
      ctx->uncomp_size = ce.uncomp_size;
      ctx->primary_idx = ce.primary_idx;
      ctx->is_raw = ce.is_raw;
      ctx->gpu_hash = ce.gpu_hash;

      ctx->decompress_chunk(config.threads_decomp, config.mini_size);
      cudaStreamSynchronize(ctx->stream);

      if (ce.comp_size > 0 && *ctx->host_calc_hash != ce.gpu_hash) {
        corrupted_chunk_count++;
        if (corruption_cb) {
          int action = corruption_cb(rel_p, (c - c_start + 1), (c_start + 1),
                                     ctx->uncomp_size);
          if (action == 2) {
            fout.close();
            fs::remove(out_file_path);
            throw std::runtime_error(
                "Extraction aborted by user due to payload corruption in: " +
                rel_p);
          } else if (action == 1) {
            fout.close();
            fs::remove(out_file_path);
            break;
          }
        }
        std::memset(ctx->host_out, 0, ctx->uncomp_size);
      }

      float ms = 0.0f;
      cudaEventElapsedTime(&ms, ctx->e_start, ctx->e_end);
      gpu_ms += ms;

      bytes_compressed += ce.comp_size;
      uint64_t read_offset = (c == c_start) ? start_chunk_offset : 0;
      uint64_t available_bytes = (uint64_t)ctx->uncomp_size - read_offset;
      int write_amount = (int)std::min(bytes_left, available_bytes);

      if (write_amount > 0) {
        fout.write((char *)ctx->host_out + read_offset, write_amount);
      }

      bytes_processed += write_amount;
      bytes_left -= write_amount;

      if (progress_cb) {
        double elapsed =
            std::chrono::duration<double>(
                std::chrono::high_resolution_clock::now() - t_start)
                .count();
        progress_cb(bytes_processed, total_target_bytes, bytes_compressed,
                    elapsed);
      } else if (!quiet) {
        default_cli_progress(
            bytes_processed, total_target_bytes, bytes_compressed,
            std::chrono::duration<double>(
                std::chrono::high_resolution_clock::now() - t_start)
                .count());
      }
    }
    if (fout.is_open())
      fout.close();

  } else {
    std::unordered_set<std::string> seen_lower_paths;
    uint64_t bytes_processed = 0, bytes_compressed = 0;

    std::vector<bool> file_skipped_mask(matched_items.size(), false);
    for (size_t i = 0; i < matched_items.size(); i++) {
      const FileEntry &fe = matched_items[i].fe;
      std::string rel_p = ensure_utf8(string_table + fe.path_offset);
      std::string safe_rel = sanitize_relative_path(rel_p);
      fs::path check_p =
          get_safe_abs_path(utf8_to_path(output_dir) / utf8_to_path(safe_rel));
      if (collision_mode == "skip" && fs::exists(check_p)) {
        file_skipped_mask[i] = true;
      }
    }

    std::vector<bool> target_chunks_mask(footer.num_chunks, false);
    for (size_t i = 0; i < matched_items.size(); i++) {
      if (file_skipped_mask[i])
        continue;
      const auto &mitem = matched_items[i];
      const FileEntry &fe = mitem.fe;

      // FIXED: Use fe.chunk_start + fe.chunk_count directly from GTOC
      uint32_t c_end = fe.chunk_start + fe.chunk_count;

      for (uint32_t c = fe.chunk_start; c < c_end && c < footer.num_chunks;
           c++) {
        target_chunks_mask[c] = true;
      }
    }

    WorkQueue<DecompressionContext *> empty_q(contexts.size());
    WorkQueue<DecompressionContext *> compute_q(contexts.size());
    WorkQueue<DecompressionContext *> write_q(contexts.size());

    for (auto *ctx : contexts)
      empty_q.push(ctx);

    std::thread reader([&]() {
      try {
        std::ifstream f_in(input_path, std::ios::binary);
        for (uint32_t c = 0; c < footer.num_chunks; c++) {
          if (!target_chunks_mask[c])
            continue;
          ChunkEntry ce = get_chunk_entry(ce_blob, c);
          DecompressionContext *ctx = nullptr;
          if (!empty_q.pop(ctx))
            break;

          f_in.seekg(ce.payload_offset);
          f_in.read((char *)ctx->host_in, ce.comp_size);

          ctx->comp_size = ce.comp_size;
          ctx->uncomp_size = ce.uncomp_size;
          ctx->primary_idx = ce.primary_idx;
          ctx->is_raw = ce.is_raw;
          ctx->gpu_hash = ce.gpu_hash;

          if (!compute_q.push(ctx))
            break;
        }
        compute_q.push(nullptr);
      } catch (...) {
        empty_q.cancel();
        compute_q.cancel();
        write_q.cancel();
      }
    });

    std::thread compute([&]() {
      while (true) {
        DecompressionContext *ctx = nullptr;
        if (!compute_q.pop(ctx) || !ctx) {
          write_q.push(nullptr);
          break;
        }

        ctx->decompress_chunk(config.threads_decomp, config.mini_size);
        if (!write_q.push(ctx))
          break;
      }
    });

    size_t matched_idx = 0;
    uint64_t file_bytes_left = 0;
    std::ofstream current_fout;
    bool skip_current_file = false;
    std::string current_rel_p = "";
    uint32_t current_c_start = 0, current_c_count = 0;
    fs::path current_out_p;

    while (true) {
      DecompressionContext *ctx = nullptr;
      if (!write_q.pop(ctx) || !ctx)
        break;

      cudaStreamSynchronize(ctx->stream);

      if (ctx->comp_size > 0 && *ctx->host_calc_hash != ctx->gpu_hash) {
        corrupted_chunk_count++;
        std::memset(ctx->host_out, 0, ctx->uncomp_size);
      }

      float ms = 0.0f;
      cudaEventElapsedTime(&ms, ctx->e_start, ctx->e_end);
      gpu_ms += ms;

      bytes_compressed += ctx->comp_size;
      int chunk_bytes_left = ctx->uncomp_size;
      int chunk_read_offset = 0;
      while (chunk_bytes_left > 0) {
        if (file_bytes_left == 0) {
          if (current_fout.is_open())
            current_fout.close();
          if (matched_idx >= matched_items.size())
            break;

          const auto &mitem = matched_items[matched_idx];
          const FileEntry &fe = mitem.fe;
          current_rel_p = ensure_utf8(string_table + fe.path_offset);
          std::string safe_rel = sanitize_relative_path(current_rel_p);
          current_c_start = fe.chunk_start;
          current_c_count = fe.chunk_count;
          skip_current_file = false;

          if (chunk_read_offset < (int)mitem.start_chunk_offset &&
              fe.chunk_start == current_c_start) {
            int gap = (int)mitem.start_chunk_offset - chunk_read_offset;
            if (gap <= chunk_bytes_left) {
              chunk_read_offset += gap;
              chunk_bytes_left -= gap;
            }
          }

#ifdef _WIN32
          std::string lower_p = utf8_tolower(safe_rel);
          if (seen_lower_paths.count(lower_p)) {
            fs::path p_obj = utf8_to_path(safe_rel);
            std::string base = path_to_utf8(p_obj.stem());
            std::string ext = path_to_utf8(p_obj.extension());
            fs::path parent_p =
                p_obj.parent_path(); // PRESERVE PARENT DIRECTORY!

            int counter = 1;
            fs::path new_p =
                parent_p /
                utf8_to_path(base + " (" + std::to_string(counter) + ")" + ext);
            std::string new_rel = path_to_utf8(new_p);
            std::string new_lower = utf8_tolower(new_rel);

            while (seen_lower_paths.count(new_lower)) {
              counter++;
              new_p =
                  parent_p / utf8_to_path(base + " (" +
                                          std::to_string(counter) + ")" + ext);
              new_rel = path_to_utf8(new_p);
              new_lower = utf8_tolower(new_rel);
            }
            safe_rel = new_rel;
          }
          std::string final_lower = utf8_tolower(safe_rel);
          seen_lower_paths.insert(final_lower);
#endif

          current_out_p = get_safe_abs_path(utf8_to_path(output_dir) /
                                            utf8_to_path(safe_rel));

          if (fs::exists(current_out_p)) {
            if (collision_mode == "skip") {
              skip_current_file = true;
            } else if (collision_mode == "rename") {
              fs::path p_obj = utf8_to_path(current_out_p.string());
              std::string base = path_to_utf8(p_obj.stem());
              std::string ext = path_to_utf8(p_obj.extension());
              fs::path parent_p = current_out_p.parent_path();
              int counter = 1;
              fs::path new_p =
                  parent_p / utf8_to_path(base + " (" +
                                          std::to_string(counter) + ")" + ext);
              while (fs::exists(new_p)) {
                counter++;
                new_p = parent_p /
                        utf8_to_path(base + " (" + std::to_string(counter) +
                                     ")" + ext);
              }
              current_out_p = new_p;
            }
          }

          if (!skip_current_file) {
            if (current_out_p.has_parent_path()) {
              fs::create_directories(current_out_p.parent_path());
            }
            current_fout.open(current_out_p, std::ios::binary);
          }

          file_bytes_left = fe.file_size;
          matched_idx++;

          if (fe.file_size == 0) {
            if (current_fout.is_open())
              current_fout.close();
            continue;
          }
        }

        int write_amount =
            (int)std::min((uint64_t)chunk_bytes_left, file_bytes_left);

        if (!skip_current_file && current_fout.is_open()) {
          current_fout.write((char *)ctx->host_out + chunk_read_offset,
                             write_amount);
        }

        bytes_processed += write_amount;
        chunk_bytes_left -= write_amount;
        chunk_read_offset += write_amount;
        file_bytes_left -= write_amount;

        if (progress_cb) {
          double elapsed =
              std::chrono::duration<double>(
                  std::chrono::high_resolution_clock::now() - t_start)
                  .count();
          progress_cb(bytes_processed, total_target_bytes, bytes_compressed,
                      elapsed);
        } else if (!quiet) {
          default_cli_progress(
              bytes_processed, total_target_bytes, bytes_compressed,
              std::chrono::duration<double>(
                  std::chrono::high_resolution_clock::now() - t_start)
                  .count());
        }
      }

      empty_q.push(ctx);
    }

    if (current_fout.is_open())
      current_fout.close();

    while (matched_idx < matched_items.size()) {
      const auto &mitem = matched_items[matched_idx++];
      if (mitem.fe.file_size == 0) {
        std::string rel_p = ensure_utf8(string_table + mitem.fe.path_offset);
        std::string safe_rel = sanitize_relative_path(rel_p);
        fs::path out_p = get_safe_abs_path(utf8_to_path(output_dir) /
                                           utf8_to_path(safe_rel));
        if (out_p.has_parent_path()) {
          fs::create_directories(out_p.parent_path());
        }
        std::ofstream fout(out_p, std::ios::binary);
      }
    }

    empty_q.cancel();
    compute_q.cancel();
    write_q.cancel();

    if (reader.joinable())
      reader.join();
    if (compute.joinable())
      compute.join();
  }

  auto t_end = std::chrono::high_resolution_clock::now();
  float wall_ms =
      std::chrono::duration<float, std::milli>(t_end - t_start).count();

  if (corrupted_chunk_count > 0 && !quiet) {
    std::cout << "\n[NOTICE] Extraction completed with "
              << corrupted_chunk_count << " damaged block(s) padded."
              << std::endl;
  }

  if (!quiet)
    std::cout << "\n" << std::endl;

  return TimingStats{wall_ms, gpu_ms};
}