<!--
  GPUCompact
  Copyright (C) 2026 UDPSendToFailed

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
-->
<script lang="ts">
  import { fade, scale } from 'svelte/transition';
  import { appState } from '../stores/appState.svelte';
  import { Sliders, X, Check, Cpu, HardDrive, Layers, Zap } from 'lucide-svelte';

  let showModal = $state(false);

  export function openModal() {
    showModal = true;
  }

  function closeModal() {
    showModal = false;
  }

  function applyConfig() {
    appState.selectedProfile = 'custom';
    showModal = false;
  }
</script>

{#if showModal}
  <div 
    transition:fade={{ duration: 180 }}
    class="fixed inset-0 bg-black/70 backdrop-blur-md flex items-center justify-center z-50 p-6 select-none modal-backdrop-anim"
  >
    <div 
      transition:scale={{ duration: 200, start: 0.95 }}
      class="glass-modal w-full max-w-lg p-6 space-y-6 border border-white/10 shadow-2xl modal-card-anim"
    >
      <!-- Modal Header -->
      <div class="flex items-center justify-between border-b border-white/10 pb-4">
        <div class="flex items-center space-x-3">
          <div class="w-9 h-9 rounded-xl bg-indigo-500/20 border border-indigo-500/30 flex items-center justify-center text-indigo-400">
            <Sliders size={18} />
          </div>
          <div>
            <h3 class="text-sm font-semibold text-white">Custom Compression Setup</h3>
            <p class="text-xs text-zinc-400">Configure Macro-Chunk, Mini-Block, and Thread Allocation</p>
          </div>
        </div>

        <button 
          onclick={closeModal}
          class="p-1.5 rounded-lg hover:bg-white/10 text-zinc-400 hover:text-white transition-colors cursor-pointer"
        >
          <X size={16} />
        </button>
      </div>

      <!-- Hyperparameters Grid -->
      <div class="space-y-4 font-mono text-xs">
        <!-- Macro-Chunk Size -->
        <div class="fluent-panel p-3.5 space-y-2">
          <div class="flex items-center justify-between">
            <span class="text-zinc-200 font-semibold flex items-center space-x-2 font-sans">
              <HardDrive size={14} class="text-indigo-400" />
              <span>Macro-Chunk Size</span>
            </span>
            <span class="text-indigo-400 font-bold">{appState.customConfig.macro_mb} MB</span>
          </div>
          <div class="grid grid-cols-4 gap-2">
            {#each [1, 2, 4, 8] as m}
              <button 
                onclick={() => appState.customConfig.macro_mb = m}
                class="py-1.5 rounded-lg text-center border transition-all cursor-pointer {appState.customConfig.macro_mb === m ? 'bg-indigo-600 text-white border-indigo-500 font-bold' : 'bg-black/40 text-zinc-400 border-white/5 hover:bg-white/5'}"
              >
                {m} MB
              </button>
            {/each}
          </div>
        </div>

        <!-- Mini-Block Size -->
        <div class="fluent-panel p-3.5 space-y-2">
          <div class="flex items-center justify-between">
            <span class="text-zinc-200 font-semibold flex items-center space-x-2 font-sans">
              <Layers size={14} class="text-indigo-400" />
              <span>Mini-Block Size</span>
            </span>
            <span class="text-indigo-400 font-bold">{appState.customConfig.mini_size} B</span>
          </div>
          <div class="grid grid-cols-6 gap-1.5">
            {#each [256, 512, 1024, 2048, 4096, 8192] as mb}
              <button 
                onclick={() => appState.customConfig.mini_size = mb}
                class="py-1.5 rounded-lg text-center border text-[11px] transition-all cursor-pointer {appState.customConfig.mini_size === mb ? 'bg-indigo-600 text-white border-indigo-500 font-bold' : 'bg-black/40 text-zinc-400 border-white/5 hover:bg-white/5'}"
              >
                {mb} B
              </button>
            {/each}
          </div>
        </div>

        <!-- tANS L-State Window -->
        <div class="fluent-panel p-3.5 space-y-2">
          <div class="flex items-center justify-between">
            <span class="text-zinc-200 font-semibold flex items-center space-x-2 font-sans">
              <Zap size={14} class="text-indigo-400" />
              <span>tANS State Window (L)</span>
            </span>
            <span class="text-indigo-400 font-bold">{appState.customConfig.l_state}</span>
          </div>
          <div class="grid grid-cols-4 gap-2">
            {#each [512, 1024, 2048, 4096] as l}
              <button 
                onclick={() => appState.customConfig.l_state = l}
                class="py-1.5 rounded-lg text-center border transition-all cursor-pointer {appState.customConfig.l_state === l ? 'bg-indigo-600 text-white border-indigo-500 font-bold' : 'bg-black/40 text-zinc-400 border-white/5 hover:bg-white/5'}"
              >
                L={l}
              </button>
            {/each}
          </div>
        </div>

        <!-- Worker Threads -->
        <div class="fluent-panel p-3.5 space-y-3">
          <div class="flex items-center space-x-2 text-zinc-200 font-semibold font-sans">
            <Cpu size={14} class="text-indigo-400" />
            <span>Worker Threads</span>
          </div>

          <div class="grid grid-cols-2 gap-4 font-sans">
            <div>
              <label for="threads-comp" class="block text-[11px] text-zinc-400 mb-1">Compression Threads: <strong class="text-white font-mono">{appState.customConfig.threads_comp}</strong></label>
              <input 
                id="threads-comp"
                type="range" 
                min="1" 
                max="16" 
                bind:value={appState.customConfig.threads_comp}
                class="w-full accent-indigo-500"
              />
            </div>

            <div>
              <label for="threads-decomp" class="block text-[11px] text-zinc-400 mb-1">Decompression Threads: <strong class="text-white font-mono">{appState.customConfig.threads_decomp}</strong></label>
              <input 
                id="threads-decomp"
                type="range" 
                min="16" 
                max="128" 
                step="8"
                bind:value={appState.customConfig.threads_decomp}
                class="w-full accent-indigo-500"
              />
            </div>
          </div>
        </div>
      </div>

      <!-- Action Buttons -->
      <div class="flex items-center justify-end space-x-3 pt-2">
        <button 
          onclick={closeModal}
          class="px-4 py-2 rounded-xl bg-white/5 hover:bg-white/10 text-xs text-zinc-300 font-medium transition-colors cursor-pointer"
        >
          Cancel
        </button>
        <button 
          onclick={applyConfig}
          class="px-5 py-2 rounded-xl bg-indigo-600 hover:bg-indigo-500 text-xs text-white font-semibold flex items-center space-x-1.5 shadow-md shadow-indigo-600/30 transition-all cursor-pointer"
        >
          <Check size={14} />
          <span>Apply Configuration</span>
        </button>
      </div>
    </div>
  </div>
{/if}
