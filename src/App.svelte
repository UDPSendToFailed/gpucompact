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
  import { onMount } from "svelte";
  import { fade } from "svelte/transition";
  import { listen } from "@tauri-apps/api/event";
  import { invoke } from "@tauri-apps/api/core";
  import { appState } from "./lib/stores/appState.svelte";
  import type {
    ProgressPayload,
    ArchiveInspectResult,
    CollisionScanResult,
    CorruptionPayload,
    BenchmarkRowPayload,
    CollisionMode,
  } from "./lib/types";
  import TitleBar from "./lib/components/TitleBar.svelte";
  import DropPage from "./lib/components/DropPage.svelte";
  import ArchiveTable from "./lib/components/ArchiveTable.svelte";
  import ProgressModal from "./lib/components/ProgressModal.svelte";
  import CollisionModal from "./lib/components/CollisionModal.svelte";
  import ConfirmModal from "./lib/components/ConfirmModal.svelte";
  import AlertModal from "./lib/components/AlertModal.svelte";
  import CorruptionModal from "./lib/components/CorruptionModal.svelte";
  import BenchmarkModal from "./lib/components/BenchmarkModal.svelte";

  async function processAppend(paths: string[], mode: CollisionMode) {
    if (!appState.archivePath) return;

    appState.isOperating = true;
    appState.operatingTitle = `Appending dropped items...`;
    appState.progress = null;

    try {
      await invoke("append_archive", {
        archivePath: appState.archivePath,
        inputs: paths,
        collisionMode: mode,
      });

      const data = await invoke<ArchiveInspectResult>("inspect_archive", {
        archivePath: appState.archivePath,
      });
      appState.setArchive(appState.archivePath, data);
    } catch (err: any) {
      appState.showAlert("Append Error", `Failed to append items: ${err}`);
    } finally {
      appState.isOperating = false;
    }
  }

  onMount(async () => {
    const handleContextMenu = (e: MouseEvent) => {
      e.preventDefault();
    };

    const handleKeyDown = (e: KeyboardEvent) => {
      if (
        e.key === "F5" ||
        (e.ctrlKey &&
          (e.key === "r" || e.key === "R" || e.key === "p" || e.key === "P"))
      ) {
        e.preventDefault();
      }
    };

    window.addEventListener("contextmenu", handleContextMenu);
    window.addEventListener("keydown", handleKeyDown);

    try {
      const initialFile = await invoke<string | null>("get_initial_cli_file");
      if (initialFile) {
        const data = await invoke<ArchiveInspectResult>("inspect_archive", {
          archivePath: initialFile,
        });
        appState.setArchive(initialFile, data);
      }
    } catch (err) {
      console.error("Initial CLI file inspect failed:", err);
    }

    const unlistenProgress = await listen<ProgressPayload>(
      "progress",
      (event) => {
        appState.progress = event.payload;
      },
    );

    const unlistenCorruption = await listen<CorruptionPayload>(
      "corruption_detected",
      (event) => {
        appState.corruptionPayload = event.payload;
        appState.showCorruptionModal = true;
      },
    );

    const unlistenBenchmark = await listen<BenchmarkRowPayload>(
      "benchmark_row",
      (event) => {
        appState.benchmarkRows = [...appState.benchmarkRows, event.payload];
      },
    );

    const unlistenDrop = await listen<{ paths: string[] }>(
      "tauri://drag-drop",
      async (event) => {
        const paths = event.payload.paths;
        if (!paths || paths.length === 0) return;

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

        if (appState.currentView === "archive" && appState.archivePath) {
          try {
            const scan = await invoke<CollisionScanResult>(
              "check_append_collisions",
              {
                archivePath: appState.archivePath,
                inputs: paths,
              },
            );

            if (scan.colliding_paths.length > 0) {
              appState.collisionScan = scan;
              appState.showCollisionModal = true;
              appState.collisionCallback = (mode) => {
                if (mode !== "cancel") {
                  processAppend(paths, mode);
                }
              };
            } else {
              processAppend(paths, "replace");
            }
          } catch (err: any) {
            appState.showAlert(
              "Pre-flight Error",
              `Collision scan failed: ${err}`,
            );
          }
        } else {
          appState.dropTargets = Array.from(
            new Set([...appState.dropTargets, ...paths]),
          );
          appState.currentView = "drop";
        }
      },
    );

    return () => {
      window.removeEventListener("contextmenu", handleContextMenu);
      window.removeEventListener("keydown", handleKeyDown);
      unlistenProgress();
      unlistenCorruption();
      unlistenBenchmark();
      unlistenDrop();
    };
  });
</script>

<div
  class="h-screen w-screen bg-[#09090b] text-zinc-100 flex flex-col overflow-hidden font-sans selection:bg-indigo-500/30"
>
  <TitleBar />

  <main
    class="flex-1 min-h-0 relative overflow-hidden grid grid-cols-1 grid-rows-1"
  >
    {#if appState.currentView === "drop"}
      <div
        transition:fade={{ duration: 180 }}
        class="col-start-1 row-start-1 h-full w-full"
      >
        <DropPage />
      </div>
    {:else if appState.currentView === "archive"}
      <div
        transition:fade={{ duration: 180 }}
        class="col-start-1 row-start-1 h-full w-full"
      >
        <ArchiveTable />
      </div>
    {/if}
  </main>

  <ProgressModal />
  <CollisionModal />
  <ConfirmModal />
  <AlertModal />
  <CorruptionModal />
  <BenchmarkModal />
</div>
