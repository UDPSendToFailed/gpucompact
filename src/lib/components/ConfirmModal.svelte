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
    import { AlertTriangle, Trash2, Info } from "lucide-svelte";

    function handleConfirm() {
        const cb = appState.confirmModal.onConfirm;
        appState.closeConfirm();
        if (cb) cb();
    }

    function handleCancel() {
        appState.closeConfirm();
    }
</script>

{#if appState.confirmModal.show}
    <div
        transition:fade={{ duration: 180 }}
        class="fixed inset-0 bg-black/60 backdrop-blur-md flex items-center justify-center z-50 p-6 select-none modal-backdrop-anim"
    >
        <div
            transition:scale={{ duration: 200, start: 0.95 }}
            class="glass-modal w-full max-w-sm p-6 space-y-5 text-center modal-card-anim border border-white/10"
        >
            {#if appState.confirmModal.variant === "danger"}
                <div
                    class="w-12 h-12 rounded-2xl bg-red-500/20 border border-red-500/30 flex items-center justify-center text-red-400 mx-auto shrink-0"
                >
                    <Trash2 size={24} />
                </div>
            {:else if appState.confirmModal.variant === "warning"}
                <div
                    class="w-12 h-12 rounded-2xl bg-amber-500/20 border border-amber-500/30 flex items-center justify-center text-amber-400 mx-auto shrink-0"
                >
                    <AlertTriangle size={24} />
                </div>
            {:else}
                <div
                    class="w-12 h-12 rounded-2xl bg-indigo-500/20 border border-indigo-500/30 flex items-center justify-center text-indigo-400 mx-auto shrink-0"
                >
                    <Info size={24} />
                </div>
            {/if}

            <div>
                <h3 class="text-sm font-semibold text-white">
                    {appState.confirmModal.title}
                </h3>
                <p class="text-xs text-zinc-400 mt-1.5 leading-relaxed">
                    {appState.confirmModal.message}
                </p>
            </div>

            <div class="space-y-2 pt-1">
                {#if appState.confirmModal.isWarningOnly}
                    <button
                        onclick={handleConfirm}
                        class="w-full px-4 py-2.5 rounded-xl bg-indigo-600 hover:bg-indigo-500 text-xs font-semibold text-white transition-all shadow-md active:scale-95 cursor-pointer"
                    >
                        {appState.confirmModal.confirmLabel || "Understand"}
                    </button>
                {:else}
                    <button
                        onclick={handleConfirm}
                        class="w-full px-4 py-2.5 rounded-xl text-xs font-semibold text-white flex items-center justify-center space-x-2 transition-all shadow-md active:scale-95 cursor-pointer {appState
                            .confirmModal.variant === 'danger'
                            ? 'bg-red-600 hover:bg-red-500 shadow-red-600/20'
                            : 'bg-indigo-600 hover:bg-indigo-500 shadow-indigo-600/20'}"
                    >
                        <span>{appState.confirmModal.confirmLabel}</span>
                    </button>

                    <button
                        onclick={handleCancel}
                        class="w-full px-4 py-2.5 rounded-xl bg-white/5 hover:bg-white/10 border border-white/5 text-xs font-medium text-zinc-400 hover:text-white transition-all active:scale-95 cursor-pointer"
                    >
                        {appState.confirmModal.cancelLabel || "Cancel"}
                    </button>
                {/if}
            </div>
        </div>
    </div>
{/if}
