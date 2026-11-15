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
#include "kernels.cuh"

__global__ void bit_to_word_kernel(const unsigned int *__restrict__ bits,
                                   unsigned int *__restrict__ words, int n) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < n)
    words[idx] = (bits[idx] + 63) / 64;
}

__global__ void sa_8byte_keys_kernel(const unsigned char *__restrict__ T,
                                     uint64_t *__restrict__ keys,
                                     int *__restrict__ sa, int n) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n)
    return;
  uint64_t key = 0;
  if (idx + 7 < n) {
    key = ((uint64_t)T[idx] << 56) | ((uint64_t)T[idx + 1] << 48) |
          ((uint64_t)T[idx + 2] << 40) | ((uint64_t)T[idx + 3] << 32) |
          ((uint64_t)T[idx + 4] << 24) | ((uint64_t)T[idx + 5] << 16) |
          ((uint64_t)T[idx + 6] << 8) | ((uint64_t)T[idx + 7]);
  } else {
    for (int k = 0; k < 8; k++) {
      int j = idx + k;
      while (j >= n)
        j -= n;
      key = (key << 8) | (uint64_t)T[j];
    }
  }
  keys[idx] = key;
  sa[idx] = idx;
}

__global__ void sa_pass0_bucket_kernel(const uint64_t *__restrict__ sorted_keys,
                                       int *__restrict__ bucket_diff,
                                       uint8_t *__restrict__ tied_flags,
                                       int n) {
  int pos = blockIdx.x * blockDim.x + threadIdx.x;
  if (pos >= n)
    return;
  uint64_t cur = sorted_keys[pos];
  bool eq_prev = (pos > 0) && (cur == sorted_keys[pos - 1]);
  bool eq_next = (pos + 1 < n) && (cur == sorted_keys[pos + 1]);
  bucket_diff[pos] = eq_prev ? 0 : pos;
  tied_flags[pos] = (eq_prev || eq_next) ? 1 : 0;
}

__global__ void sa_set_rank_kernel(const int *__restrict__ bucket_starts,
                                   const int *__restrict__ sa_sorted,
                                   int *__restrict__ rank, int n) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n)
    return;
  rank[sa_sorted[idx]] = bucket_starts[idx];
}

__global__ void sa_active_key_kernel(const int *__restrict__ rank,
                                     const int *__restrict__ active_pos,
                                     const int *__restrict__ sa_final,
                                     uint64_t *__restrict__ keys,
                                     int *__restrict__ sa_out,
                                     int n, int m_active, int k_mod,
                                     int rank_shift) {
  int m = blockIdx.x * blockDim.x + threadIdx.x;
  if (m >= m_active)
    return;
  int s = sa_final[active_pos[m]];
  int j = s + k_mod;
  if (j >= n)
    j -= n;
  uint64_t r1 = (uint32_t)rank[s];
  uint64_t r2 = (uint32_t)rank[j];
  keys[m] = (r1 << rank_shift) | r2;
  sa_out[m] = s;
}

__global__ void sa_active_bucket_kernel(const uint64_t *__restrict__ sorted_keys,
                                        const int *__restrict__ active_pos,
                                        const int *__restrict__ active_sa_sorted,
                                        int *__restrict__ sa_final,
                                        int *__restrict__ bucket_diff,
                                        uint8_t *__restrict__ tied_flags,
                                        int m_active) {
  int m = blockIdx.x * blockDim.x + threadIdx.x;
  if (m >= m_active)
    return;
  int pos = active_pos[m];
  sa_final[pos] = active_sa_sorted[m];
  uint64_t cur = sorted_keys[m];
  bool eq_prev = (m > 0) && (cur == sorted_keys[m - 1]);
  bool eq_next = (m + 1 < m_active) && (cur == sorted_keys[m + 1]);
  bucket_diff[m] = eq_prev ? 0 : pos;
  tied_flags[m] = (eq_prev || eq_next) ? 1 : 0;
}

