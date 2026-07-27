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
    import { fade, scale, slide } from "svelte/transition";
    import { open } from "@tauri-apps/plugin-dialog";
    import { invoke } from "@tauri-apps/api/core";
    import { appState } from "../stores/appState.svelte";
    import type { BenchmarkRowPayload, ProfilePreset } from "../types";
    import {
        Gauge,
        Play,
        Folder,
        FileText,
        X,
        CheckCircle2,
        XCircle,
        Zap,
        Sparkles,
        Shield,
        Layers,
        Copy,
        Check,
        Loader2,
    } from "lucide-svelte";

    let benchTargets = $state<string[]>([]);
    let selectedProfile = $state<ProfilePreset>("balanced");
    let isRunning = $state<boolean>(false);
    let isFinished = $state<boolean>(false);
    let copiedFeedback = $state<boolean>(false);
    let startTime = $state<number>(0);
    let elapsedTime = $state<number>(0);
    let timerInterval: any = null;

    const profiles = [
        {
            id: "balanced" as ProfilePreset,
            name: "Balanced",
            desc: "4 MB / 512 B Mini",
            icon: Zap,
        },
        {
            id: "speed" as ProfilePreset,
            name: "Speed",
            desc: "4 MB / 512 B Mini",
            icon: Sparkles,
        },
        {
            id: "ratio" as ProfilePreset,
            name: "Ratio",
            desc: "4 MB / 2 KB Mini",
            icon: Shield,
        },
        {
            id: "best_speed" as ProfilePreset,
            name: "Best speed",
            desc: "4 MB / 256 B Mini",
            icon: Zap,
        },
        {
            id: "best_ratio" as ProfilePreset,
            name: "Best ratio",
            desc: "8 MB / 8 KB Mini",
            icon: Layers,
        },
    ];

    function formatBytes(bytes: number): string {
        if (bytes === 0) return "0 B";
        const k = 1024;
        const sizes = ["B", "KB", "MB", "GB", "TB"];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + " " + sizes[i];
    }

    async function pickFiles() {
        const selected = await open({ multiple: true });
        if (selected) {
            const paths = Array.isArray(selected) ? selected : [selected];
            benchTargets = Array.from(new Set([...benchTargets, ...paths]));
        }
    }

    async function pickFolder() {
        const selected = await open({ directory: true, multiple: false });
        if (selected && typeof selected === "string") {
            benchTargets = Array.from(new Set([...benchTargets, selected]));
        }
    }

    function removeTarget(idx: number) {
        benchTargets = benchTargets.filter((_, i) => i !== idx);
    }

    let summary = $derived.by(() => {
        const rows = appState.benchmarkRows;
        let totalOrig = 0,
            totalComp = 0;
        let allShaPass = true;
        for (const r of rows) {
            totalOrig += r.orig_bytes;
            totalComp += r.comp_bytes;
            if (!r.sha_passed) allShaPass = false;
        }
        const overallRatio = totalComp > 0 ? totalOrig / totalComp : 1.0;
        return {
            totalOrig,
            totalComp,
            overallRatio,
            allShaPass: rows.length > 0 ? allShaPass : true,
            fileCount: rows.length,
        };
    });

    async function startRun() {
        if (benchTargets.length === 0) return;
        appState.benchmarkRows = [];
        isRunning = true;
        isFinished = false;
        startTime = Date.now();
        elapsedTime = 0;

        timerInterval = setInterval(() => {
            elapsedTime = (Date.now() - startTime) / 1000;
        }, 100);

        try {
            await invoke("run_gui_benchmark", {
                inputs: benchTargets,
                profile: selectedProfile,
                customConfig:
                    selectedProfile === "custom" ? appState.customConfig : null,
            });
            isFinished = true;
        } catch (err: any) {
            appState.showAlert("Benchmark Error", `Run failed: ${err}`);
        } finally {
            isRunning = false;
            clearInterval(timerInterval);
        }
    }

    function resetSuite() {
        isRunning = false;
        isFinished = false;
        appState.benchmarkRows = [];
    }

    function closeModal() {
        if (isRunning) return;
        appState.showBenchmarkModal = false;
        resetSuite();
    }

    function copyMarkdownReport() {
        let md = `### GPUCompact Benchmark Report\n\n`;
        md += `| Filename | Orig Size | Comp Size | Ratio | C_Wall (MB/s) | C_GPU (MB/s) | D_Wall (MB/s) | D_GPU (MB/s) | SHA-256 |\n`;
        md += `| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n`;

        for (const r of appState.benchmarkRows) {
            md += `| \`${r.filename}\` | ${formatBytes(r.orig_bytes)} | ${formatBytes(r.comp_bytes)} | ${r.ratio.toFixed(2)}x | ${r.c_wall_mbs.toFixed(1)} | ${r.c_gpu_mbs.toFixed(1)} | ${r.d_wall_mbs.toFixed(1)} | ${r.d_gpu_mbs.toFixed(1)} | ${r.sha_passed ? "PASS" : "FAIL"} |\n`;
        }

        md += `\n**Overall Benchmark Ratio:** ${summary.overallRatio.toFixed(2)}x  \n`;
        md += `**Total Data Processed:** ${formatBytes(summary.totalOrig)}  \n`;
        md += `**Cryptographic Verification:** ${summary.allShaPass ? "100% Lossless (PASS)" : "CORRUPTION DETECTED (FAIL)"}\n`;

        navigator.clipboard.writeText(md);
        copiedFeedback = true;
        setTimeout(() => (copiedFeedback = false), 1500);
    }
