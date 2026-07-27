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
  import { fade } from "svelte/transition";
  import { open } from "@tauri-apps/plugin-dialog";
  import { invoke } from "@tauri-apps/api/core";
  import { appState } from "../stores/appState.svelte";
  import type {
    ArchiveFileEntry,
    ArchiveInspectResult,
    CollisionMode,
  } from "../types";
  import {
    Plus,
    Download,
    FolderDown,
    X,
    Search,
    FileText,
    CheckSquare,
    Square,
    Folder,
    ChevronRight,
    LayoutList,
    FolderTree,
    Trash2,
    Copy,
    Check,
    ArrowUp,
    ArrowDown,
    ArrowUpDown,
    MinusSquare,
  } from "lucide-svelte";

  let viewMode = $state<"tree" | "flat">("tree");
  let currentFolderPath = $state<string>("");
  let lastClickedIndex = $state<number | null>(null);
  let contextMenu = $state<{
    x: number;
    y: number;
    item: FolderNode | null;
  } | null>(null);
  let copiedFeedback = $state<boolean>(false);

  type SortColumn = "name" | "uncompressed" | "compressed" | "ratio" | "type";
  let sortColumn = $state<SortColumn>("name");
  let sortDirection = $state<"asc" | "desc">("asc");

  let isPainting = $state<boolean>(false);
  let paintMode = $state<"select" | "deselect">("select");
  let dragStartPos = $state<{ x: number; y: number } | null>(null);

  // Virtual Windowing State for rendering 100k+ file archives smoothly without DOM bloat
  let scrollTop = $state<number>(0);
  let containerHeight = $state<number>(600);

  const ROW_HEIGHT = 36; // Exact row height in pixels

  function formatBytes(bytes: number): string {
    if (bytes === 0) return "0 B";
    const k = 1024;
    const sizes = ["B", "KB", "MB", "GB", "TB"];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + " " + sizes[i];
  }

  interface FolderNode {
    name: string;
    fullPath: string;
    isFolder: boolean;
    fileCount: number;
    uncompressedSize: number;
    compressedSize: number;
    ratio: number;
    fileEntry?: ArchiveFileEntry;
    containedFilePaths: string[];
  }

  function handleSort(col: SortColumn) {
    if (sortColumn === col) {
      sortDirection = sortDirection === "asc" ? "desc" : "asc";
    } else {
      sortColumn = col;
      sortDirection = col === "name" ? "asc" : "desc";
    }
  }

  function sortItems(items: FolderNode[]): FolderNode[] {
    const mult = sortDirection === "asc" ? 1 : -1;

    return [...items].sort((a, b) => {
      if (sortColumn === "name") {
        if (a.isFolder && !b.isFolder) return -1;
        if (!a.isFolder && b.isFolder) return 1;
        return mult * (a.name < b.name ? -1 : a.name > b.name ? 1 : 0);
      }
      if (sortColumn === "type") {
        if (a.isFolder && !b.isFolder) return -1 * mult;
        if (!a.isFolder && b.isFolder) return 1 * mult;
        return a.name < b.name ? -1 : a.name > b.name ? 1 : 0;
      }
      if (sortColumn === "uncompressed")
        return mult * (a.uncompressedSize - b.uncompressedSize);
      if (sortColumn === "compressed")
        return mult * (a.compressedSize - b.compressedSize);
      if (sortColumn === "ratio") return mult * (a.ratio - b.ratio);
      return 0;
    });
  }

  let currentItems = $derived.by(() => {
    if (!appState.archiveData) return [];
    const allFiles = appState.archiveData.files;

    if (viewMode === "flat") {
      let list = allFiles.map((f) => ({
        name: f.path,
        fullPath: f.path,
        isFolder: false,
        fileCount: 1,
        uncompressedSize: f.uncompressed_size,
        compressedSize: f.compressed_size,
        ratio: f.ratio,
        fileEntry: f,
        containedFilePaths: [f.path],
      }));

      if (appState.searchQuery.trim()) {
        const q = appState.searchQuery.toLowerCase();
        list = list.filter((item) => item.name.toLowerCase().includes(q));
      }
      return sortItems(list);
    }

    const cleanFolderPath = currentFolderPath
      .replace(/\\/g, "/")
      .replace(/^\/+|\/+$/g, "");
    const folderMap = new Map<string, FolderNode>();
    const directFiles: FolderNode[] = [];

    for (const f of allFiles) {
      const normPath = f.path.replace(/\\/g, "/").replace(/^\/+/, "");
      let relPath = normPath;
      if (cleanFolderPath) {
        if (!normPath.startsWith(cleanFolderPath + "/")) continue;
        relPath = normPath.slice(cleanFolderPath.length + 1);
      }

      const parts = relPath.split("/").filter(Boolean);
      if (parts.length === 0) continue;

      if (parts.length === 1) {
        if (
          !appState.searchQuery.trim() ||
          f.path.toLowerCase().includes(appState.searchQuery.toLowerCase())
        ) {
          directFiles.push({
            name: parts[0],
            fullPath: f.path,
            isFolder: false,
            fileCount: 1,
            uncompressedSize: f.uncompressed_size,
            compressedSize: f.compressed_size,
            ratio: f.ratio,
            fileEntry: f,
            containedFilePaths: [f.path],
          });
        }
      } else {
        const folderName = parts[0];
        const folderFullPath = cleanFolderPath
          ? `${cleanFolderPath}/${folderName}`
          : folderName;

        let folder = folderMap.get(folderName);
        if (!folder) {
          folder = {
            name: folderName,
            fullPath: folderFullPath,
            isFolder: true,
            fileCount: 0,
            uncompressedSize: 0,
            compressedSize: 0,
            ratio: 1.0,
            containedFilePaths: [],
          };
          folderMap.set(folderName, folder);
        }
        folder.fileCount++;
        folder.uncompressedSize += f.uncompressed_size;
        folder.compressedSize += f.compressed_size;
        folder.containedFilePaths.push(f.path);
      }
    }

    for (const folder of folderMap.values()) {
      folder.ratio =
        folder.compressedSize > 0
          ? folder.uncompressedSize / folder.compressedSize
          : 1.0;
    }

    const folderList = Array.from(folderMap.values());
    if (appState.searchQuery.trim()) {
      const q = appState.searchQuery.toLowerCase();
      const filteredFolders = folderList.filter((f) =>
        f.name.toLowerCase().includes(q),
      );
      return sortItems([...filteredFolders, ...directFiles]);
    }

    return sortItems([...folderList, ...directFiles]);
  });

  let virtualWindow = $derived.by(() => {
    const total = currentItems.length;
    if (total === 0) {
      return {
        startIndex: 0,
        endIndex: 0,
        paddingTop: 0,
        paddingBottom: 0,
        visibleItems: [] as { item: FolderNode; idx: number }[],
      };
    }

    const visibleCount = Math.ceil(containerHeight / ROW_HEIGHT);
    const buffer = 15; // 15 buffer items above & below for ultra-smooth scrolling

    const startIndex = Math.max(0, Math.floor(scrollTop / ROW_HEIGHT) - buffer);
    const endIndex = Math.min(total, startIndex + visibleCount + buffer * 2);

    const paddingTop = startIndex * ROW_HEIGHT;
    const paddingBottom = Math.max(0, (total - endIndex) * ROW_HEIGHT);

    const visibleItems: { item: FolderNode; idx: number }[] = [];
    for (let i = startIndex; i < endIndex; i++) {
      visibleItems.push({ item: currentItems[i], idx: i });
    }

    return { startIndex, endIndex, paddingTop, paddingBottom, visibleItems };
  });

  function handleTableScroll(e: Event) {
    const target = e.currentTarget as HTMLElement;
    scrollTop = target.scrollTop;
    containerHeight = target.clientHeight;
  }

  let breadcrumbs = $derived.by(() => {
    if (!currentFolderPath) return [];
    return currentFolderPath.replace(/\\/g, "/").split("/").filter(Boolean);
  });

  let visibleFilePaths = $derived.by(() => {
    const paths: string[] = [];
    for (const item of currentItems) {
      if (item.isFolder) {
        for (const p of item.containedFilePaths) {
          paths.push(p);
        }
      } else {
        paths.push(item.fullPath);
      }
    }
    return paths;
  });

  let headerSelectStatus = $derived.by(() => {
    if (visibleFilePaths.length === 0) return "none";
    let selectedCount = 0;
    for (const p of visibleFilePaths) {
      if (appState.selectedFilePaths.has(p)) {
        selectedCount++;
      }
    }
    if (selectedCount === visibleFilePaths.length) return "all";
    if (selectedCount > 0) return "some";
    return "none";
  });

  let selectedLabel = $derived.by(() => {
    const size = appState.selectedFilePaths.size;
    if (size === 0) return "";
    if (size === 1) {
      const first = Array.from(appState.selectedFilePaths)[0];
      const filename = first.split(/[/\\]/).pop() || first;
      return `"${filename}"`;
    }
    return `${size} Selected Items`;
  });

  function navigateBreadcrumb(index: number) {
    if (index < 0) {
      currentFolderPath = "";
    } else {
      currentFolderPath = breadcrumbs.slice(0, index + 1).join("/");
    }
    scrollTop = 0;
    if (scrollContainer) scrollContainer.scrollTop = 0;
  }

  function openFolder(folderPath: string) {
    currentFolderPath = folderPath;
    scrollTop = 0;
    if (scrollContainer) scrollContainer.scrollTop = 0;
  }

  function getFolderSelectionStatus(item: FolderNode): "none" | "all" | "some" {
    if (!item.isFolder || item.containedFilePaths.length === 0) return "none";
    let count = 0;
    for (const p of item.containedFilePaths) {
      if (appState.selectedFilePaths.has(p)) count++;
    }
    if (count === item.containedFilePaths.length) return "all";
    if (count > 0) return "some";
    return "none";
  }

  function toggleFolderSelection(item: FolderNode, event?: MouseEvent) {
    if (event) event.stopPropagation();
    const status = getFolderSelectionStatus(item);
    if (status === "all") {
      appState.removePaths(item.containedFilePaths);
    } else {
      appState.addPaths(item.containedFilePaths);
    }
  }

  function onRowMouseDown(item: FolderNode, index: number, event: MouseEvent) {
    if (event.button !== 0) return;
    const target = event.target as HTMLElement;
    if (target.closest("button") || target.closest("input")) return;

    dragStartPos = { x: event.clientX, y: event.clientY };
    isPainting = false;

    if (event.shiftKey && lastClickedIndex !== null) {
      const start = Math.min(lastClickedIndex, index);
      const end = Math.max(lastClickedIndex, index);

      const pathsToSelect: string[] = [];
      for (let i = start; i <= end; i++) {
        if (currentItems[i].isFolder) {
          for (const p of currentItems[i].containedFilePaths)
            pathsToSelect.push(p);
        } else {
          pathsToSelect.push(currentItems[i].fullPath);
        }
      }
      appState.addPaths(pathsToSelect);
    } else if (event.ctrlKey || event.metaKey) {
      if (item.isFolder) {
        toggleFolderSelection(item);
      } else {
        appState.togglePath(item.fullPath);
      }
    }

    lastClickedIndex = index;
  }

  function onRowMouseMove(item: FolderNode, event: MouseEvent) {
    if (event.buttons !== 1 || !dragStartPos) return;

    const dx = event.clientX - dragStartPos.x;
    const dy = event.clientY - dragStartPos.y;
    const dist = Math.sqrt(dx * dx + dy * dy);

    if (dist > 5) {
      if (!isPainting) {
        isPainting = true;
        paintMode = item.isFolder
          ? getFolderSelectionStatus(item) === "all"
            ? "deselect"
            : "select"
          : appState.selectedFilePaths.has(item.fullPath)
            ? "deselect"
            : "select";
        onRowMouseEnter(item);
      }
    }
  }

  function onRowMouseEnter(item: FolderNode) {
    if (!isPainting) return;
    if (item.isFolder) {
      if (paintMode === "select") appState.addPaths(item.containedFilePaths);
      else appState.removePaths(item.containedFilePaths);
    } else {
      if (paintMode === "select") appState.selectPath(item.fullPath);
      else appState.deselectPath(item.fullPath);
    }
  }

  function onWindowMouseUp() {
    isPainting = false;
    dragStartPos = null;
  }

  function toggleSelectAll() {
    if (visibleFilePaths.length === 0) return;
    if (headerSelectStatus === "all") {
      appState.removePaths(visibleFilePaths);
    } else {
      appState.addPaths(visibleFilePaths);
    }
  }

  async function addFilesToArchive() {
    if (!appState.archivePath || !appState.archiveData) return;

    const selected = await open({ multiple: true });
    if (!selected) return;
    const paths = Array.isArray(selected) ? selected : [selected];

    const existingSet = new Set(
      appState.archiveData.files.map((f) => f.path.replace(/\\/g, "/")),
    );
    const hasCollision = paths.some((p) => {
      const filename = p.split(/[/\\]/).pop() || p;
      return existingSet.has(filename);
    });

    if (hasCollision) {
      appState.showCollisionModal = true;
      appState.collisionCallback = (mode: CollisionMode) => {
        performAppend(paths, mode);
      };
    } else {
      performAppend(paths, "replace");
    }
  }

  async function performAppend(paths: string[], collisionMode: CollisionMode) {
    if (!appState.archivePath) return;

    appState.isOperating = true;
    appState.operatingTitle = `Adding ${paths.length} file(s)...`;
    appState.progress = null;

    try {
      await invoke("append_archive", {
        archivePath: appState.archivePath,
        inputs: paths,
        collisionMode,
      });

      const data = await invoke<ArchiveInspectResult>("inspect_archive", {
        archivePath: appState.archivePath,
      });
      appState.setArchive(appState.archivePath, data);
    } catch (err: any) {
      appState.showAlert("Append Error", `Failed to append file(s): ${err}`);
    } finally {
      appState.isOperating = false;
    }
  }

  async function performExtraction(
    outDir: string,
    files: string[],
    collisionMode: CollisionMode,
  ) {
    if (!appState.archivePath) return;

    appState.isOperating = true;
    appState.operatingTitle =
      files.length === 1
        ? `Extracting ${files[0]}...`
        : files.length > 1
          ? `Extracting ${files.length} item(s)...`
          : "Extracting All Items...";
    appState.progress = null;

    try {
      const extractTarget = files.length > 0 ? files.join("|") : null;
      await invoke("decompress_archive", {
        archivePath: appState.archivePath,
        outputDir: outDir,
        extractFile: extractTarget,
        collisionMode,
      });
    } catch (err: any) {
      appState.showAlert(
        "Extraction Error",
        `Failed to extract item(s): ${err}`,
      );
    } finally {
      appState.isOperating = false;
    }
  }

  async function extractFiles(files: string[]) {
    if (!appState.archivePath) return;

    const outDir = await open({ directory: true, multiple: false });
    if (!outDir || typeof outDir !== "string") return;

    const extractTarget = files.length > 0 ? files.join("|") : null;

    try {
      const scan = await invoke<CollisionScanResult>(
        "check_extract_collisions",
        {
          archivePath: appState.archivePath,
          outputDir: outDir,
          extractFile: extractTarget,
        },
      );

      if (scan && scan.colliding_paths && scan.colliding_paths.length > 0) {
        appState.collisionScan = scan;
        appState.showCollisionModal = true;
        appState.collisionCallback = (mode: CollisionMode | "cancel") => {
          if (mode !== "cancel") {
            performExtraction(outDir, files, mode);
          }
        };
      } else {
        performExtraction(outDir, files, "replace");
      }
    } catch (err: any) {
      performExtraction(outDir, files, "replace");
    }
  }

  async function deleteSelectedFiles() {
    if (
      !appState.archivePath ||
      !appState.archiveData ||
      appState.selectedFilePaths.size === 0
    )
      return;

    const totalFiles = appState.archiveData.files.length;
    const toDelete = Array.from(appState.selectedFilePaths);

    // Pre-validation guard: block deleting all files from archive
    if (toDelete.length >= totalFiles) {
      appState.showConfirm({
        title: "Cannot Delete All Files",
        message:
          "GPUCompact archives must contain at least one file. To remove all files, delete the archive file itself.",
        confirmLabel: "Understand",
        variant: "warning",
        isWarningOnly: true,
        onConfirm: () => {},
      });
      return;
    }

    const label = selectedLabel;
    appState.showConfirm({
      title: "Confirm File Deletion",
      message: `Are you sure you want to delete ${label} from this archive? This action cannot be undone.`,
      confirmLabel: "Delete Files",
      cancelLabel: "Cancel",
      variant: "danger",
      isWarningOnly: false,
      onConfirm: async () => {
        appState.isOperating = true;
        appState.operatingTitle = `Removing ${toDelete.length} item(s)...`;
        appState.progress = null;

        try {
          await invoke("remove_archive", {
            archivePath: appState.archivePath,
            filesToRemove: toDelete,
          });

          const data = await invoke<ArchiveInspectResult>("inspect_archive", {
            archivePath: appState.archivePath!,
          });
          appState.setArchive(appState.archivePath!, data);
        } catch (err: any) {
          appState.showAlert(
            "Deletion Error",
            `Failed to remove item(s): ${err}`,
          );
        } finally {
          appState.isOperating = false;
        }
      },
    });
  }

  function handleContextMenu(e: MouseEvent, item: FolderNode | null) {
    e.preventDefault();
    if (item) {
      if (item.isFolder) {
        appState.addPaths(item.containedFilePaths);
      } else if (!appState.selectedFilePaths.has(item.fullPath)) {
        appState.clearSelection();
        appState.selectPath(item.fullPath);
      }
    }
    contextMenu = { x: e.clientX, y: e.clientY, item };
  }

  function closeContextMenu() {
    contextMenu = null;
  }

  function copySelectedPaths() {
    const paths = Array.from(appState.selectedFilePaths).join("\n");
    navigator.clipboard.writeText(paths);
    copiedFeedback = true;
    setTimeout(() => (copiedFeedback = false), 1500);
    closeContextMenu();
  }