__global__ void extract_bwt_kernel(const int *__restrict__ sa,
                                   const unsigned char *__restrict__ data,
                                   unsigned char *__restrict__ bwt,
                                   int *__restrict__ primary_idx, int n) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= n)
    return;
  int s = sa[idx];
  if (s == 0)
    *primary_idx = idx;
  bwt[idx] = data[(s == 0) ? n - 1 : s - 1];
}

__global__ void
zrle_encode_kernel(const unsigned char *__restrict__ bwt,
                   unsigned short *__restrict__ out_symbols,
                   unsigned int *__restrict__ chunk_symbol_lengths,
                   uint32_t *__restrict__ hist,
                   int total_size, int chunk_size, int threads_comp) {
  __shared__ uint32_t s_block_hist[257];
  for (int i = threadIdx.x; i < 257; i += blockDim.x) {
    s_block_hist[i] = 0;
  }
  __syncthreads();

  int chunk_id = blockIdx.x * blockDim.x + threadIdx.x;
  int start = chunk_id * chunk_size;
  int end = start + chunk_size;
  if (end > total_size)
    end = total_size;

  if (start < total_size) {
    extern __shared__ unsigned char s_alphabet[];
    unsigned char *alphabet = &s_alphabet[threadIdx.x * 260];
    for (int i = 0; i < 256; i++) {
      alphabet[i] = (unsigned char)i;
    }

    int zero_run = 0, sym_offset = 0, out_base = chunk_id * chunk_size;
    uint32_t hist0 = 0, hist1 = 0;
    for (int i = start; i < end; i++) {
      unsigned char c = bwt[i], idx = 0;
      for (int j = 0; j < 256; j++) {
        if (alphabet[j] == c) {
          idx = (unsigned char)j;
          break;
        }
      }

      if (idx == 0) {
        zero_run++;
      } else {
        if (zero_run > 0) {
          int temp = zero_run;
          while (temp > 0) {
            if (temp % 2 == 1) {
              out_symbols[out_base + sym_offset++] = 0;
              hist0++;
              temp = (temp - 1) / 2;
            } else {
              out_symbols[out_base + sym_offset++] = 1;
              hist1++;
              temp = (temp - 2) / 2;
            }
          }
          zero_run = 0;
        }
        out_symbols[out_base + sym_offset++] = idx + 1;
        atomicAdd(&s_block_hist[idx + 1], 1);
      }

      // Move-To-Front (MTF) update
      if (idx == 1) {
        alphabet[1] = alphabet[0];
        alphabet[0] = c;
      } else if (idx > 1) {
        for (int j = idx; j > 0; j--)
          alphabet[j] = alphabet[j - 1];
        alphabet[0] = c;
      }
    }
    if (zero_run > 0) {
      int temp = zero_run;
      while (temp > 0) {
        if (temp % 2 == 1) {
          out_symbols[out_base + sym_offset++] = 0;
          hist0++;
          temp = (temp - 1) / 2;
        } else {
          out_symbols[out_base + sym_offset++] = 1;
          hist1++;
          temp = (temp - 2) / 2;
        }
      }
    }
    if (hist0 > 0)
      atomicAdd(&s_block_hist[0], hist0);
    if (hist1 > 0)
      atomicAdd(&s_block_hist[1], hist1);
    chunk_symbol_lengths[chunk_id] = sym_offset;
  }

  __syncthreads();
  for (int i = threadIdx.x; i < 257; i += blockDim.x) {
    if (s_block_hist[i] > 0) {
      atomicAdd(&hist[i], s_block_hist[i]);
    }
  }
}

