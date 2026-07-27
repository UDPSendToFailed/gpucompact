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
#include "c_api.h"
#include "archive_reader.h"
#include "archive_writer.h"
#include "benchmark.h"
#include "path_utils.h"
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static LaunchConfig to_launch_config(CLaunchConfig c) {
  LaunchConfig config;
  config.macro_mb = c.macro_mb > 0 ? c.macro_mb : 4;
  config.mini_size = c.mini_size > 0 ? c.mini_size : 1024;
  config.L = c.L > 0 ? c.L : 2048;
  config.threads_comp = c.threads_comp > 0 ? c.threads_comp : 6;
  config.threads_decomp = c.threads_decomp > 0 ? c.threads_decomp : 56;
  return config;
}

extern "C" {

DllExport int gpucompact_build_archive(const char **input_paths, int num_inputs,
                                       const char *output_path,
                                       CLaunchConfig config,
                                       CProgressCallback cb, void *user_data) {
  try {
    std::vector<std::string> inputs;
    for (int i = 0; i < num_inputs; i++)
      inputs.push_back(input_paths[i]);

    ProgressCallback p_cb = nullptr;
    if (cb) {
      p_cb = [cb, user_data](uint64_t processed, uint64_t total,
                             uint64_t compressed, double elapsed_sec) {
        cb(processed, total, compressed, elapsed_sec, user_data);
      };
    }

    build_archive(inputs, output_path, to_launch_config(config), nullptr, true,
                  p_cb);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "[DLL ERROR] build_archive: " << e.what() << std::endl;
    return -1;
  }
}

DllExport int
gpucompact_extract_archive(const char *input_path, const char *output_dir,
                           CLaunchConfig config, const char *extract_file,
                           const char *collision_mode, CProgressCallback cb,
                           void *user_data, CCorruptionCallback corruption_cb) {
  try {
    ProgressCallback p_cb = nullptr;
    if (cb) {
      p_cb = [cb, user_data](uint64_t processed, uint64_t total,
                             uint64_t compressed, double elapsed_sec) {
        cb(processed, total, compressed, elapsed_sec, user_data);
      };
    }

    CorruptionCallback c_cb = nullptr;
    if (corruption_cb) {
      c_cb = [corruption_cb,
              user_data](const std::string &file_path, uint32_t chunk_idx,
                         uint32_t total_chunks, uint64_t uncomp_bytes) -> int {
        return corruption_cb(file_path.c_str(), chunk_idx, total_chunks,
                             uncomp_bytes, user_data);
      };
    }

    std::string target_file = extract_file ? extract_file : "";
    std::string cmode = collision_mode ? collision_mode : "replace";
    extract_archive(input_path, output_dir, to_launch_config(config), nullptr,
                    target_file, true, p_cb, c_cb, cmode);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "[DLL ERROR] extract_archive: " << e.what() << std::endl;
    return -1;
  }
}

DllExport char *gpucompact_check_extract_collisions(const char *archive_path,
                                                    const char *output_dir,
                                                    const char *extract_file) {
  try {
    std::ifstream fin(archive_path, std::ios::binary);
    if (!fin.is_open())
      return nullptr;

    char magic[4];
    fin.read(magic, 4);
    if (std::string(magic, 4) != "GCMP")
      return nullptr;

    fin.seekg(-((int)sizeof(GTOCFooter)), std::ios::end);
    GTOCFooter footer;
    fin.read((char *)&footer, sizeof(GTOCFooter));
    if (std::string(footer.magic, 4) != "GTOC")
      return nullptr;

    fin.seekg(footer.gtoc_offset);
    uint64_t gtoc_size = (uint64_t)fs::file_size(archive_path) -
                         sizeof(GTOCFooter) - footer.gtoc_offset;
    std::vector<char> gtoc_blob(gtoc_size);
    fin.read(gtoc_blob.data(), gtoc_size);

    uint64_t fe_size = footer.num_files * sizeof(FileEntry);
    uint64_t ce_size = footer.num_chunks * sizeof(ChunkEntry);
    uint64_t str_size = gtoc_size - fe_size - ce_size;

    const char *string_table = gtoc_blob.data();
    const char *fe_blob = gtoc_blob.data() + str_size;

    std::vector<std::string> target_patterns;
    if (extract_file && strlen(extract_file) > 0) {
      std::stringstream ss(extract_file);
      std::string token;
      while (std::getline(ss, token, '|')) {
        if (!token.empty())
          target_patterns.push_back(normalize_archive_path(token));
      }
    }

    std::vector<std::string> collisions;
    fs::path out_dir_p = utf8_to_path(output_dir ? output_dir : "");

    for (uint32_t i = 0; i < footer.num_files; i++) {
      FileEntry fe;
      std::memcpy(&fe, fe_blob + i * sizeof(FileEntry), sizeof(FileEntry));
      std::string rel_p = ensure_utf8(string_table + fe.path_offset);
      std::string norm_p = normalize_archive_path(rel_p);

      if (!target_patterns.empty()) {
        bool matched = false;
        for (const auto &pat : target_patterns) {
          if (norm_p == pat || norm_p.rfind(pat + "/", 0) == 0) {
            matched = true;
            break;
          }
        }
        if (!matched)
          continue;
      }

      fs::path check_p;
      if (target_patterns.size() == 1 && !target_patterns[0].empty()) {
        check_p = out_dir_p / utf8_to_path(norm_p).filename();
      } else {
        check_p = out_dir_p / utf8_to_path(norm_p);
      }

      if (fs::exists(check_p)) {
        collisions.push_back(norm_p);
      }
    }

    std::stringstream json;
    json << "{\n";
    json << "  \"has_collisions\": " << (collisions.empty() ? "false" : "true")
         << ",\n";
    json << "  \"colliding_files\": [\n";
    for (size_t i = 0; i < collisions.size(); i++) {
      json << "    \"" << collisions[i] << "\"";
      if (i + 1 < collisions.size())
        json << ",";
      json << "\n";
    }
    json << "  ]\n";
    json << "}\n";

    std::string res = json.str();
    char *out_str = (char *)malloc(res.size() + 1);
    std::strcpy(out_str, res.c_str());
    return out_str;
  } catch (...) {
    return nullptr;
  }
}

DllExport char *gpucompact_inspect_archive_json(const char *archive_path) {
  try {
    std::ifstream fin(archive_path, std::ios::binary);
    if (!fin.is_open())
      return nullptr;

    char magic[4];
    fin.read(magic, 4);
    if (std::string(magic, 4) != "GCMP")
      return nullptr;

    fin.seekg(-((int)sizeof(GTOCFooter)), std::ios::end);
    GTOCFooter footer;
    fin.read((char *)&footer, sizeof(GTOCFooter));
    if (std::string(footer.magic, 4) != "GTOC")
      return nullptr;

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
    const FileEntry *file_entries =
        (const FileEntry *)(gtoc_blob.data() + str_size);
    const ChunkEntry *chunk_entries =
        (const ChunkEntry *)(gtoc_blob.data() + str_size + fe_size);

    uint64_t total_uncomp = 0, total_comp = 0;
    for (uint32_t i = 0; i < footer.num_files; i++)
      total_uncomp += file_entries[i].file_size;
    for (uint32_t i = 0; i < footer.num_chunks; i++)
      total_comp += chunk_entries[i].comp_size;

    double overall_ratio =
        (total_comp > 0) ? ((double)total_uncomp / total_comp) : 1.0;

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"metadata\": {\n";
    ss << "    \"archive_name\": \""
       << path_to_utf8(fs::path(archive_path).filename()) << "\",\n";
    ss << "    \"archive_size\": " << total_file_size << ",\n";
    ss << "    \"macro_mb\": " << footer.macro_mb << ",\n";
    ss << "    \"mini_size\": " << footer.mini_size << ",\n";
    ss << "    \"L_state\": " << footer.L_state << ",\n";
    ss << "    \"num_files\": " << footer.num_files << ",\n";
    ss << "    \"num_chunks\": " << footer.num_chunks << ",\n";
    ss << "    \"total_uncompressed_bytes\": " << total_uncomp << ",\n";
    ss << "    \"total_compressed_bytes\": " << total_comp << ",\n";
    ss << "    \"overall_ratio\": " << overall_ratio << "\n";
    ss << "  },\n";
    ss << "  \"files\": [\n";

    for (uint32_t i = 0; i < footer.num_files; i++) {
      const FileEntry &fe = file_entries[i];
      std::string rel_p = ensure_utf8(string_table + fe.path_offset);

      uint64_t f_uncomp = fe.file_size;
      double f_comp_weighted = 0.0;
      uint64_t bytes_remaining = f_uncomp;

      for (uint32_t c = fe.chunk_start;
           c < fe.chunk_start + fe.chunk_count && c < footer.num_chunks; c++) {
        const ChunkEntry &ce = chunk_entries[c];
        if (ce.uncomp_size == 0)
          continue;

        uint64_t bytes_in_chunk =
            std::min(bytes_remaining, (uint64_t)ce.uncomp_size);
        double weight = (double)bytes_in_chunk / ce.uncomp_size;
        f_comp_weighted += ce.comp_size * weight;
        bytes_remaining -= bytes_in_chunk;
      }
      uint64_t f_comp = (uint64_t)f_comp_weighted;
      double ratio = (f_comp > 0) ? ((double)f_uncomp / f_comp) : 1.0;

      std::string clean_path;
      for (unsigned char ch : rel_p) {
        if (ch == '\\')
          clean_path += "\\\\";
        else if (ch == '"')
          clean_path += "\\\"";
        else if (ch == '\n')
          clean_path += "\\n";
        else if (ch == '\r')
          clean_path += "\\r";
        else if (ch == '\t')
          clean_path += "\\t";
        else if (ch == '\b')
          clean_path += "\\b";
        else if (ch == '\f')
          clean_path += "\\f";
        else if (ch < 0x20) {
          char hex[7];
          std::snprintf(hex, sizeof(hex), "\\u%04x", ch);
          clean_path += hex;
        } else {
          clean_path += ch;
        }
      }

      ss << "    {\n";
      ss << "      \"path\": \"" << clean_path << "\",\n";
      ss << "      \"uncompressed_size\": " << f_uncomp << ",\n";
      ss << "      \"compressed_size\": " << f_comp << ",\n";
      ss << "      \"ratio\": " << ratio << ",\n";
      ss << "      \"chunk_start\": " << fe.chunk_start << ",\n";
      ss << "      \"chunk_count\": " << fe.chunk_count << "\n";
      ss << "    }" << (i + 1 < footer.num_files ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";

    std::string json_str = ss.str();
    char *result = (char *)std::malloc(json_str.length() + 1);
    std::memcpy(result, json_str.c_str(), json_str.length() + 1);
    return result;
  } catch (...) {
    return nullptr;
  }
}

DllExport void gpucompact_free_string(char *str) {
  if (str)
    std::free(str);
}

DllExport int gpucompact_append_to_archive(const char *archive_path,
                                           const char **input_paths,
                                           int num_inputs, CLaunchConfig config,
                                           const char *collision_mode,
                                           CProgressCallback cb,
                                           void *user_data) {
  try {
    std::vector<std::string> inputs;
    for (int i = 0; i < num_inputs; i++)
      inputs.push_back(input_paths[i]);

    ProgressCallback p_cb = nullptr;
    if (cb) {
      p_cb = [cb, user_data](uint64_t processed, uint64_t total,
                             uint64_t compressed, double elapsed_sec) {
        cb(processed, total, compressed, elapsed_sec, user_data);
      };
    }

    std::string mode = collision_mode ? collision_mode : "replace";
    append_to_archive(archive_path, inputs, to_launch_config(config), mode,
                      p_cb);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "[DLL ERROR] append_to_archive: " << e.what() << std::endl;
    return -1;
  }
}

DllExport int gpucompact_remove_from_archive(const char *archive_path,
                                             const char **files_to_remove,
                                             int num_files,
                                             CProgressCallback cb,
                                             void *user_data) {
  try {
    std::vector<std::string> files;
    for (int i = 0; i < num_files; i++)
      files.push_back(files_to_remove[i]);

    ProgressCallback p_cb = nullptr;
    if (cb) {
      p_cb = [cb, user_data](uint64_t processed, uint64_t total,
                             uint64_t compressed, double elapsed_sec) {
        cb(processed, total, compressed, elapsed_sec, user_data);
      };
    }

    remove_from_archive(archive_path, files, p_cb);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "[DLL ERROR] remove_from_archive: " << e.what() << std::endl;
    return -1;
  }
}

DllExport int gpucompact_run_benchmark_stream(const char **input_paths,
                                              int num_inputs,
                                              CLaunchConfig config,
                                              CBenchmarkCallback cb,
                                              void *user_data) {
  try {
    std::vector<std::string> inputs;
    for (int i = 0; i < num_inputs; i++)
      inputs.push_back(input_paths[i]);

    BenchmarkFileCallback b_cb = nullptr;
    if (cb) {
      b_cb = [cb, user_data](const BenchmarkFileResult &res) {
        CBenchmarkFileResult c_res;
        std::memset(&c_res, 0, sizeof(CBenchmarkFileResult));
        std::memcpy(c_res.filename, res.filename, sizeof(c_res.filename));
        c_res.orig_bytes = res.orig_bytes;
        c_res.comp_bytes = res.comp_bytes;
        c_res.ratio = res.ratio;
        c_res.c_wall_mbs = res.c_wall_mbs;
        c_res.c_gpu_mbs = res.c_gpu_mbs;
        c_res.d_wall_mbs = res.d_wall_mbs;
        c_res.d_gpu_mbs = res.d_gpu_mbs;
        c_res.sha_passed = res.sha_passed ? 1 : 0;
        std::memcpy(c_res.orig_sha, res.orig_sha, sizeof(c_res.orig_sha));
        std::memcpy(c_res.dec_sha, res.dec_sha, sizeof(c_res.dec_sha));

        cb(&c_res, user_data);
      };
    }

    run_benchmark_stream(inputs, to_launch_config(config), b_cb);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "[DLL ERROR] run_benchmark_stream: " << e.what() << std::endl;
    return -1;
  }
}
}