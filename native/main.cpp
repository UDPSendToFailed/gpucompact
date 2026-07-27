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
#include "archive_writer.h"
#include "benchmark.h"
#include "context.cuh"
#include "path_utils.h"
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <windows.h>
#pragma comment(                                                               \
    lib, "shell32.lib") // Automatic MSVC linking for CommandLineToArgvW
#endif

namespace fs = std::filesystem;

static bool is_power_of_two(int n) { return n > 0 && (n & (n - 1)) == 0; }

void warmup_pipeline(const LaunchConfig &config) {
  std::cout << "GPUCompact 1.0.0 (CUDA)" << std::endl;
  std::cout << "Initializing GPU context..." << std::flush;

  int dummy_n = std::min(1024 * 1024, config.macro_mb * 1024 * 1024);
  CompressionContext comp_ctx(config.macro_mb * 1024 * 1024, config.mini_size,
                              config.L);
  DecompressionContext decomp_ctx(config.macro_mb * 1024 * 1024,
                                  config.mini_size, config.L);

  for (int i = 0; i < dummy_n; i++)
    comp_ctx.host_in[i] = (unsigned char)(i % 255);
  comp_ctx.bytes_read = dummy_n;
  comp_ctx.compress_chunk(config.threads_comp, config.mini_size);
  cudaDeviceSynchronize();

  decomp_ctx.uncomp_size = dummy_n;
  decomp_ctx.comp_size = comp_ctx.comp_size;
  decomp_ctx.is_raw = comp_ctx.is_raw;
  decomp_ctx.primary_idx = comp_ctx.primary_idx;
  decomp_ctx.num_chunks = comp_ctx.num_chunks;
  decomp_ctx.total_words = comp_ctx.total_words;
  decomp_ctx.gpu_hash = comp_ctx.gpu_hash;

  std::memcpy(decomp_ctx.host_in, comp_ctx.host_out, comp_ctx.comp_size);
  decomp_ctx.decompress_chunk(config.threads_decomp, config.mini_size);
  cudaDeviceSynchronize();

  std::cout << " Done.\n" << std::endl;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  // Bootstrapper: Parse native Windows UTF-16 command line into pristine UTF-8
  // argv
  int wargc = 0;
  LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
  std::vector<std::string> utf8_args;
  std::vector<char *> utf8_argv;
  if (wargv) {
    utf8_args.reserve(wargc);
    utf8_argv.reserve(wargc);
    for (int i = 0; i < wargc; i++) {
      utf8_args.push_back(path_to_utf8(fs::path(wargv[i])));
    }
    for (int i = 0; i < wargc; i++) {
      utf8_argv.push_back(&utf8_args[i][0]);
    }
    LocalFree(wargv);
    argc = wargc;
    argv = utf8_argv.data();
  }
#endif

  try {
    LaunchConfig config; // Defaults: macro=4MB, mini=1024B, L=2048,
                         // threads_comp=32, threads_decomp=128
    std::vector<std::string> input_paths;
    std::string output_path = "";
    std::string bench_path = "";
    std::string extract_file = "";
    std::string profile = "";
    std::string collision_mode = "replace";
    std::vector<std::string> add_paths;
    std::vector<std::string> remove_files;

    bool mode_compress = false;
    bool mode_decompress = false;
    bool mode_info = false;
    bool force_overwrite = false;

    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg == "-i" || arg == "--input") {
        if (i + 1 >= argc)
          throw std::runtime_error(
              "Flag '" + arg + "' requires one or more input path parameters.");
        while (i + 1 < argc && argv[i + 1][0] != '-')
          input_paths.push_back(argv[++i]);
      } else if (arg == "-o" || arg == "--output") {
        if (i + 1 >= argc)
          throw std::runtime_error("Flag '" + arg +
                                   "' requires an output path parameter.");
        output_path = argv[++i];
      } else if (arg == "-c" || arg == "--compress") {
        mode_compress = true;
      } else if (arg == "-d" || arg == "--decompress") {
        mode_decompress = true;
      } else if (arg == "-f" || arg == "--force") {
        force_overwrite = true;
      } else if (arg == "-l" || arg == "--info" || arg == "--list") {
        mode_info = true;
        if (i + 1 < argc && argv[i + 1][0] != '-') {
          input_paths.push_back(argv[++i]);
        }
      } else if (arg == "--extract") {
        if (i + 1 >= argc)
          throw std::runtime_error(
              "Flag '--extract' requires a target filename parameter.");
        extract_file = argv[++i];
        mode_decompress = true;
      } else if (arg == "--bench") {
        if (i + 1 >= argc)
          throw std::runtime_error(
              "Flag '--bench' requires a dataset folder parameter.");
        bench_path = argv[++i];
      } else if (arg == "--profile") {
        if (i + 1 >= argc)
          throw std::runtime_error(
              "Flag '--profile' requires a profile preset name.");
        profile = argv[++i];
      } else if (arg == "-a" || arg == "--add") {
        if (i + 1 >= argc)
          throw std::runtime_error(
              "Flag '" + arg + "' requires one or more input path parameters.");
        while (i + 1 < argc && argv[i + 1][0] != '-')
          add_paths.push_back(argv[++i]);
      } else if (arg == "-r" || arg == "--remove") {
        if (i + 1 >= argc)
          throw std::runtime_error("Flag '" + arg +
                                   "' requires one or more target filenames.");
        while (i + 1 < argc && argv[i + 1][0] != '-')
          remove_files.push_back(argv[++i]);
      } else if (arg == "--collision") {
        if (i + 1 >= argc)
          throw std::runtime_error("Flag '--collision' requires mode "
                                   "('replace', 'rename', 'skip').");
        collision_mode = argv[++i];
      } else if (arg == "--macro") {
        if (i + 1 >= argc)
          throw std::runtime_error(
              "Flag '--macro' requires a megabyte size value.");
        config.macro_mb = std::stoi(argv[++i]);
      } else if (arg == "--mini") {
        if (i + 1 >= argc)
          throw std::runtime_error(
              "Flag '--mini' requires a block byte size value.");
        config.mini_size = std::stoi(argv[++i]);
      } else if (arg == "--L") {
        if (i + 1 >= argc)
          throw std::runtime_error(
              "Flag '--L' requires a tANS state count value.");
        config.L = std::stoi(argv[++i]);
      }
    }

    if (profile == "best_speed") {
      config.macro_mb = 4;
      config.mini_size = 256;
      config.L = 512;
      config.threads_comp = 32;
      config.threads_decomp = 32;
    } else if (profile == "speed") {
      config.macro_mb = 4;
      config.mini_size = 512;
      config.L = 512;
      config.threads_comp = 32;
      config.threads_decomp = 32;
    } else if (profile == "balanced") {
      config.macro_mb = 4;
      config.mini_size = 512;
      config.L = 1024;
      config.threads_comp = 32;
      config.threads_decomp = 64;
    } else if (profile == "ratio") {
      config.macro_mb = 4;
      config.mini_size = 2048;
      config.L = 2048;
      config.threads_comp = 32;
      config.threads_decomp = 64;
    } else if (profile == "best_ratio") {
      config.macro_mb = 8;
      config.mini_size = 8192;
      config.L = 2048;
      config.threads_comp = 32;
      config.threads_decomp = 64;
    } else if (!profile.empty()) {
      throw std::runtime_error("Unknown profile '" + profile +
                               "'. Valid presets: 'balanced', 'speed', "
                               "'ratio', 'best_speed', 'best_ratio'.");
    }

    if (config.macro_mb < 1 || config.macro_mb > 64) {
      throw std::runtime_error("Invalid --macro size (" +
                               std::to_string(config.macro_mb) +
                               " MB). Must be between 1 and 64 MB.");
    }
    if (config.mini_size < 128 || config.mini_size > 16384 ||
        !is_power_of_two(config.mini_size)) {
      throw std::runtime_error(
          "Invalid --mini size (" + std::to_string(config.mini_size) +
          " B). Must be a power of 2 between 128 and 16384.");
    }
    if (config.L < 256 || config.L > 8192 || !is_power_of_two(config.L)) {
      throw std::runtime_error("Invalid --L state window (" +
                               std::to_string(config.L) +
                               "). Must be a power of 2 between 256 and 8192.");
    }
    if (collision_mode != "replace" && collision_mode != "rename" &&
        collision_mode != "skip") {
      throw std::runtime_error("Invalid --collision mode '" + collision_mode +
                               "'. Must be 'replace', 'rename', or 'skip'.");
    }

    std::string target_archive = !input_paths.empty() ? input_paths[0] : "";

    if (mode_info) {
      if (target_archive.empty()) {
        std::cerr
            << "error: Target archive required: gpucompact -l <archive.gcmp>"
            << std::endl;
        return 1;
      }
      inspect_archive_cli(target_archive);
      return 0;
    }

    if (!remove_files.empty()) {
      if (target_archive.empty()) {
        std::cerr << "error: Target archive required: gpucompact -i "
                     "<archive.gcmp> -r <file...>"
                  << std::endl;
        return 1;
      }
      remove_from_archive(target_archive, remove_files);
      std::cout << "Successfully removed target files." << std::endl;
      return 0;
    }

    if (!add_paths.empty()) {
      if (target_archive.empty()) {
        std::cerr << "error: Target archive required: gpucompact -i "
                     "<archive.gcmp> -a <file...>"
                  << std::endl;
        return 1;
      }
      warmup_pipeline(config);
      append_to_archive(target_archive, add_paths, config, collision_mode);
      std::cout << "Successfully appended target items." << std::endl;
      return 0;
    }

    if (!mode_compress && !mode_decompress && !input_paths.empty()) {
      if (input_paths.size() == 1 &&
          input_paths[0].find(".gcmp") != std::string::npos) {
        mode_decompress = true;
      } else {
        mode_compress = true;
      }
    }

    if (mode_compress && output_path.empty()) {
      if (input_paths.size() == 1) {
        output_path = fs::path(input_paths[0]).filename().string() + ".gcmp";
      } else {
        output_path = "archive.gcmp";
      }
    }

    if (mode_decompress && output_path.empty()) {
      if (!target_archive.empty() &&
          target_archive.find(".gcmp") != std::string::npos) {
        output_path = target_archive.substr(0, target_archive.length() - 5) +
                      "_extracted";
      } else {
        output_path = "extracted";
      }
    }

    if (input_paths.empty() && bench_path.empty()) {
      std::cout
          << "GPUCompact 1.0.0 (CUDA)\n\n"
          << "Usage:\n"
          << "  gpucompact -c -i <input...> [-o archive.gcmp] [--profile "
             "<preset>]\n"
          << "  gpucompact -d -i <archive.gcmp> [-o dest_dir] [--extract "
             "<file>]\n"
          << "  gpucompact -l <archive.gcmp>\n"
          << "  gpucompact -i <archive.gcmp> -a <add_input...> [--collision "
             "<mode>]\n"
          << "  gpucompact -i <archive.gcmp> -r <remove_file...>\n"
          << "  gpucompact --bench <dir> [--profile <preset>]\n\n"
          << "Options:\n"
          << "  -c, --compress         Compress input files or directories "
             "into a .gcmp archive\n"
          << "  -d, --decompress       Decompress a .gcmp archive to target "
             "directory\n"
          << "  -i, --input <path...>  Input path(s) or target archive\n"
          << "  -o, --output <path>    Output archive or extraction directory\n"
          << "  -l, --list, --info     Display archive table of contents and "
             "metadata\n"
          << "  -a, --add <path...>    Append files or directories to an "
             "existing archive\n"
          << "  -r, --remove <file...> Remove file(s) from an existing "
             "archive\n"
          << "  -f, --force            Overwrite output file if it already "
             "exists\n"
          << "  --extract <file>       Extract a specific single file from "
             "archive\n"
          << "  --collision <mode>     Collision policy: replace, rename, skip "
             "(default: replace)\n"
          << "  --profile <preset>     Compression profile: best_speed, speed, "
             "balanced, ratio, best_ratio\n"
          << "  --macro <MB>           Macroblock size in MB (1..64, default: "
             "4)\n"
          << "  --mini <bytes>         Miniblock size in bytes (128..16384, "
             "default: 1024)\n"
          << "  --L <states>           tANS state table size (256..8192, "
             "default: 2048)\n"
          << "  --bench <path>         Run benchmark suite over a file or "
             "directory\n"
          << std::endl;
      return 0;
    }

    if (mode_compress && fs::exists(output_path) && !force_overwrite) {
      throw std::runtime_error(
          "Target archive '" + output_path +
          "' already exists. Use -f or --force to overwrite, or -a to append.");
    }

    warmup_pipeline(config);

    if (mode_compress) {
      build_archive(input_paths, output_path, config);
    } else if (mode_decompress) {
      extract_archive(target_archive, output_path, config, nullptr,
                      extract_file, false, nullptr, nullptr, collision_mode);
    }

    if (!bench_path.empty()) {
      run_benchmark(bench_path, config);
    }

  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}