__global__ void build_tans_all_kernel(
    const unsigned int *__restrict__ hist, int *__restrict__ p_out,
    int *__restrict__ prefix_p_out, int *__restrict__ max_x,
    int *__restrict__ symbol_spread, int *__restrict__ encoding_table,
    int *__restrict__ decoding_table, int *__restrict__ decoding_symbol,
    int *__restrict__ next_state, int L, int alphabet_size) {
  int tid = threadIdx.x;
  if (tid >= alphabet_size)
    return;

  __shared__ unsigned int s_hist[257];
  __shared__ int s_p[257];
  s_hist[tid] = hist[tid];
  s_p[tid] = (hist[tid] > 0) ? 1 : 0;
  __syncthreads();

  int p_sum = 0;
  double weight_sum = 0.0;
  for (int i = 0; i < alphabet_size; i++) {
    p_sum += s_p[i];
    weight_sum += (double)s_hist[i];
  }
  __syncthreads();
  int remaining = L - p_sum;
  if (remaining > 0) {
    double weight = (double)s_hist[tid];
    int add = (weight_sum > 0.0) ? (int)((remaining * weight) / weight_sum)
                                 : (tid == 0 ? remaining : 0);
    s_p[tid] += add;
  }
  __syncthreads();

  p_sum = 0;
  double max_weight = -1.0;
  int max_idx = 0;
  for (int i = 0; i < alphabet_size; i++) {
    p_sum += s_p[i];
    if ((double)s_hist[i] > max_weight) {
      max_weight = (double)s_hist[i];
      max_idx = i;
    }
  }
  __syncthreads();
  if (L - p_sum > 0 && tid == max_idx && weight_sum > 0.0)
    s_p[tid] += (L - p_sum);
  __syncthreads();

  p_out[tid] = s_p[tid];
  max_x[tid] = 2 * s_p[tid] - 1;
  int prefix = 0;
  for (int i = 0; i < tid; i++)
    prefix += s_p[i];
  prefix_p_out[tid] = prefix;

  int step = (L / 2) + 3;
  for (int k = 0; k < s_p[tid]; k++) {
    int spread_idx = ((prefix + k) * step) % L;
    if (spread_idx >= 0 && spread_idx < L)
      symbol_spread[spread_idx] = tid;
  }
  __syncthreads();

  if (tid == 0) {
    for (int i = 0; i < alphabet_size; i++)
      next_state[i] = s_p[i];
    for (int x = L; x < 2 * L; x++) {
      int s = symbol_spread[x - L];
      int dec_idx = x - L;
      if (dec_idx >= 0 && dec_idx < L) {
        decoding_symbol[dec_idx] = s;
        int state = next_state[s];
        decoding_table[dec_idx] = state;
        int enc_idx = prefix_p_out[s] + state - s_p[s];
        if (enc_idx >= 0 && enc_idx < L)
          encoding_table[enc_idx] = x;
        next_state[s] = state + 1;
      }
    }
  }
}

