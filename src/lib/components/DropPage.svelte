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
  import { slide } from "svelte/transition";
  import { open, save } from "@tauri-apps/plugin-dialog";
  import { invoke } from "@tauri-apps/api/core";
  import { appState } from "../stores/appState.svelte";
  import type { ProfilePreset, ArchiveInspectResult } from "../types";
  import {
    Upload,
    Folder,
    FileText,
    Zap,
    Shield,
    Sparkles,
    Layers,
    ArrowRight,
    X,
    Sliders,
    Gauge,
  } from "lucide-svelte";
  import CustomConfigModal from "./CustomConfigModal.svelte";

  let customModalRef: CustomConfigModal | null = $state(null);

  const profiles = [
    {
      id: "balanced" as ProfilePreset,
      name: "Balanced",
      desc: "4 MB Macro / 512 B Mini",
      icon: Zap,
    },
    {
      id: "speed" as ProfilePreset,
      name: "Speed",
      desc: "4 MB Macro / 512 B Mini",
      icon: Sparkles,
    },
    {
      id: "ratio" as ProfilePreset,
      name: "Ratio",
      desc: "4 MB Macro / 2 KB Mini",
      icon: Shield,
    },
    {
      id: "best_speed" as ProfilePreset,
      name: "Best speed",
      desc: "4 MB Macro / 256 B Mini",
      icon: Zap,
    },
    {
      id: "best_ratio" as ProfilePreset,
      name: "Best ratio",
      desc: "8 MB Macro / 8 KB Mini",
      icon: Layers,
    },
  ];

  async function chooseFiles() {
    const selected = await open({ multiple: true });
    if (selected) {
      const paths = Array.isArray(selected) ? selected : [selected];
      handlePaths(paths);
    }
  }

  async function chooseFolder() {
    const selected = await open({ directory: true, multiple: false });
    if (selected && typeof selected === "string") {
      handlePaths([selected]);
    }
  }

  function openBenchmark() {
    appState.benchmarkRows = [];
    appState.showBenchmarkModal = true;
  }

  async function handlePaths(paths: string[]) {
    if (paths.length === 0) return;

    if (paths.length === 1 && paths[0].endsWith(".gcmp")) {
      try {
        const data = await invoke<ArchiveInspectResult>("inspect_archive", {
          archivePath: paths[0],
        });
        appState.setArchive(paths[0], data);
      } catch (err: any) {
        appState.showAlert(
          "Failed to Open Archive",
          `Cannot read archive: ${err}`,
        );
      }
      return;
    }

    appState.dropTargets = Array.from(
      new Set([...appState.dropTargets, ...paths]),
    );
  }

  function removeTarget(idx: number) {
    appState.dropTargets = appState.dropTargets.filter((_, i) => i !== idx);
  }

  async function startCompression() {
    if (appState.dropTargets.length === 0) return;

    const defaultName =
      appState.dropTargets.length === 1
        ? appState.dropTargets[0].split(/[/\\]/).pop() + ".gcmp"
        : "archive.gcmp";

    const outPath = await save({
      defaultPath: defaultName,
      filters: [{ name: "GPUCompact Archive", extensions: ["gcmp"] }],
    });

    if (!outPath) return;

    appState.isOperating = true;
    appState.operatingTitle = `Compressing ${appState.dropTargets.length} item(s)...`;
    appState.progress = null;

    try {
      await invoke("compress_archive", {
        inputs: appState.dropTargets,
        output: outPath,
        profile: appState.selectedProfile,
        customConfig:
          appState.selectedProfile === "custom" ? appState.customConfig : null,
      });

      const data = await invoke<ArchiveInspectResult>("inspect_archive", {
        archivePath: outPath,
      });
      appState.setArchive(outPath, data);
    } catch (err: any) {
      appState.showAlert("Compression Error", `GPU engine failed: ${err}`);
    } finally {
      appState.isOperating = false;
    }
  }
</script>

