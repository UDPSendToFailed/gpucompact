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
#include "sha256.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on

#pragma comment(lib, "bcrypt.lib")

std::string compute_sha256_file(const std::string &filepath) {
  if (!fs::exists(filepath))
    return "";
  BCRYPT_ALG_HANDLE hAlg = NULL;
  BCRYPT_HASH_HANDLE hHash = NULL;
  DWORD cbData = 0, cbHash = 32, cbHashObject = 0;
  PBYTE pbHashObject = NULL;
  PBYTE pbHash = NULL;
  std::string result = "";

  if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0)
    return "";

  if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject,
                        sizeof(DWORD), &cbData, 0) != 0) {
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return "";
  }

  pbHashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHashObject);
  pbHash = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHash);

  if (BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0) ==
      0) {
    std::ifstream file(filepath, std::ios::binary);
    if (file.is_open()) {
      char buf[65536];
      while (file) {
        file.read(buf, sizeof(buf));
        std::streamsize r = file.gcount();
        if (r > 0) {
          BCryptHashData(hHash, (PBYTE)buf, (ULONG)r, 0);
        }
      }
      if (BCryptFinishHash(hHash, pbHash, cbHash, 0) == 0) {
        char hex[65];
        for (DWORD i = 0; i < cbHash; i++) {
          sprintf_s(hex + (i * 2), 3, "%02x", pbHash[i]);
        }
        result = std::string(hex);
      }
    }
    BCryptDestroyHash(hHash);
  }

  if (pbHashObject)
    HeapFree(GetProcessHeap(), 0, pbHashObject);
  if (pbHash)
    HeapFree(GetProcessHeap(), 0, pbHash);
  if (hAlg)
    BCryptCloseAlgorithmProvider(hAlg, 0);

  return result;
}
#else
std::string compute_sha256_file(const std::string &filepath) {
  return "unsupported";
}
#endif