__global__ void tabled_encode_kernel(
    const unsigned short *__restrict__ symbols,
    const unsigned int *__restrict__ lengths, uint64_t *__restrict__ out_words,
    unsigned int *__restrict__ bit_lengths, const int *__restrict__ p,
    const int *__restrict__ prefix_p, const int *__restrict__ max_x,
    const int *__restrict__ enc_table, int num_chunks, int chunk_size,
    int max_words, int L, uint32_t *__restrict__ d_overflow_flag) {
  int chunk_id = blockIdx.x * blockDim.x + threadIdx.x;

  extern __shared__ unsigned char s_mem_th[];
  int *s_enc = (int *)s_mem_th;
  int *s_p = (int *)(s_mem_th + L * sizeof(int));
  int *s_prefix_p = (int *)(s_mem_th + (L + 257) * sizeof(int));
  int *s_max_x = (int *)(s_mem_th + (L + 514) * sizeof(int));

  // Coalesced, branch-free shared memory pre-staging
  for (int i = threadIdx.x; i < L; i += blockDim.x)
    s_enc[i] = enc_table[i];
  for (int i = threadIdx.x; i < 257; i += blockDim.x) {
    s_p[i] = p[i];
    s_prefix_p[i] = prefix_p[i];
    s_max_x[i] = max_x[i];
  }
  __syncthreads();

  if (chunk_id >= num_chunks)
    return;

  int len = lengths[chunk_id], in_base = chunk_id * chunk_size,
      out_base = chunk_id * max_words;
  uint64_t bit_buf = 0;
  int bit_cnt = 0, word_off = 0, x = L;
  int clz_L = __clz(L);

  for (int i = len - 1; i >= 0; i--) {
    unsigned short s = symbols[in_base + i];
    int mx = s_max_x[s];
    int k = __clz(mx) - clz_L;
    k += ((x >> k) > mx);
    if (k > 0) {
      uint32_t bits = (uint32_t)x & ((1u << k) - 1u);
      x >>= k;
      bit_buf |= ((uint64_t)bits << bit_cnt);
      bit_cnt += k;
      if (bit_cnt >= 64) {
        if (word_off >= max_words) {
          atomicExch(d_overflow_flag, 1u);
          return;
        }
        out_words[out_base + word_off++] = bit_buf;
        bit_cnt -= 64;
        bit_buf = (bit_cnt > 0) ? ((uint64_t)bits >> (k - bit_cnt)) : 0ULL;
      }
    }
    x = s_enc[s_prefix_p[s] + x - s_p[s]];
  }

  int state_bits = 32 - clz_L;
  uint32_t final_bits = (uint32_t)x & ((1u << state_bits) - 1u);
  bit_buf |= ((uint64_t)final_bits << bit_cnt);
  bit_cnt += state_bits;
  if (bit_cnt >= 64) {
    if (word_off >= max_words) {
      atomicExch(d_overflow_flag, 1u);
      return;
    }
    out_words[out_base + word_off++] = bit_buf;
    bit_cnt -= 64;
    bit_buf =
        (bit_cnt > 0) ? ((uint64_t)final_bits >> (state_bits - bit_cnt)) : 0ULL;
  }
  if (bit_cnt > 0) {
    if (word_off >= max_words) {
      atomicExch(d_overflow_flag, 1u);
      return;
    }
    out_words[out_base + word_off++] = bit_buf;
  }
  bit_lengths[chunk_id] = (word_off * 64) - (64 - bit_cnt);
  if (bit_cnt == 0)
    bit_lengths[chunk_id] = word_off * 64;
}

// -------------------------------------------------------------------------
// TABLE-BASED DECODER KERNEL
// -------------------------------------------------------------------------

