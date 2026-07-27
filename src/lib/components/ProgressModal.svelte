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
  import { Zap, Loader2 } from 'lucide-svelte';

  function formatBytes(bytes: number): string {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  }
</script>

{#if appState.isOperating}
  <div 
    transition:fade={{ duration: 180 }}
    class="fixed inset-0 bg-black/60 backdrop-blur-md flex items-center justify-center z-50 p-6 select-none modal-backdrop-anim"
  >
    <div 
      transition:scale={{ duration: 200, start: 0.95 }}
      class="glass-modal w-full max-w-md p-6 space-y-5 shadow-2xl border border-white/10 modal-card-anim"
    >
      <!-- Modal Header -->
      <div class="flex items-center space-x-3">
        <div class="w-9 h-9 rounded-xl bg-indigo-500/20 border border-indigo-500/30 flex items-center justify-center text-indigo-400 shrink-0">
          <Zap size={18} />
        </div>
        <div class="min-w-0">
          <h3 class="text-sm font-semibold text-white truncate">{appState.operatingTitle}</h3>
          <p class="text-[11px] text-zinc-400 font-sans">GPU-Accelerated Parallel Pipeline</p>
        </div>
      </div>

      <!-- Progress Bar -->
      <div class="space-y-2">
        <div class="flex items-center justify-between text-xs font-mono">
          <span class="text-zinc-300 text-[11px]">
            {#if appState.progress}
              {formatBytes(appState.progress.processed)} of {formatBytes(appState.progress.total)}
            {:else}
              Preparing operation...
            {/if}
          </span>
          <span class="text-indigo-400 font-bold">
            {appState.progress ? `${appState.progress.percentage.toFixed(0)}%` : '0%'}
          </span>
        </div>

        <div class="w-full h-2.5 rounded-full bg-black/60 overflow-hidden border border-white/5 relative">
          <div 
            class="h-full rounded-full bg-indigo-500 transition-all duration-150 shadow-md shadow-indigo-500/40"
            style="width: {appState.progress ? appState.progress.percentage : 0}%;"
          ></div>
        </div>
      </div>

      <!-- Performance Metrics -->
      {#if appState.progress}
        <div class="grid grid-cols-3 gap-2 pt-1 font-mono text-center">
          <div class="p-2 rounded-xl bg-white/5 border border-white/5">
            <span class="block text-[10px] text-zinc-500 font-sans uppercase tracking-wider">Speed</span>
            <span class="text-xs font-semibold text-zinc-200">{appState.progress.speed_mbs.toFixed(2)} MB/s</span>
          </div>

          <div class="p-2 rounded-xl bg-white/5 border border-white/5">
            <span class="block text-[10px] text-zinc-500 font-sans uppercase tracking-wider">Ratio</span>
            <span class="text-xs font-semibold text-indigo-400">{appState.progress.ratio.toFixed(2)}x</span>
          </div>

          <div class="p-2 rounded-xl bg-white/5 border border-white/5">
            <span class="block text-[10px] text-zinc-500 font-sans uppercase tracking-wider">Remaining</span>
            <span class="text-xs font-semibold text-zinc-200">{appState.progress.eta_sec.toFixed(1)}s</span>
          </div>
        </div>
      {:else}
        <div class="flex items-center justify-center py-2 space-x-2 text-zinc-500 text-xs font-sans">
          <Loader2 size={15} class="animate-spin text-indigo-500" />
          <span>Initializing pipeline...</span>
        </div>
      {/if}
    </div>
  </div>
{/if}
