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
#include <cwctype>
#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

// Checks if a string is valid UTF-8
inline bool is_valid_utf8(const std::string &str) {
  const unsigned char *bytes = (const unsigned char *)str.data();
  size_t len = str.size();
  size_t i = 0;
  while (i < len) {
    if (bytes[i] <= 0x7F) {
      i++;
    } else if ((bytes[i] & 0xE0) == 0xC0) {
      if (i + 1 >= len || (bytes[i + 1] & 0xC0) != 0x80)
        return false;
      i += 2;
    } else if ((bytes[i] & 0xF0) == 0xE0) {
      if (i + 2 >= len || (bytes[i + 1] & 0xC0) != 0x80 ||
          (bytes[i + 2] & 0xC0) != 0x80)
        return false;
      i += 3;
    } else if ((bytes[i] & 0xF8) == 0xF0) {
      if (i + 3 >= len || (bytes[i + 1] & 0xC0) != 0x80 ||
          (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80)
        return false;
      i += 4;
    } else {
      return false;
    }
  }
  return true;
}

// Fallback decoder: ensures any string (including legacy ANSI/OEM CP1250/CP852)
// is valid UTF-8.
inline std::string ensure_utf8(const std::string &str) {
  if (is_valid_utf8(str))
    return str;
#ifdef _WIN32
  if (str.empty())
    return "";
  int wlen = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
  if (wlen <= 0)
    return str;
  std::wstring wstr(wlen, 0);
  MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstr[0], wlen);

  int ulen =
      WideCharToMultiByte(CP_UTF8, 0, &wstr[0], wlen, NULL, 0, NULL, NULL);
  if (ulen <= 0)
    return str;
  std::string utf8(ulen, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], wlen, &utf8[0], ulen, NULL, NULL);
  return utf8;
#else
  return str;
#endif
}

// Converts std::filesystem::path to a canonical UTF-8 std::string on all
// platforms.
inline std::string path_to_utf8(const fs::path &p) {
#ifdef _WIN32
  const std::wstring &wstr = p.wstring();
  if (wstr.empty())
    return "";
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                        NULL, 0, NULL, NULL);
  if (size_needed <= 0)
    return "";
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0],
                      size_needed, NULL, NULL);
  return strTo;
#else
  return p.string();
#endif
}

// Converts a UTF-8 std::string back to a native std::filesystem::path on all
// platforms.
inline fs::path utf8_to_path(const std::string &utf8_str) {
#ifdef _WIN32
  std::string clean_utf8 = ensure_utf8(utf8_str);
  if (clean_utf8.empty())
    return fs::path();
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, &clean_utf8[0],
                                        (int)clean_utf8.size(), NULL, 0);
  if (size_needed <= 0)
    return fs::path();
  std::wstring wstrTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &clean_utf8[0], (int)clean_utf8.size(),
                      &wstrTo[0], size_needed);
  return fs::path(wstrTo);
#else
  return fs::path(utf8_str);
#endif
}

// Standardized path normalizer (converts \ to /, removes leading/trailing
// slashes, ensures UTF-8).
inline std::string normalize_archive_path(std::string path_str) {
  path_str = ensure_utf8(path_str);
  for (char &c : path_str) {
    if (c == '\\')
      c = '/';
  }
  size_t start = path_str.find_first_not_of('/');
  if (start != std::string::npos) {
    path_str = path_str.substr(start);
  } else {
    path_str.clear();
  }
  while (!path_str.empty() && path_str.back() == '/') {
    path_str.pop_back();
  }
  return path_str;
}

// Hierarchical and exact path matcher for archive items.
inline bool path_matches_or_is_subpath(const std::string &item_path,
                                       const std::string &target_pattern) {
  std::string norm_item = normalize_archive_path(item_path);
  std::string norm_target = normalize_archive_path(target_pattern);

  if (norm_target.empty())
    return false;
  if (norm_item == norm_target)
    return true;

  std::string prefix = norm_target + "/";
  return (norm_item.rfind(prefix, 0) == 0);
}

// Unicode-aware UTF-8 lowercasing (Standard C++ CRT safe, zero extra lib
// dependencies)
inline std::string utf8_tolower(const std::string &utf8_str) {
#ifdef _WIN32
  fs::path p = utf8_to_path(utf8_str);
  std::wstring wstr = p.wstring();
  for (wchar_t &wc : wstr) {
    wc = (wchar_t)towlower(wc);
  }
  return path_to_utf8(fs::path(wstr));
#else
  std::string result = utf8_str;
  for (char &c : result) {
    c = (char)std::tolower((unsigned char)c);
  }
  return result;
#endif
}