__global__ void tabled_decode_kernel(
    const uint64_t *__restrict__ in_words,
    const unsigned int *__restrict__ word_offsets,
    const unsigned int *__restrict__ bit_lengths, unsigned char *__restrict__ bwt,
    const int *__restrict__ dec_table, const int *__restrict__ dec_symbol,
    int total_size, int chunk_size, int L) {
  extern __shared__ int s_dec_mem[];
  int *s_dec_table = s_dec_mem;
  int *s_dec_symbol = &s_dec_mem[L];

  unsigned char *s_alphabets = (unsigned char *)&s_dec_mem[2 * L];
  unsigned char *alphabet = &s_alphabets[threadIdx.x * 260];

#if defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 800
  for (int i = threadIdx.x * 4; i < L; i += blockDim.x * 4) {
    if (i + 3 < L) {
      cp_async_16(&s_dec_table[i], &dec_table[i]);
      cp_async_16(&s_dec_symbol[i], &dec_symbol[i]);
    } else {
      for (int k = i; k < L; k++) {
        s_dec_table[k] = dec_table[k];
        s_dec_symbol[k] = dec_symbol[k];
      }
    }
  }
  cp_async_commit();
  cp_async_wait_all();
  __syncthreads();
#else
  for (int i = threadIdx.x; i < L; i += blockDim.x) {
    s_dec_table[i] = dec_table[i];
    s_dec_symbol[i] = dec_symbol[i];
  }
  __syncthreads();
#endif

  for (int i = 0; i < 256; i++)
    alphabet[i] = (unsigned char)i;

  int chunk_id = blockIdx.x * blockDim.x + threadIdx.x;
  int start = chunk_id * chunk_size;
  if (start >= total_size)
    return;
  int end = start + chunk_size;
  if (end > total_size)
    end = total_size;

  int word_base = word_offsets[chunk_id];
  int current_bit_idx = (int)bit_lengths[chunk_id] - 1;
  int clz_L = __clz(L);
  int state_bits = 32 - clz_L;
  int cached_w_idx = (current_bit_idx >= 0) ? (current_bit_idx >> 6) : -1;
  uint64_t cached_word =
      (cached_w_idx >= 0) ? in_words[word_base + cached_w_idx] : 0ULL;

  int x = 0;
  if (current_bit_idx >= state_bits - 1) {
    int low_bit = current_bit_idx - state_bits + 1;
    int w_idx = low_bit >> 6;
    int b_off = low_bit & 63;
    uint64_t w1 = cached_word;
    if (w_idx != cached_w_idx) {
      cached_word = in_words[word_base + w_idx];
      cached_w_idx = w_idx;
    }
    uint64_t combined = (b_off + state_bits <= 64)
                            ? (cached_word >> b_off)
                            : ((cached_word >> b_off) | (w1 << (64 - b_off)));
    x = (int)((uint32_t)combined & ((1u << state_bits) - 1u));
    current_bit_idx -= state_bits;
  } else {
    for (int b = state_bits - 1; b >= 0; b--) {
      unsigned int bit = 0;
      if (current_bit_idx >= 0) {
        bit = (unsigned int)((in_words[word_base + (current_bit_idx >> 6)] >>
                              (current_bit_idx & 63)) &
                             1ULL);
        current_bit_idx--;
      }
      x |= (bit << b);
    }
  }

  int out_idx = start, run_length = 0, power = 1;
  while (out_idx + run_length < end) {
    int s = s_dec_symbol[x - L];
    x = s_dec_table[x - L];
    if (x < L) {
      int k = __clz(x) - clz_L;
      if (current_bit_idx >= k - 1) {
        int low_bit = current_bit_idx - k + 1;
        int w_idx = low_bit >> 6;
        int b_off = low_bit & 63;
        uint64_t w1 = cached_word;
        if (w_idx != cached_w_idx) {
          cached_word = in_words[word_base + w_idx];
          cached_w_idx = w_idx;
        }
        uint64_t combined =
            (b_off + k <= 64) ? (cached_word >> b_off)
                              : ((cached_word >> b_off) | (w1 << (64 - b_off)));
        uint32_t bits = (uint32_t)combined & ((1u << k) - 1u);
        x = (x << k) | (int)bits;
        current_bit_idx -= k;
      } else if (current_bit_idx >= 0) {
        int avail = current_bit_idx + 1;
        uint32_t bits =
            (uint32_t)(in_words[word_base] & ((1ULL << avail) - 1ULL));
        x = ((x << avail) | (int)bits) << (k - avail);
        current_bit_idx = -1;
      } else {
        x <<= k;
      }
    }
    if (s == 0) {
      run_length += power * 1;
      power *= 2;
    } else if (s == 1) {
      run_length += power * 2;
      power *= 2;
    } else {
      if (run_length > 0) {
        unsigned char c0 = alphabet[0];
        for (int i = 0; i < run_length && out_idx < end; i++)
          bwt[out_idx++] = c0;
        run_length = 0;
        power = 1;
      }
      if (out_idx < end) {
        unsigned int idx = s - 1;
        unsigned char c = alphabet[idx];
        bwt[out_idx++] = c;
        if (idx == 1) {
          alphabet[1] = alphabet[0];
          alphabet[0] = c;
        } else if (idx > 1) {
          for (int j = idx; j > 0; j--)
            alphabet[j] = alphabet[j - 1];
          alphabet[0] = c;
        }
      }
    }
  }
  if (run_length > 0) {
    unsigned char c0 = alphabet[0];
    for (int i = 0; i < run_length && out_idx < end; i++)
      bwt[out_idx++] = c0;
  }
}