</script>

<svelte:window onclick={closeContextMenu} onmouseup={onWindowMouseUp} />

{#if appState.archiveData}
  <div
    class="h-full w-full flex flex-col p-5 space-y-4 overflow-hidden relative select-none"
  >
    <!-- Header -->
    <div
      class="fluent-panel px-4 py-3 flex items-center justify-between gap-4 shrink-0 overflow-x-auto"
    >
      <div class="flex items-center space-x-4 min-w-0">
        <div class="min-w-0">
          <h2
            class="text-sm font-semibold text-white font-mono truncate max-w-xs"
          >
            {appState.archiveData.metadata.archive_name}
          </h2>
          <div
            class="flex items-center space-x-2.5 text-[11px] text-zinc-400 mt-0.5 whitespace-nowrap"
          >
            <span
              >Files: <strong class="text-zinc-200"
                >{appState.archiveData.metadata.num_files}</strong
              ></span
            >
            <span class="text-zinc-600">•</span>
            <span
              >Ratio: <strong class="text-indigo-400 font-bold"
                >{appState.archiveData.metadata.overall_ratio.toFixed(
                  2,
                )}x</strong
              ></span
            >
            <span class="text-zinc-600">•</span>
            <span
              >Size: <strong class="text-zinc-200"
                >{formatBytes(
                  appState.archiveData.metadata.total_compressed_bytes,
                )}</strong
              >
              / {formatBytes(
                appState.archiveData.metadata.total_uncompressed_bytes,
              )}</span
            >
            <span class="text-zinc-600">•</span>
            <div
              class="inline-flex items-center space-x-1 font-mono text-[10px]"
            >
              <span
                class="bg-zinc-800/80 text-zinc-300 px-1.5 py-0.5 rounded border border-white/5 whitespace-nowrap"
                >Macro={appState.archiveData.metadata.macro_mb}MB</span
              >
              <span
                class="bg-zinc-800/80 text-zinc-300 px-1.5 py-0.5 rounded border border-white/5 whitespace-nowrap"
                >Mini={appState.archiveData.metadata.mini_size}B</span
              >
              <span
                class="bg-zinc-800/80 text-zinc-300 px-1.5 py-0.5 rounded border border-white/5 whitespace-nowrap"
                >L={appState.archiveData.metadata.L_state}</span
              >
              <span
                class="bg-zinc-800/80 text-zinc-400 px-1.5 py-0.5 rounded border border-white/5 whitespace-nowrap"
                >Chunks={appState.archiveData.metadata.num_chunks}</span
              >
            </div>
          </div>
        </div>
      </div>

      <!-- Action Buttons -->
      <div class="flex items-center space-x-2 shrink-0">
        <button
          onclick={addFilesToArchive}
          class="px-3.5 py-1.5 rounded-xl bg-white/5 hover:bg-white/10 border border-white/10 text-xs font-medium text-white flex items-center space-x-1.5 transition-all whitespace-nowrap active:scale-95 cursor-pointer"
        >
          <Plus size={14} class="text-indigo-400 shrink-0" />
          <span>Add Files...</span>
        </button>

        {#if appState.selectedFilePaths.size > 0}
          <button
            onclick={() => extractFiles(Array.from(appState.selectedFilePaths))}
            class="px-3.5 py-1.5 rounded-xl bg-indigo-600 hover:bg-indigo-500 text-xs font-semibold text-white flex items-center space-x-1.5 shadow-md shadow-indigo-600/20 transition-all whitespace-nowrap active:scale-95 cursor-pointer"
          >
            <Download size={14} class="shrink-0" />
            <span>Extract Selected ({appState.selectedFilePaths.size})</span>
          </button>
        {/if}

        <button
          onclick={() => extractFiles([])}
          class="px-3.5 py-1.5 rounded-xl bg-indigo-600 hover:bg-indigo-500 text-xs font-semibold text-white flex items-center space-x-1.5 shadow-md shadow-indigo-600/30 transition-all whitespace-nowrap active:scale-95 cursor-pointer"
        >
          <FolderDown size={14} class="shrink-0" />
          <span>Extract All</span>
        </button>

        <button
          onclick={() => appState.resetDrop()}
          class="p-1.5 rounded-xl hover:bg-white/10 text-zinc-400 hover:text-white transition-all ml-1 cursor-pointer shrink-0"
          title="Close Archive"
        >
          <X size={16} />
        </button>
      </div>
    </div>

    <!-- Search & View Mode Toolbar -->
    <div class="flex items-center justify-between px-1 shrink-0">
      <div class="flex items-center space-x-3">
        <div
          class="flex items-center bg-black/40 p-0.5 rounded-lg border border-white/10 text-xs"
        >
          <button
            onclick={() => (viewMode = "tree")}
            class="px-2.5 py-1 rounded flex items-center space-x-1.5 transition-all cursor-pointer {viewMode ===
            'tree'
              ? 'bg-indigo-500/20 text-indigo-400 font-semibold'
              : 'text-zinc-400 hover:text-white'}"
            title="Tree Directory View"
          >
            <FolderTree size={13} />
            <span>Tree</span>
          </button>

          <button
            onclick={() => (viewMode = "flat")}
            class="px-2.5 py-1 rounded flex items-center space-x-1.5 transition-all cursor-pointer {viewMode ===
            'flat'
              ? 'bg-indigo-500/20 text-indigo-400 font-semibold'
              : 'text-zinc-400 hover:text-white'}"
            title="Flat List View"
          >
            <LayoutList size={13} />
            <span>Flat</span>
          </button>
        </div>

        {#if viewMode === "tree"}
          <div
            class="flex items-center space-x-1 font-mono text-xs text-zinc-400 bg-white/5 px-2.5 py-1 rounded-lg border border-white/5 whitespace-nowrap"
          >
            <button
              onclick={() => navigateBreadcrumb(-1)}
              class="hover:text-indigo-400 transition-colors flex items-center space-x-1 cursor-pointer"
            >
              <Folder size={13} class="text-indigo-400" />
              <span>root</span>
            </button>

            {#each breadcrumbs as crumb, idx}
              <ChevronRight size={12} class="text-zinc-600 shrink-0" />
              <button
                onclick={() => navigateBreadcrumb(idx)}
                class="hover:text-indigo-400 transition-colors cursor-pointer {idx ===
                breadcrumbs.length - 1
                  ? 'text-zinc-200 font-semibold'
                  : ''}"
              >
                {crumb}
              </button>
            {/each}
          </div>
        {/if}
      </div>

      <div class="flex items-center space-x-4">
        <div class="relative w-64">
          <Search size={14} class="absolute left-3 top-2.5 text-zinc-500" />
          <input
            type="text"
            bind:value={appState.searchQuery}
            placeholder="Filter items..."
            class="input-standard w-full pl-9 pr-3 py-1.5"
          />
        </div>

        <div class="text-xs text-zinc-400 whitespace-nowrap">
          <span
            >Selected: <strong class="text-white"
              >{appState.selectedFilePaths.size}</strong
            >
            of {appState.archiveData.files.length}</span
          >
        </div>
      </div>
    </div>

    <!-- Main Scrollable File Table -->
    <div
      class="fluent-panel flex-1 min-h-0 overflow-hidden grid grid-cols-1 grid-rows-1 relative"
    >
      {#key `${currentFolderPath}_${viewMode}`}
        <div
          transition:fade={{ duration: 140 }}
          onscroll={handleTableScroll}
          bind:this={scrollContainer}
          class="col-start-1 row-start-1 w-full h-full overflow-y-auto relative"
        >
          <table class="w-full text-left text-xs border-collapse">
            <thead
              class="sticky top-0 bg-[#09090b]/95 backdrop-blur-md z-20 text-zinc-400 font-medium border-b border-white/5 select-none"
            >
              <tr>
                <th class="py-2.5 px-4 w-10 text-center">
                  <button
                    onclick={toggleSelectAll}
                    class="text-zinc-500 hover:text-zinc-300 cursor-pointer"
                  >
                    {#if headerSelectStatus === "all"}
                      <CheckSquare size={14} class="text-indigo-400" />
                    {:else if headerSelectStatus === "some"}
                      <MinusSquare size={14} class="text-indigo-400/80" />
                    {:else}
                      <Square size={14} />
                    {/if}
                  </button>
                </th>

                <th
                  class="py-2.5 px-3 cursor-pointer hover:text-white transition-colors"
                  onclick={() => handleSort("name")}
                >
                  <div class="flex items-center space-x-1">
                    <span>Name / Path</span>
                    {#if sortColumn === "name"}
                      {#if sortDirection === "asc"}<ArrowUp
                          size={12}
                          class="text-indigo-400"
                        />{:else}<ArrowDown
                          size={12}
                          class="text-indigo-400"
                        />{/if}
                    {:else}
                      <ArrowUpDown size={11} class="text-zinc-600" />
                    {/if}
                  </div>
                </th>

                <th
                  class="py-2.5 px-3 text-right cursor-pointer hover:text-white transition-colors"
                  onclick={() => handleSort("uncompressed")}
                >
                  <div class="flex items-center justify-end space-x-1">
                    <span>Uncompressed</span>
                    {#if sortColumn === "uncompressed"}
                      {#if sortDirection === "asc"}<ArrowUp
                          size={12}
                          class="text-indigo-400"
                        />{:else}<ArrowDown
                          size={12}
                          class="text-indigo-400"
                        />{/if}
                    {:else}
                      <ArrowUpDown size={11} class="text-zinc-600" />
                    {/if}
                  </div>
                </th>

                <th
                  class="py-2.5 px-3 text-right cursor-pointer hover:text-white transition-colors"
                  onclick={() => handleSort("compressed")}
                >
                  <div class="flex items-center justify-end space-x-1">
                    <span>Compressed</span>
                    {#if sortColumn === "compressed"}
                      {#if sortDirection === "asc"}<ArrowUp
                          size={12}
                          class="text-indigo-400"
                        />{:else}<ArrowDown
                          size={12}
                          class="text-indigo-400"
                        />{/if}
                    {:else}
                      <ArrowUpDown size={11} class="text-zinc-600" />
                    {/if}
                  </div>
                </th>

                <th
                  class="py-2.5 px-3 text-right cursor-pointer hover:text-white transition-colors"
                  onclick={() => handleSort("ratio")}
                >
                  <div class="flex items-center justify-end space-x-1">
                    <span>Ratio</span>
                    {#if sortColumn === "ratio"}
                      {#if sortDirection === "asc"}<ArrowUp
                          size={12}
                          class="text-indigo-400"
                        />{:else}<ArrowDown
                          size={12}
                          class="text-indigo-400"
                        />{/if}
                    {:else}
                      <ArrowUpDown size={11} class="text-zinc-600" />
                    {/if}
                  </div>
                </th>

                <th
                  class="py-2.5 px-3 text-center cursor-pointer hover:text-white transition-colors"
                  onclick={() => handleSort("type")}
                >
                  <div class="flex items-center justify-center space-x-1">
                    <span>Type</span>
                    {#if sortColumn === "type"}
                      {#if sortDirection === "asc"}<ArrowUp
                          size={12}
                          class="text-indigo-400"
                        />{:else}<ArrowDown
                          size={12}
                          class="text-indigo-400"
                        />{/if}
                    {:else}
                      <ArrowUpDown size={11} class="text-zinc-600" />
                    {/if}
                  </div>
                </th>
              </tr>
            </thead>
            <tbody class="divide-y divide-white/5 font-mono">
              {#if virtualWindow.paddingTop > 0}
                <tr style="height: {virtualWindow.paddingTop}px;" class="border-0 p-0 pointer-events-none">
                  <td colspan="6" class="p-0 border-0"></td>
                </tr>
              {/if}

              {#each virtualWindow.visibleItems as { item, idx } (item.fullPath)}
                {@const folderStatus = getFolderSelectionStatus(item)}
                {@const isFileSelected =
                  !item.isFolder &&
                  appState.selectedFilePaths.has(item.fullPath)}
                {@const isRowSelected = item.isFolder
                  ? folderStatus !== "none"
                  : isFileSelected}

                <tr
                  onmousedown={(e) => onRowMouseDown(item, idx, e)}
                  onmousemove={(e) => onRowMouseMove(item, e)}
                  onmouseenter={() => onRowMouseEnter(item)}
                  ondblclick={() =>
                    item.isFolder
                      ? openFolder(item.fullPath)
                      : extractFiles([item.fullPath])}
                  oncontextmenu={(e) => handleContextMenu(e, item)}
                  class="cursor-pointer transition-colors hover:bg-white/5 {isRowSelected
                    ? 'bg-indigo-500/20 text-white font-medium border-l-2 border-l-indigo-500'
                    : 'text-zinc-300'}"
                >
                  <td class="py-2 px-4 text-center">
                    {#if item.isFolder}
                      <button
                        onclick={(e) => toggleFolderSelection(item, e)}
                        class="text-zinc-500 hover:text-zinc-200 transition-colors cursor-pointer"
                        title="Toggle folder selection"
                      >
                        {#if folderStatus === "all"}
                          <CheckSquare size={14} class="text-indigo-400" />
                        {:else if folderStatus === "some"}
                          <MinusSquare size={14} class="text-indigo-400/80" />
                        {:else}
                          <Square size={14} />
                        {/if}
                      </button>
                    {:else}
                      <input
                        type="checkbox"
                        checked={isFileSelected}
                        onchange={() => appState.togglePath(item.fullPath)}
                        class="rounded bg-zinc-800 border-zinc-700 text-indigo-500 focus:ring-0 cursor-pointer"
                      />
                    {/if}
                  </td>
                  <td
                    class="py-2 px-3 truncate max-w-md flex items-center space-x-2.5"
                  >
                    {#if item.isFolder}
                      <Folder size={14} class="text-indigo-400 shrink-0" />
                      <span class="font-semibold text-zinc-100"
                        >{item.name} /</span
                      >
                    {:else}
                      <FileText size={13} class="text-zinc-500 shrink-0" />
                      <span class="truncate">{item.name}</span>
                    {/if}
                  </td>
                  <td class="py-2 px-3 text-right text-zinc-400"
                    >{formatBytes(item.uncompressedSize)}</td
                  >
                  <td class="py-2 px-3 text-right text-zinc-400"
                    >{formatBytes(item.compressedSize)}</td
                  >
                  <td
                    class="py-2 px-3 text-right font-semibold {item.ratio >= 1.0
                      ? 'text-indigo-400'
                      : 'text-amber-400'}"
                  >
                    {item.ratio.toFixed(2)}x
                  </td>
                  <td class="py-2 px-3 text-center text-zinc-500">
                    {#if item.isFolder}
                      <span
                        class="text-[10px] bg-indigo-500/10 text-indigo-400 px-1.5 py-0.5 rounded border border-indigo-500/20 font-sans"
                        >Folder ({item.fileCount})</span
                      >
                    {:else}
                      <span class="text-[10px] text-zinc-500 font-sans"
                        >File</span
                      >
                    {/if}
                  </td>
                </tr>
              {/each}

              {#if virtualWindow.paddingBottom > 0}
                <tr style="height: {virtualWindow.paddingBottom}px;" class="border-0 p-0 pointer-events-none">
                  <td colspan="6" class="p-0 border-0"></td>
                </tr>
              {/if}
            </tbody>
          </table>
        </div>
      {/key}
    </div>
  </div>

  <!-- Context Menu -->
  {#if contextMenu}
    <div
      transition:fade={{ duration: 120 }}
      style="left: {contextMenu.x}px; top: {contextMenu.y}px;"
      class="fixed glass-modal py-1.5 w-60 z-50 shadow-2xl text-xs font-sans border border-white/10 modal-card-anim"
    >
      <button
        onclick={() => extractFiles(Array.from(appState.selectedFilePaths))}
        class="w-full px-3.5 py-2 text-left text-zinc-200 hover:bg-indigo-600 hover:text-white flex items-center space-x-2 cursor-pointer truncate"
      >
        <Download size={13} class="shrink-0" />
        <span class="truncate"
          >Extract {selectedLabel || "Selected Item"}...</span
        >
      </button>

      <button
        onclick={() => extractFiles([])}
        class="w-full px-3.5 py-2 text-left text-zinc-200 hover:bg-indigo-600 hover:text-white flex items-center space-x-2 cursor-pointer"
      >
        <FolderDown size={13} class="shrink-0" />
        <span>Extract All Items...</span>
      </button>

      <button
        onclick={copySelectedPaths}
        class="w-full px-3.5 py-2 text-left text-zinc-200 hover:bg-indigo-600 hover:text-white flex items-center space-x-2 cursor-pointer"
      >
        {#if copiedFeedback}
          <Check size={13} class="text-green-400 shrink-0" />
          <span class="text-green-400 font-semibold">Paths Copied!</span>
        {:else}
          <Copy size={13} class="shrink-0" />
          <span>Copy Selected Path(s)</span>
        {/if}
      </button>

      <div class="my-1 border-t border-white/10"></div>

      <button
        onclick={deleteSelectedFiles}
        class="w-full px-3.5 py-2 text-left text-red-400 hover:bg-red-500/20 flex items-center space-x-2 cursor-pointer truncate"
      >
        <Trash2 size={13} class="shrink-0" />
        <span class="truncate"
          >Delete {selectedLabel || "Selected Item"}...</span
        >
      </button>
    </div>
  {/if}
{/if}
