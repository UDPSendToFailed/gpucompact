/*
 * GPUCompact
 * Copyright (C) 2026 UDPSendToFailed
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
import type {
  ArchiveInspectResult,
  BenchmarkRowPayload,
  CollisionMode,
  CollisionScanResult,
  CorruptionPayload,
  CustomLaunchConfig,
  ProfilePreset,
  ProgressPayload
} from '../types';

export interface ConfirmModalOptions {
  title: string;
  message: string;
  confirmLabel?: string;
  cancelLabel?: string;
  variant?: 'danger' | 'warning' | 'info';
  isWarningOnly?: boolean;
  onConfirm?: () => void;
}

export class AppState {
  currentView = $state<'drop' | 'archive'>('drop');
  archivePath = $state<string | null>(null);
  archiveData = $state<ArchiveInspectResult | null>(null);

  dropTargets = $state<string[]>([]);
  selectedProfile = $state<ProfilePreset>('balanced');
  customConfig = $state<CustomLaunchConfig>({
    macro_mb: 4,
    mini_size: 1024,
    l_state: 2048,
    threads_comp: 6,
    threads_decomp: 56,
  });

  isOperating = $state<boolean>(false);
  operatingTitle = $state<string>('');
  progress = $state<ProgressPayload | null>(null);

  showCollisionModal = $state<boolean>(false);
  collisionScan = $state<CollisionScanResult | null>(null);
  collisionCallback = $state<((mode: CollisionMode | 'cancel') => void) | null>(null);

  showCorruptionModal = $state<boolean>(false);
  corruptionPayload = $state<CorruptionPayload | null>(null);

  // Native Benchmark Suite State
  showBenchmarkModal = $state<boolean>(false);
  benchmarkRows = $state<BenchmarkRowPayload[]>([]);

  confirmModal = $state<{
    show: boolean;
    title: string;
    message: string;
    confirmLabel: string;
    cancelLabel: string;
    variant: 'danger' | 'warning' | 'info';
    isWarningOnly: boolean;
    onConfirm: (() => void) | null;
  }>({
    show: false,
    title: '',
    message: '',
    confirmLabel: 'Confirm',
    cancelLabel: 'Cancel',
    variant: 'info',
    isWarningOnly: false,
    onConfirm: null,
  });

  alertModal = $state<{
    show: boolean;
    title: string;
    message: string;
    variant: 'error' | 'info';
  }>({
    show: false,
    title: '',
    message: '',
    variant: 'error',
  });

  selectedFilePaths = $state<Set<string>>(new Set());
  searchQuery = $state<string>('');

  showConfirm(options: ConfirmModalOptions) {
    this.confirmModal = {
      show: true,
      title: options.title,
      message: options.message,
      confirmLabel: options.confirmLabel || 'Confirm',
      cancelLabel: options.cancelLabel || 'Cancel',
      variant: options.variant || 'info',
      isWarningOnly: options.isWarningOnly || false,
      onConfirm: options.onConfirm || null,
    };
  }

  closeConfirm() {
    this.confirmModal = { ...this.confirmModal, show: false };
  }

  showAlert(title: string, message: string, variant: 'error' | 'info' = 'error') {
    this.alertModal = {
      show: true,
      title,
      message,
      variant,
    };
  }

  closeAlert() {
    this.alertModal = { ...this.alertModal, show: false };
  }

  selectPath(path: string) {
    const next = new Set(this.selectedFilePaths);
    next.add(path);
    this.selectedFilePaths = next;
  }

  deselectPath(path: string) {
    const next = new Set(this.selectedFilePaths);
    next.delete(path);
    this.selectedFilePaths = next;
  }

  togglePath(path: string) {
    const next = new Set(this.selectedFilePaths);
    if (next.has(path)) {
      next.delete(path);
    } else {
      next.add(path);
    }
    this.selectedFilePaths = next;
  }

  setSelection(paths: Iterable<string>) {
    this.selectedFilePaths = new Set(paths);
  }

  addPaths(paths: Iterable<string>) {
    const next = new Set(this.selectedFilePaths);
    for (const p of paths) next.add(p);
    this.selectedFilePaths = next;
  }

  removePaths(paths: Iterable<string>) {
    const next = new Set(this.selectedFilePaths);
    for (const p of paths) next.delete(p);
    this.selectedFilePaths = next;
  }

  clearSelection() {
    this.selectedFilePaths = new Set();
  }

  resetDrop() {
    this.dropTargets = [];
    this.currentView = 'drop';
    this.archivePath = null;
    this.archiveData = null;
    this.clearSelection();
    this.searchQuery = '';
  }

  setArchive(path: string, data: ArchiveInspectResult) {
    this.archivePath = path;
    this.archiveData = data;
    this.currentView = 'archive';
    this.clearSelection();
    this.searchQuery = '';
  }
}

export const appState = new AppState();