__global__ void dense_pack_kernel(const uint64_t *__restrict__ in,
                                  const unsigned int *__restrict__ offsets,
                                  const unsigned int *__restrict__ lens,
                                  uint64_t *__restrict__ out, int n,
                                  int max_w) {
  int cid = blockIdx.x * blockDim.x + threadIdx.x;
  if (cid >= n)
    return;
  int len = lens[cid], in_b = cid * max_w, out_b = offsets[cid];

  uintptr_t in_ptr = reinterpret_cast<uintptr_t>(in + in_b);
  uintptr_t out_ptr = reinterpret_cast<uintptr_t>(out + out_b);

  // SAFE ALIGNMENT CHECK: Ensure both pointers are 16-byte aligned before
  // vector load/store
  if ((in_ptr % 16 == 0) && (out_ptr % 16 == 0)) {
    const ulonglong2 *in_128 = reinterpret_cast<const ulonglong2 *>(in + in_b);
    ulonglong2 *out_128 = reinterpret_cast<ulonglong2 *>(out + out_b);
    int vec_len = len / 2;
    for (int i = 0; i < vec_len; i++) {
      out_128[i] = in_128[i];
    }
    if (len % 2) {
      out[out_b + len - 1] = in[in_b + len - 1];
    }
  } else {
    for (int i = 0; i < len; i++) {
      out[out_b + i] = in[in_b + i];
    }
  }
}

__global__ void bwt_block_hist_kernel(const unsigned char *__restrict__ bwt,
                                      uint32_t *__restrict__ block_hist,
                                      int n, int num_blocks) {
  __shared__ uint32_t s_hist[256];
  int tid = threadIdx.x;
  s_hist[tid] = 0;
  __syncthreads();

  int start = blockIdx.x * 4096;
  int end = start + 4096;
  if (end > n)
    end = n;

  for (int i = start + tid; i < end; i += 256) {
    atomicAdd(&s_hist[bwt[i]], 1U);
  }
  __syncthreads();

  block_hist[tid * num_blocks + blockIdx.x] = s_hist[tid];
}

__global__ void bwt_scatter_lf_kernel(const unsigned char *__restrict__ bwt,
                                      const uint32_t *__restrict__ block_offsets,
                                      int *__restrict__ LF,
                                      int n, int num_blocks) {
  __shared__ uint32_t s_base[256];
  __shared__ uint16_t s_warp_hist[8][256];

  int tid = threadIdx.x;
  int warp_id = tid >> 5;
  int lane = tid & 31;

  s_base[tid] = block_offsets[tid * num_blocks + blockIdx.x];
  for (int w = 0; w < 8; w++) {
    s_warp_hist[w][tid] = 0;
  }
  __syncthreads();

  int start = blockIdx.x * 4096;
  for (int step = 0; step < 16; step++) {
    int pos = start + step * 256 + tid;
    bool valid = (pos < n);
    uint32_t key = valid ? (uint32_t)bwt[pos] : 256U;
    uint32_t mask = __match_any_sync(0xFFFFFFFFU, key);
    int warp_rank = 0;

    if (valid) {
      warp_rank = __popc(mask & ((1U << lane) - 1));
      if (lane == (__ffs(mask) - 1)) {
        s_warp_hist[warp_id][key] = (uint16_t)__popc(mask);
      }
    }
    __syncthreads();

    if (valid) {
      uint32_t prefix = s_base[key] + (uint32_t)warp_rank;
      for (int w = 0; w < warp_id; w++) {
        prefix += s_warp_hist[w][key];
      }
      LF[pos] = (int)prefix;
    }
    __syncthreads();

    uint32_t step_sum = 0;
    for (int w = 0; w < 8; w++) {
      step_sum += s_warp_hist[w][tid];
      s_warp_hist[w][tid] = 0;
    }
    s_base[tid] += step_sum;
    __syncthreads();
  }
}