<div class="h-full w-full flex flex-col p-6 space-y-6 overflow-y-auto">
  <div
    class="fluent-panel flex-1 min-h-[220px] p-8 flex flex-col items-center justify-center text-center relative group border-dashed border-white/10 hover:border-indigo-500/40 transition-all"
  >
    <div
      class="w-14 h-14 rounded-2xl bg-indigo-500/10 border border-indigo-500/20 flex items-center justify-center text-indigo-400 mb-4 group-hover:scale-105 group-hover:bg-indigo-500/20 transition-all shadow-lg shadow-indigo-500/5"
    >
      <Upload size={26} />
    </div>

    <h2 class="text-base font-semibold text-white mb-1">
      Drag & Drop Files or Folders
    </h2>
    <p class="text-xs text-zinc-400 max-w-sm mb-6">
      Drop any folder or dataset to compress using CUDA acceleration, or drop a <span
        class="text-indigo-400 font-mono">.gcmp</span
      > archive to inspect.
    </p>

    <div class="flex items-center space-x-3">
      <button
        onclick={chooseFiles}
        class="px-4 py-2 rounded-xl bg-white/5 hover:bg-white/10 border border-white/10 text-xs font-medium text-white flex items-center space-x-2 transition-all shadow-md active:scale-95 cursor-pointer"
      >
        <FileText size={14} class="text-indigo-400" />
        <span>Choose File(s)...</span>
      </button>

      <button
        onclick={chooseFolder}
        class="px-4 py-2 rounded-xl bg-white/5 hover:bg-white/10 border border-white/10 text-xs font-medium text-white flex items-center space-x-2 transition-all shadow-md active:scale-95 cursor-pointer"
      >
        <Folder size={14} class="text-indigo-400" />
        <span>Choose Folder...</span>
      </button>

      <button
        onclick={openBenchmark}
        class="px-4 py-2 rounded-xl bg-indigo-600/20 hover:bg-indigo-600/30 border border-indigo-500/30 text-xs font-semibold text-indigo-300 flex items-center space-x-2 transition-all shadow-md active:scale-95 cursor-pointer"
      >
        <Gauge size={14} class="text-indigo-400" />
        <span>Run Benchmark...</span>
      </button>
    </div>
  </div>

  {#if appState.dropTargets.length > 0}
    <div
      transition:slide={{ duration: 180 }}
      class="fluent-panel p-4 space-y-3"
    >
      <div class="flex items-center justify-between px-1">
        <span class="text-xs font-semibold text-zinc-300"
          >Selected Items ({appState.dropTargets.length})</span
        >
        <button
          onclick={() => (appState.dropTargets = [])}
          class="text-[11px] text-zinc-500 hover:text-red-400 transition-colors cursor-pointer"
        >
          Clear All
        </button>
      </div>

      <div class="max-h-36 overflow-y-auto space-y-1.5 pr-1">
        {#each appState.dropTargets as path, idx (path)}
          <div
            transition:slide={{ duration: 150 }}
            class="flex items-center justify-between px-3 py-1.5 rounded-lg bg-black/40 border border-white/5 text-xs transition-all"
          >
            <span class="text-zinc-300 font-mono truncate max-w-lg">{path}</span
            >
            <button
              onclick={() => removeTarget(idx)}
              class="text-zinc-500 hover:text-zinc-300 p-0.5 cursor-pointer transition-colors"
            >
              <X size={13} />
            </button>
          </div>
        {/each}
      </div>
    </div>
  {/if}

  <div class="space-y-2.5">
    <div class="flex items-center justify-between px-1">
      <span class="text-xs font-semibold text-zinc-400 uppercase tracking-wider"
        >Compression Preset</span
      >
    </div>

    <div class="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-6 gap-3">
      {#each profiles as p}
        {@const Icon = p.icon}
        <button
          onclick={() => (appState.selectedProfile = p.id)}
          class="fluent-panel p-3.5 text-left flex flex-col justify-between transition-all hover:bg-zinc-800/40 relative overflow-hidden group border-white/5 cursor-pointer {appState.selectedProfile ===
          p.id
            ? 'border-indigo-500/50 bg-indigo-500/10 shadow-lg shadow-indigo-500/10'
            : ''}"
        >
          <div>
            <div class="flex items-center justify-between mb-2">
              <span class="text-xs font-semibold text-white truncate"
                >{p.name}</span
              >
              <Icon
                size={14}
                class={appState.selectedProfile === p.id
                  ? "text-indigo-400"
                  : "text-zinc-500"}
              />
            </div>
            <p class="text-[10px] text-zinc-400 leading-tight">{p.desc}</p>
          </div>
        </button>
      {/each}

      <button
        onclick={() => {
          appState.selectedProfile = "custom";
          customModalRef?.openModal();
        }}
        class="fluent-panel p-3.5 text-left flex flex-col justify-between transition-all hover:bg-zinc-800/40 relative overflow-hidden group border-white/5 cursor-pointer {appState.selectedProfile ===
        'custom'
          ? 'border-indigo-500/50 bg-indigo-500/10 shadow-lg shadow-indigo-500/10'
          : ''}"
      >
        <div>
          <div class="flex items-center justify-between mb-2">
            <span class="text-xs font-semibold text-white">Custom...</span>
            <Sliders
              size={14}
              class={appState.selectedProfile === "custom"
                ? "text-indigo-400"
                : "text-zinc-500"}
            />
          </div>
          <p class="text-[10px] text-zinc-400 leading-tight">
            Macro={appState.customConfig.macro_mb}MB / Mini={appState
              .customConfig.mini_size}B / L={appState.customConfig.l_state}
          </p>
        </div>
      </button>
    </div>
  </div>

  {#if appState.dropTargets.length > 0}
    <div transition:slide={{ duration: 150 }} class="pt-2 flex justify-end">
      <button
        onclick={startCompression}
        class="px-6 py-2.5 rounded-xl bg-indigo-600 hover:bg-indigo-500 text-xs font-semibold text-white flex items-center space-x-2 shadow-lg shadow-indigo-600/30 transition-all active:scale-95 cursor-pointer"
      >
        <span>Compress to Archive</span>
        <ArrowRight size={14} />
      </button>
    </div>
  {/if}
</div>

<CustomConfigModal bind:this={customModalRef} />
