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
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

// -------------------------------------------------------------------------
// PTX ASYNC COPY INTRINSICS (sm_80 / sm_89 Hardware DMA)
// -------------------------------------------------------------------------
#if defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 800
__device__ inline void cp_async_16(void *smem_ptr, const void *glob_ptr) {
  uint32_t smem_addr =
      static_cast<uint32_t>(__cvta_generic_to_shared(smem_ptr));
  asm volatile("cp.async.cg.shared.global [%0], [%1], 16;\n" ::"r"(smem_addr),
               "l"(glob_ptr));
}

__device__ inline void cp_async_4(void *smem_ptr, const void *glob_ptr) {
  uint32_t smem_addr =
      static_cast<uint32_t>(__cvta_generic_to_shared(smem_ptr));
  asm volatile("cp.async.ca.shared.global [%0], [%1], 4;\n" ::"r"(smem_addr),
               "l"(glob_ptr));
}

__device__ inline void cp_async_commit() {
  asm volatile("cp.async.commit_group;\n");
}

__device__ inline void cp_async_wait_all() {
  asm volatile("cp.async.wait_group 0;\n");
}
#endif

// -------------------------------------------------------------------------
// HELPER UTILITY KERNEL PROTOTYPES
// -------------------------------------------------------------------------
__global__ void bit_to_word_kernel(const unsigned int *__restrict__ bits,
                                   unsigned int *__restrict__ words, int n);

// -------------------------------------------------------------------------
// BWT & TANS KERNEL PROTOTYPES
// -------------------------------------------------------------------------
__global__ void sa_8byte_keys_kernel(const unsigned char *__restrict__ T,
                                     uint64_t *__restrict__ keys,
                                     int *__restrict__ sa, int n);

__global__ void sa_pass0_bucket_kernel(const uint64_t *__restrict__ sorted_keys,
                                       int *__restrict__ bucket_diff,
                                       uint8_t *__restrict__ tied_flags,
                                       int n);

__global__ void sa_set_rank_kernel(const int *__restrict__ bucket_starts,
                                   const int *__restrict__ sa_sorted,
                                   int *__restrict__ rank, int n);

__global__ void sa_active_key_kernel(const int *__restrict__ rank,
                                     const int *__restrict__ active_pos,
                                     const int *__restrict__ sa_final,
                                     uint64_t *__restrict__ keys,
                                     int *__restrict__ sa_out,
                                     int n, int m_active, int k_mod,
                                     int rank_shift);

__global__ void sa_active_bucket_kernel(const uint64_t *__restrict__ sorted_keys,
                                        const int *__restrict__ active_pos,
                                        const int *__restrict__ active_sa_sorted,
                                        int *__restrict__ sa_final,
                                        int *__restrict__ bucket_diff,
                                        uint8_t *__restrict__ tied_flags,
                                        int m_active);

__global__ void extract_bwt_kernel(const int *__restrict__ sa,
                                   const unsigned char *__restrict__ data,
                                   unsigned char *__restrict__ bwt,
                                   int *__restrict__ primary_idx, int n);

__global__ void
zrle_encode_kernel(const unsigned char *__restrict__ bwt,
                   unsigned short *__restrict__ out_symbols,
                   unsigned int *__restrict__ chunk_symbol_lengths,
                   uint32_t *__restrict__ hist,
                   int total_size, int chunk_size, int threads_comp);

__global__ void build_tans_all_kernel(
    const unsigned int *__restrict__ hist, int *__restrict__ p_out,
    int *__restrict__ prefix_p_out, int *__restrict__ max_x,
    int *__restrict__ symbol_spread, int *__restrict__ encoding_table,
    int *__restrict__ decoding_table, int *__restrict__ decoding_symbol,
    int *__restrict__ next_state, int L, int alphabet_size);

__global__ void tabled_encode_kernel(
    const unsigned short *__restrict__ symbols,
    const unsigned int *__restrict__ lengths, uint64_t *__restrict__ out_words,
    unsigned int *__restrict__ bit_lengths, const int *__restrict__ p,
    const int *__restrict__ prefix_p, const int *__restrict__ max_x,
    const int *__restrict__ enc_table, int num_chunks, int chunk_size,
    int max_words, int L, uint32_t *__restrict__ d_overflow_flag);

__global__ void tabled_decode_kernel(
    const uint64_t *__restrict__ in_words,
    const unsigned int *__restrict__ word_offsets,
    const unsigned int *__restrict__ bit_lengths,
    unsigned char *__restrict__ bwt, const int *__restrict__ dec_table,
    const int *__restrict__ dec_symbol, int total_size, int chunk_size, int L);

__global__ void dense_pack_kernel(const uint64_t *__restrict__ in,
                                  const unsigned int *__restrict__ offsets,
                                  const unsigned int *__restrict__ lens,
                                  uint64_t *__restrict__ out, int n, int max_w);

__global__ void bwt_block_hist_kernel(const unsigned char *__restrict__ bwt,
                                      uint32_t *__restrict__ block_hist,
                                      int n, int num_blocks);

__global__ void bwt_scatter_lf_kernel(const unsigned char *__restrict__ bwt,
                                      const uint32_t *__restrict__ block_offsets,
                                      int *__restrict__ LF,
                                      int n, int num_blocks);

__global__ void anchor_init_kernel(const int *__restrict__ LF,
                                   const int *__restrict__ primary_idx,
                                   int2 *__restrict__ JD_in,
                                   int n, int num_anchors);

__global__ void anchor_jump_step_kernel(const int2 *__restrict__ JD_in,
                                        int2 *__restrict__ JD_out,
                                        int total_anchors);

__global__ void anchor_scatter_kernel(const int2 *__restrict__ JD,
                                      const int *__restrict__ LF,
                                      const unsigned char *__restrict__ bwt,
                                      unsigned char *__restrict__ out,
                                      const int *__restrict__ primary_idx,
                                      int n, int num_anchors);

__global__ void gpu_hash_kernel(const unsigned char *__restrict__ data,
                                unsigned long long *__restrict__ d_hash,
                                int size);