__global__ void anchor_init_kernel(const int *__restrict__ LF,
                                   const int *__restrict__ primary_idx,
                                   int2 *__restrict__ JD_in,
                                   int n, int num_anchors) {
  int aid = blockIdx.x * blockDim.x + threadIdx.x;
  if (aid > num_anchors)
    return;

  int prim = *primary_idx;
  if (aid == num_anchors && (prim & 127) == 0) {
    JD_in[aid] = make_int2(aid, 0);
    return;
  }

  int start_gid = (aid < num_anchors) ? (aid << 7) : prim;
  if (start_gid == prim) {
    JD_in[aid] = make_int2(aid, 0);
    return;
  }

  int curr = LF[start_gid];
  int steps = 1;
  while ((curr & 127) != 0 && curr != prim) {
    curr = LF[curr];
    steps++;
  }
  int next_aid = ((curr & 127) == 0) ? (curr >> 7) : num_anchors;
  JD_in[aid] = make_int2(next_aid, steps);
}

__global__ void anchor_jump_step_kernel(const int2 *__restrict__ JD_in,
                                        int2 *__restrict__ JD_out,
                                        int total_anchors) {
  int aid = blockIdx.x * blockDim.x + threadIdx.x;
  if (aid >= total_anchors)
    return;
  int2 my_jd = JD_in[aid];
  int2 target_jd = JD_in[my_jd.x];
  JD_out[aid] = make_int2(target_jd.x, my_jd.y + target_jd.y);
}

__global__ void anchor_scatter_kernel(const int2 *__restrict__ JD,
                                      const int *__restrict__ LF,
                                      const unsigned char *__restrict__ bwt,
                                      unsigned char *__restrict__ out,
                                      const int *__restrict__ primary_idx,
                                      int n, int num_anchors) {
  int aid = blockIdx.x * blockDim.x + threadIdx.x;
  if (aid > num_anchors)
    return;

  int prim = *primary_idx;
  if (aid == num_anchors && (prim & 127) == 0)
    return;

  int start_gid = (aid < num_anchors) ? (aid << 7) : prim;
  int pos = (start_gid == prim) ? (n - 1) : (JD[aid].y - 1);
  int curr = start_gid;
  do {
    out[pos--] = bwt[curr];
    curr = LF[curr];
  } while ((curr & 127) != 0 && curr != prim);
}

__global__ void gpu_hash_kernel(const unsigned char *__restrict__ data,
                                uint64_t *__restrict__ d_hash, int size) {
  int tid = threadIdx.x;
  int idx = blockIdx.x * blockDim.x + tid;
  uint64_t local_hash = 0;

  int num_words = (size + 7) / 8;

  if (idx < num_words) {
    int start_byte = idx * 8;
    int remain = size - start_byte;
    uint64_t val = 0;

    if (remain >= 8) {
      val = *reinterpret_cast<const uint64_t *>(data + start_byte);
    } else {
      for (int i = 0; i < remain; i++) {
        val |= (static_cast<uint64_t>(data[start_byte + i])) << (i * 8);
      }
    }

    local_hash = val * 0xbf58476d1ce4e5b9ULL;
    local_hash ^= (local_hash >> 31);
  }

  for (int offset = 16; offset > 0; offset /= 2) {
    local_hash ^= __shfl_down_sync(0xFFFFFFFF, local_hash, offset);
  }

  __shared__ uint64_t shared_hash[32];
  if (tid % 32 == 0)
    shared_hash[tid / 32] = local_hash;
  __syncthreads();

  if (tid < 32) {
    local_hash = (tid < ((blockDim.x + 31) / 32)) ? shared_hash[tid] : 0;
    for (int offset = 16; offset > 0; offset /= 2) {
      local_hash ^= __shfl_down_sync(0xFFFFFFFF, local_hash, offset);
    }
    if (tid == 0 && local_hash != 0) {
      atomicXor(reinterpret_cast<unsigned long long *>(d_hash),
                (unsigned long long)local_hash);
    }
  }
}