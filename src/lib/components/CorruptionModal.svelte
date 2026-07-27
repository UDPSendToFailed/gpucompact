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
    import { invoke } from "@tauri-apps/api/core";
    import { appState } from "../stores/appState.svelte";
    import {
        AlertOctagon,
        ShieldAlert,
        SkipForward,
        X,
        Play,
    } from "lucide-svelte";

    function formatBytes(bytes: number): string {
        if (bytes === 0) return "0 B";
        const k = 1024;
        const sizes = ["B", "KB", "MB", "GB", "TB"];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + " " + sizes[i];
    }

    async function resolveChoice(choice: "continue" | "skip" | "cancel") {
        appState.showCorruptionModal = false;
        try {
            await invoke("resolve_corruption", { choice });
        } catch (err) {
            console.error("Failed to send corruption resolution signal:", err);
        }
    }
</script>

{#if appState.showCorruptionModal && appState.corruptionPayload}
    <div
        transition:fade={{ duration: 180 }}
        class="fixed inset-0 bg-black/70 backdrop-blur-md flex items-center justify-center z-50 p-6 select-none modal-backdrop-anim"
    >
        <div
            transition:scale={{ duration: 200, start: 0.95 }}
            class="glass-modal w-full max-w-md p-6 space-y-5 modal-card-anim border border-red-500/30 shadow-2xl shadow-red-950/40"
        >
            <!-- Header -->
            <div
                class="flex items-center space-x-3 border-b border-white/10 pb-4"
            >
                <div
                    class="w-10 h-10 rounded-2xl bg-red-500/20 border border-red-500/30 flex items-center justify-center text-red-400 shrink-0"
                >
                    <AlertOctagon size={22} />
                </div>
                <div>
                    <h3 class="text-sm font-semibold text-white">
                        Payload Corruption Detected
                    </h3>
                    <p class="text-xs text-red-400 font-mono mt-0.5">
                        GPU Hash Mismatch Error
                    </p>
                </div>
            </div>

            <!-- Affected File & Chunk Details -->
            <div
                class="fluent-panel p-3.5 space-y-2 bg-black/40 border-red-500/20 text-xs"
            >
                <div class="flex items-start space-x-2">
                    <ShieldAlert
                        size={15}
                        class="text-red-400 shrink-0 mt-0.5"
                    />
                    <div class="min-w-0 flex-1">
                        <span
                            class="block text-[10px] text-zinc-500 font-sans uppercase tracking-wider"
                            >Affected File Path</span
                        >
                        <span class="font-mono text-zinc-200 truncate block"
                            >{appState.corruptionPayload.file_path}</span
                        >
                    </div>
                </div>

                <div class="grid grid-cols-2 gap-2 pt-1 font-mono text-[11px]">
                    <div
                        class="bg-white/5 p-2 rounded-lg border border-white/5"
                    >
                        <span class="block text-[10px] text-zinc-500 font-sans"
                            >Damaged Block</span
                        >
                        <span class="text-amber-400 font-semibold"
                            >Chunk #{appState.corruptionPayload.chunk_index} of {appState
                                .corruptionPayload.total_chunks}</span
                        >
                    </div>

                    <div
                        class="bg-white/5 p-2 rounded-lg border border-white/5"
                    >
                        <span class="block text-[10px] text-zinc-500 font-sans"
                            >Block Size</span
                        >
                        <span class="text-zinc-200 font-semibold"
                            >{formatBytes(
                                appState.corruptionPayload.uncomp_bytes,
                            )}</span
                        >
                    </div>
                </div>
            </div>

            <p class="text-xs text-zinc-400 leading-relaxed font-sans">
                A bit-flip or payload corruption was detected in this block.
                Choose how to handle extraction:
            </p>

            <!-- Action Buttons -->
            <div class="space-y-2 pt-1 font-sans">
                <button
                    onclick={() => resolveChoice("continue")}
                    class="w-full px-4 py-2.5 rounded-xl bg-indigo-600 hover:bg-indigo-500 text-xs font-semibold text-white flex items-center justify-center space-x-2 transition-all shadow-md active:scale-95 cursor-pointer"
                >
                    <Play size={14} />
                    <span>Continue & Pad Block (Zero-Fill)</span>
                </button>

                <button
                    onclick={() => resolveChoice("skip")}
                    class="w-full px-4 py-2.5 rounded-xl bg-white/10 hover:bg-white/15 border border-white/10 text-xs font-medium text-amber-300 hover:text-amber-200 flex items-center justify-center space-x-2 transition-all active:scale-95 cursor-pointer"
                >
                    <SkipForward size={14} />
                    <span>Skip Extracting This File</span>
                </button>

                <button
                    onclick={() => resolveChoice("cancel")}
                    class="w-full px-4 py-2.5 rounded-xl bg-white/5 hover:bg-white/10 border border-white/5 text-xs font-medium text-zinc-400 hover:text-white flex items-center justify-center space-x-2 transition-all cursor-pointer"
                >
                    <X size={14} />
                    <span>Cancel Extraction Entirely</span>
                </button>
            </div>
        </div>
    </div>
{/if}