</script>

{#if appState.showBenchmarkModal}
    <div
        transition:fade={{ duration: 180 }}
        class="fixed inset-0 bg-black/70 backdrop-blur-md flex items-center justify-center z-50 p-6 select-none modal-backdrop-anim"
    >
        <div
            transition:scale={{ duration: 200, start: 0.95 }}
            class="glass-modal w-full max-w-5xl h-[85vh] p-6 flex flex-col justify-between space-y-4 border border-white/10 shadow-2xl modal-card-anim"
        >
            <!-- Header -->
            <div
                class="flex items-center justify-between border-b border-white/10 pb-4 shrink-0"
            >
                <div class="flex items-center space-x-3">
                    <div
                        class="w-10 h-10 rounded-2xl bg-indigo-500/20 border border-indigo-500/30 flex items-center justify-center text-indigo-400"
                    >
                        <Gauge size={22} />
                    </div>
                    <div>
                        <h3 class="text-sm font-semibold text-white">
                            Hardware Benchmark Suite
                        </h3>
                        <p class="text-xs text-zinc-400">
                            Microsecond GPU Timers & Cryptographic SHA-256
                            Validation
                        </p>
                    </div>
                </div>

                {#if !isRunning}
                    <button
                        onclick={closeModal}
                        class="p-1.5 rounded-lg hover:bg-white/10 text-zinc-400 hover:text-white transition-colors cursor-pointer"
                    >
                        <X size={16} />
                    </button>
                {/if}
            </div>

            <!-- Main Body -->
            <div class="flex-1 min-h-0 flex flex-col space-y-4 overflow-hidden">
                {#if !isRunning && appState.benchmarkRows.length === 0}
                    <!-- Configuration Mode -->
                    <div
                        transition:fade={{ duration: 150 }}
                        class="flex-1 flex flex-col space-y-5 overflow-y-auto pr-1"
                    >
                        <!-- Target Selection -->
                        <div
                            class="fluent-panel p-6 flex flex-col items-center justify-center text-center space-y-4 border-dashed border-white/10"
                        >
                            <div
                                class="w-12 h-12 rounded-2xl bg-indigo-500/10 border border-indigo-500/20 flex items-center justify-center text-indigo-400"
                            >
                                <Folder size={22} />
                            </div>
                            <div>
                                <h4 class="text-xs font-semibold text-white">
                                    Select Benchmark Dataset
                                </h4>
                                <p class="text-[11px] text-zinc-400 mt-0.5">
                                    Pick individual test files or an entire
                                    folder tree to benchmark
                                </p>
                            </div>

                            <div class="flex items-center space-x-3">
                                <button
                                    onclick={pickFiles}
                                    class="px-4 py-2 rounded-xl bg-white/5 hover:bg-white/10 border border-white/10 text-xs font-medium text-white flex items-center space-x-2 transition-all cursor-pointer"
                                >
                                    <FileText
                                        size={14}
                                        class="text-indigo-400"
                                    />
                                    <span>Pick File(s)...</span>
                                </button>

                                <button
                                    onclick={pickFolder}
                                    class="px-4 py-2 rounded-xl bg-white/5 hover:bg-white/10 border border-white/10 text-xs font-medium text-white flex items-center space-x-2 transition-all cursor-pointer"
                                >
                                    <Folder size={14} class="text-indigo-400" />
                                    <span>Pick Folder...</span>
                                </button>
                            </div>
                        </div>

                        <!-- Target Preview List -->
                        {#if benchTargets.length > 0}
                            <div
                                transition:slide={{ duration: 150 }}
                                class="fluent-panel p-4 space-y-2"
                            >
                                <div
                                    class="flex items-center justify-between text-xs px-1"
                                >
                                    <span class="font-semibold text-zinc-300"
                                        >Selected Targets ({benchTargets.length})</span
                                    >
                                    <button
                                        onclick={() => (benchTargets = [])}
                                        class="text-[11px] text-zinc-500 hover:text-red-400"
                                        >Clear</button
                                    >
                                </div>
                                <div
                                    class="max-h-28 overflow-y-auto space-y-1 font-mono text-[11px] pr-1"
                                >
                                    {#each benchTargets as target, idx (target)}
                                        <div
                                            class="flex items-center justify-between px-3 py-1 rounded bg-black/40 border border-white/5"
                                        >
                                            <span
                                                class="truncate text-zinc-300 max-w-xl"
                                                >{target}</span
                                            >
                                            <button
                                                onclick={() =>
                                                    removeTarget(idx)}
                                                class="text-zinc-500 hover:text-zinc-300"
                                                ><X size={12} /></button
                                            >
                                        </div>
                                    {/each}
                                </div>
                            </div>
                        {/if}

                        <!-- Profile Preset Selection -->
                        <div class="space-y-2">
                            <span
                                class="text-xs font-semibold text-zinc-400 uppercase tracking-wider px-1"
                                >Profile Preset</span
                            >
                            <div class="grid grid-cols-5 gap-2.5">
                                {#each profiles as p}
                                    {@const Icon = p.icon}
                                    <button
                                        onclick={() => (selectedProfile = p.id)}
                                        class="fluent-panel p-3 text-left transition-all cursor-pointer border-white/5 {selectedProfile ===
                                        p.id
                                            ? 'border-indigo-500/50 bg-indigo-500/10 shadow-lg shadow-indigo-500/10'
                                            : 'hover:bg-white/5'}"
                                    >
                                        <div
                                            class="flex items-center justify-between mb-1"
                                        >
                                            <span
                                                class="text-xs font-semibold text-white truncate"
                                                >{p.name}</span
                                            >
                                            <Icon
                                                size={13}
                                                class={selectedProfile === p.id
                                                    ? "text-indigo-400"
                                                    : "text-zinc-500"}
                                            />
                                        </div>
                                        <p class="text-[10px] text-zinc-400">
                                            {p.desc}
                                        </p>
                                    </button>
                                {/each}
                            </div>
                        </div>
                    </div>
                {:else}
                    <!-- Streaming Live Table View -->
                    <div
                        transition:fade={{ duration: 150 }}
                        class="flex-1 flex flex-col space-y-3 min-h-0"
                    >
                        <!-- Metrics Banner -->
                        <div
                            class="fluent-panel p-3.5 grid grid-cols-4 gap-3 text-center font-mono shrink-0 bg-black/30"
                        >
                            <div
                                class="p-2 rounded-xl bg-white/5 border border-white/5"
                            >
                                <span
                                    class="block text-[10px] text-zinc-500 font-sans uppercase"
                                    >Processed Data</span
                                >
                                <span
                                    class="text-xs font-semibold text-zinc-200"
                                    >{formatBytes(summary.totalOrig)}</span
                                >
                            </div>

                            <div
                                class="p-2 rounded-xl bg-white/5 border border-white/5"
                            >
                                <span
                                    class="block text-[10px] text-zinc-500 font-sans uppercase"
                                    >Overall Ratio</span
                                >
                                <span
                                    class="text-xs font-semibold text-indigo-400"
                                    >{summary.overallRatio.toFixed(2)}x</span
                                >
                            </div>

                            <div
                                class="p-2 rounded-xl bg-white/5 border border-white/5"
                            >
                                <span
                                    class="block text-[10px] text-zinc-500 font-sans uppercase"
                                    >Elapsed Time</span
                                >
                                <span
                                    class="text-xs font-semibold text-zinc-200"
                                    >{elapsedTime.toFixed(1)}s</span
                                >
                            </div>

                            <div
                                class="p-2 rounded-xl bg-white/5 border border-white/5 flex flex-col items-center justify-center"
                            >
                                <span
                                    class="block text-[10px] text-zinc-500 font-sans uppercase mb-0.5"
                                    >SHA Verification</span
                                >
                                {#if summary.allShaPass}
                                    <span
                                        class="inline-flex items-center space-x-1 text-xs font-semibold text-green-400"
                                    >
                                        <CheckCircle2 size={13} />
                                        <span>100% Lossless</span>
                                    </span>
                                {:else}
                                    <span
                                        class="inline-flex items-center space-x-1 text-xs font-semibold text-red-400"
                                    >
                                        <XCircle size={13} />
                                        <span>Mismatch</span>
                                    </span>
                                {/if}
                            </div>
                        </div>

                        <!-- Streaming Table -->
                        <div
                            class="fluent-panel flex-1 min-h-0 overflow-y-auto relative bg-black/40"
                        >
                            <table
                                class="w-full text-left text-xs border-collapse"
                            >
                                <thead
                                    class="sticky top-0 bg-[#09090b] text-zinc-400 font-medium border-b border-white/5 z-10 font-sans"
                                >
                                    <tr>
                                        <th class="py-2.5 px-3">Filename</th>
                                        <th class="py-2.5 px-3 text-right"
                                            >Orig Size</th
                                        >
                                        <th class="py-2.5 px-3 text-right"
                                            >Ratio</th
                                        >
                                        <th class="py-2.5 px-3 text-right"
                                            >C-Wall</th
                                        >
                                        <th class="py-2.5 px-3 text-right"
                                            >C-GPU</th
                                        >
                                        <th class="py-2.5 px-3 text-right"
                                            >D-Wall</th
                                        >
                                        <th class="py-2.5 px-3 text-right"
                                            >D-GPU</th
                                        >
                                        <th class="py-2.5 px-3 text-center"
                                            >SHA-256</th
                                        >
                                    </tr>
                                </thead>
                                <tbody
                                    class="divide-y divide-white/5 font-mono text-[11px]"
                                >
                                    {#each appState.benchmarkRows as r (r.filename)}
                                        <tr
                                            transition:fade={{ duration: 120 }}
                                            class="hover:bg-white/5 text-zinc-300"
                                        >
                                            <td
                                                class="py-2 px-3 font-semibold text-zinc-100 truncate max-w-xs"
                                                >{r.filename}</td
                                            >
                                            <td
                                                class="py-2 px-3 text-right text-zinc-400"
                                                >{formatBytes(r.orig_bytes)}</td
                                            >
                                            <td
                                                class="py-2 px-3 text-right font-bold text-indigo-400"
                                                >{r.ratio.toFixed(2)}x</td
                                            >
                                            <td
                                                class="py-2 px-3 text-right text-zinc-300"
                                                >{r.c_wall_mbs.toFixed(1)} MB/s</td
                                            >
                                            <td
                                                class="py-2 px-3 text-right text-zinc-400"
                                                >{r.c_gpu_mbs.toFixed(1)} MB/s</td
                                            >
                                            <td
                                                class="py-2 px-3 text-right text-zinc-300"
                                                >{r.d_wall_mbs.toFixed(1)} MB/s</td
                                            >
                                            <td
                                                class="py-2 px-3 text-right text-zinc-400"
                                                >{r.d_gpu_mbs.toFixed(1)} MB/s</td
                                            >
                                            <td class="py-2 px-3 text-center">
                                                {#if r.sha_passed}
                                                    <span
                                                        class="inline-flex items-center space-x-1 text-green-400 font-semibold"
                                                        title={r.orig_sha}
                                                    >
                                                        <CheckCircle2
                                                            size={13}
                                                        />
                                                        <span
                                                            class="text-[10px]"
                                                            >PASS</span
                                                        >
                                                    </span>
                                                {:else}
                                                    <span
                                                        class="inline-flex items-center space-x-1 text-red-400 font-semibold"
                                                        title="SHA Mismatch!"
                                                    >
                                                        <XCircle size={13} />
                                                        <span
                                                            class="text-[10px]"
                                                            >FAIL</span
                                                        >
                                                    </span>
                                                {/if}
                                            </td>
                                        </tr>
                                    {/each}
                                </tbody>
                            </table>

                            {#if isRunning}
                                <div
                                    class="p-4 flex items-center justify-center space-x-2 text-zinc-400 text-xs font-sans"
                                >
                                    <Loader2
                                        size={16}
                                        class="animate-spin text-indigo-400"
                                    />
                                    <span>Benchmarking dataset files...</span>
                                </div>
                            {/if}
                        </div>
                    </div>
                {/if}
            </div>

            <!-- Action Footer -->
            <div
                class="flex items-center justify-between pt-2 border-t border-white/10 shrink-0 font-sans"
            >
                {#if !isRunning && appState.benchmarkRows.length > 0}
                    <div class="flex items-center space-x-2">
                        <button
                            onclick={copyMarkdownReport}
                            class="px-3.5 py-2 rounded-xl bg-white/10 hover:bg-white/15 border border-white/10 text-xs font-semibold text-white flex items-center space-x-1.5 transition-all cursor-pointer"
                        >
                            {#if copiedFeedback}
                                <Check
                                    size={14}
                                    class="text-green-400 shrink-0"
                                />
                                <span class="text-green-400"
                                    >Report Copied!</span
                                >
                            {:else}
                                <Copy size={14} class="shrink-0" />
                                <span>Copy Markdown Report</span>
                            {/if}
                        </button>
                    </div>

                    <div class="flex items-center space-x-2">
                        <button
                            onclick={resetSuite}
                            class="px-4 py-2 rounded-xl bg-white/5 hover:bg-white/10 text-xs font-medium text-zinc-300 cursor-pointer"
                        >
                            New Benchmark
                        </button>
                        <button
                            onclick={closeModal}
                            class="px-5 py-2 rounded-xl bg-indigo-600 hover:bg-indigo-500 text-xs font-semibold text-white shadow-md cursor-pointer"
                        >
                            Done
                        </button>
                    </div>
                {:else if !isRunning}
                    <div></div>
                    <button
                        disabled={benchTargets.length === 0}
                        onclick={startRun}
                        class="px-6 py-2.5 rounded-xl bg-indigo-600 hover:bg-indigo-500 disabled:opacity-40 disabled:hover:bg-indigo-600 text-xs font-semibold text-white flex items-center space-x-2 shadow-lg shadow-indigo-600/30 transition-all active:scale-95 cursor-pointer"
                    >
                        <Play size={14} />
                        <span>Start Benchmark Run</span>
                    </button>
                {/if}
            </div>
        </div>
    </div>
{/if}
