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
  import { getCurrentWindow } from '@tauri-apps/api/window';
  import { Zap, Minus, Square, X } from 'lucide-svelte';
  import { appState } from '../stores/appState.svelte';

  const appWindow = getCurrentWindow();

  function startDrag(e: MouseEvent) {
    if (e.buttons === 1) {
      appWindow.startDragging();
    }
  }

  async function minimize(e: MouseEvent) {
    e.stopPropagation();
    try {
      await appWindow.minimize();
    } catch (err) {
      console.error('Minimize error:', err);
    }
  }

  async function toggleMaximize(e: MouseEvent) {
    e.stopPropagation();
    try {
      await appWindow.toggleMaximize();
    } catch (err) {
      console.error('Maximize error:', err);
    }
  }

  async function close(e: MouseEvent) {
    e.stopPropagation();
    try {
      await appWindow.close();
    } catch (err) {
      console.error('Close error:', err);
    }
  }
</script>

<!-- svelte-ignore a11y_no_static_element_interactions -->
<div 
  onmousedown={startDrag}
  ondblclick={toggleMaximize}
  class="h-10 w-full bg-[#09090b]/90 backdrop-blur-xl border-b border-white/5 flex items-center justify-between pl-3 select-none z-50 shrink-0 cursor-default"
>
  <!-- Left Brand -->
  <div class="flex items-center space-x-2.5 pointer-events-none">
    <svg class="w-5 h-5 rounded-md" viewBox="0 0 512 512" fill="none" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <linearGradient id="tb-bg" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stop-color="#0f172a" />
          <stop offset="100%" stop-color="#020617" />
        </linearGradient>
        <linearGradient id="tb-stroke" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stop-color="#34d399" />
          <stop offset="50%" stop-color="#10b981" />
          <stop offset="100%" stop-color="#0284c7" />
        </linearGradient>
      </defs>
      <rect width="512" height="512" rx="112" fill="url(#tb-bg)" />
      <path d="M 352 176 A 130 130 0 1 0 352 336 L 256 336 L 256 256 L 320 256" fill="none" stroke="url(#tb-stroke)" stroke-width="36" stroke-linecap="round" stroke-linejoin="round" />
      <path d="M 170 256 L 210 256" stroke="#38bdf8" stroke-width="28" stroke-linecap="round" />
    </svg>
    <span class="text-xs font-semibold tracking-wide text-zinc-200">GPUCompact</span>

    {#if appState.archiveData}
      <span class="text-zinc-600 text-xs">/</span>
      <span class="text-xs text-zinc-400 font-mono truncate max-w-[240px]">{appState.archiveData.metadata.archive_name}</span>
    {/if}
  </div>

  <!-- Right Window Controls -->
  <div class="flex items-center h-full z-50">
    <button 
      onmousedown={(e) => e.stopPropagation()}
      onclick={minimize}
      class="h-full w-11 flex items-center justify-center text-zinc-400 hover:text-white hover:bg-white/10 transition-colors cursor-pointer"
      title="Minimize"
    >
      <Minus size={14} />
    </button>
    <button 
      onmousedown={(e) => e.stopPropagation()}
      onclick={toggleMaximize}
      class="h-full w-11 flex items-center justify-center text-zinc-400 hover:text-white hover:bg-white/10 transition-colors cursor-pointer"
      title="Maximize"
    >
      <Square size={12} />
    </button>
    <button 
      onmousedown={(e) => e.stopPropagation()}
      onclick={close}
      class="h-full w-11 flex items-center justify-center text-zinc-400 hover:text-white hover:bg-red-600 transition-colors cursor-pointer"
      title="Close"
    >
      <X size={14} />
    </button>
  </div>
</div>
