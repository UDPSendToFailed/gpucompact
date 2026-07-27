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
  import { fade, scale } from "svelte/transition";
  import { appState } from "../stores/appState.svelte";
  import type { CollisionMode } from "../types";
  import {
    AlertTriangle,
    Replace,
    CornerUpRight,
    SkipForward,
    X,
    Search,
    FileText,
  } from "lucide-svelte";

  let filterQuery = $state("");

  function formatBytes(bytes: number): string {
    if (bytes === 0) return "0 B";
    const k = 1024;
    const sizes = ["B", "KB", "MB", "GB", "TB"];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + " " + sizes[i];
  }

  let filteredCollisions = $derived.by(() => {
    if (!appState.collisionScan) return [];
    const list = appState.collisionScan.colliding_paths;
    if (!filterQuery.trim()) return list;
    const q = filterQuery.toLowerCase();
    return list.filter((p) => p.toLowerCase().includes(q));
  });

  function resolveCollision(mode: CollisionMode | "cancel") {
    const cb = appState.collisionCallback;
    appState.showCollisionModal = false;
    appState.collisionScan = null;
    appState.collisionCallback = null;
    filterQuery = "";
    if (cb) cb(mode);
  }
</script>

{#if appState.showCollisionModal && appState.collisionScan}
  <div
    transition:fade={{ duration: 180 }}
    class="fixed inset-0 bg-black/60 backdrop-blur-md flex items-center justify-center z-50 p-6 select-none modal-backdrop-anim"
  >
    <div
      transition:scale={{ duration: 200, start: 0.95 }}
      class="glass-modal w-full max-w-lg p-6 space-y-5 modal-card-anim border border-white/10 shadow-2xl"
    >
      <!-- Header -->
      <div class="flex items-center space-x-3 border-b border-white/10 pb-4">
        <div
          class="w-10 h-10 rounded-2xl bg-amber-500/20 border border-amber-500/30 flex items-center justify-center text-amber-400 shrink-0"
        >
          <AlertTriangle size={20} />
        </div>
        <div>
          <h3 class="text-sm font-semibold text-white">
            Path Conflicts Detected
          </h3>
          <p class="text-xs text-zinc-400 mt-0.5">
            Found <strong class="text-amber-400"
              >{appState.collisionScan.colliding_paths.length}</strong
            >
            conflict(s) out of {appState.collisionScan.total_incoming_files} file(s)
            ({formatBytes(appState.collisionScan.total_incoming_bytes)} total)
          </p>
        </div>
      </div>

      <!-- Scrollable Path Inspector -->
      <div class="space-y-2 font-mono text-xs">
        <div class="relative">
          <Search size={13} class="absolute left-3 top-2.5 text-zinc-500" />
          <input
            type="text"
            bind:value={filterQuery}
            placeholder="Search conflicting paths..."
            class="input-standard w-full pl-8 pr-3 py-1.5 font-sans"
          />
        </div>

        <div
          class="fluent-panel max-h-40 overflow-y-auto p-2 space-y-1 divide-y divide-white/5 bg-black/40"
        >
          {#each filteredCollisions as path (path)}
            <div
              class="flex items-center space-x-2 py-1 px-2 text-[11px] text-zinc-300 rounded hover:bg-white/5"
            >
              <FileText size={12} class="text-amber-400 shrink-0" />
              <span class="truncate">{path}</span>
            </div>
          {/each}

          {#if filteredCollisions.length === 0}
            <div class="text-center py-4 text-zinc-500 font-sans text-xs">
              No matching conflicting paths found.
            </div>
          {/if}
        </div>
      </div>

      <!-- Action Buttons -->
      <div class="space-y-2 pt-1 font-sans">
        <div class="grid grid-cols-3 gap-2">
          <button
            onclick={() => resolveCollision("replace")}
            class="px-3 py-2.5 rounded-xl bg-amber-600 hover:bg-amber-500 text-xs font-semibold text-white flex items-center justify-center space-x-1.5 transition-all shadow-md active:scale-95 cursor-pointer"
            title="Overwrite conflicting archive entries with incoming files"
          >
            <Replace size={14} class="shrink-0" />
            <span>Replace</span>
          </button>

          <button
            onclick={() => resolveCollision("rename")}
            class="px-3 py-2.5 rounded-xl bg-indigo-600 hover:bg-indigo-500 text-xs font-semibold text-white flex items-center justify-center space-x-1.5 transition-all shadow-md active:scale-95 cursor-pointer"
            title="Keep existing entries and auto-number incoming files"
          >
            <CornerUpRight size={14} class="shrink-0" />
            <span>Auto-Rename</span>
          </button>

          <button
            onclick={() => resolveCollision("skip")}
            class="px-3 py-2.5 rounded-xl bg-white/10 hover:bg-white/15 border border-white/10 text-xs font-medium text-zinc-300 hover:text-white flex items-center justify-center space-x-1.5 transition-all active:scale-95 cursor-pointer"
            title="Skip conflicting items and append clean new files"
          >
            <SkipForward size={14} class="shrink-0" />
            <span>Skip</span>
          </button>
        </div>

        <button
          onclick={() => resolveCollision("cancel")}
          class="w-full px-4 py-2 rounded-xl bg-white/5 hover:bg-white/10 border border-white/5 text-xs font-medium text-zinc-400 hover:text-white flex items-center justify-center space-x-1.5 transition-all cursor-pointer"
        >
          <X size={14} />
          <span>Cancel Operation</span>
        </button>
      </div>
    </div>
  </div>
{/if}
