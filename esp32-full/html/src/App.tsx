/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { motion } from 'motion/react';
import { Activity, Power, ChevronUp, ChevronDown, Wrench } from 'lucide-react';
import { Gauge } from './components/Gauge';
import { HorizontalGauge } from './components/HorizontalGauge';
import { ControlButton } from './components/ControlButton';
import { LevelControl } from './components/LevelControl';
import { ImpalaSSLogo } from './components/ImpalaSSLogo';
import { airService } from './services/airService';
import { SystemState, Corner, LeakStatus, TankMaintStatus } from './types';

function formatElapsed(seconds: number): string {
  if (seconds < 60) return '<1m';
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const mins = Math.floor((seconds % 3600) / 60);
  if (days > 0) return `${days}d ${hours}h`;
  if (hours > 0) return `${hours}h ${mins}m`;
  return `${mins}m`;
}

const LEAK_SENSOR_NAMES = ['FL', 'FR', 'RL', 'RR', 'Tank'];

export default function App() {
  const [state, setState] = useState<SystemState>({
    pressures: { FL: 0, FR: 0, RL: 0, RR: 0, tank: 0 },
    targets: { FL: 45, FR: 45, RL: 30, RR: 30 },
    compressorActive: false,
    connected: false,
    level: 0
  });

  const [showTankDetails, setShowTankDetails] = useState(false);
  const mainRef = useRef<HTMLElement>(null);

  // Long-press preset save state
  const [saveModal, setSaveModal] = useState<{ presetIndex: number; presetName: string; saved: boolean } | null>(null);
  const [tankMaintModalOpen, setTankMaintModalOpen] = useState(false);
  const longPressTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const saveTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const longPressTriggered = useRef(false);

  const applyPreset = useCallback((index: number) => {
    airService.applyPreset(index);
    mainRef.current?.scrollTo({ top: 0, behavior: 'smooth' });
  }, []);

  const clearLongPress = useCallback(() => {
    if (longPressTimer.current) { clearTimeout(longPressTimer.current); longPressTimer.current = null; }
    if (saveTimer.current) { clearTimeout(saveTimer.current); saveTimer.current = null; }
    setSaveModal(null);
  }, []);

  const handlePresetPressStart = useCallback((index: number, name: string) => {
    longPressTriggered.current = false;
    longPressTimer.current = setTimeout(() => {
      longPressTriggered.current = true;
      setSaveModal({ presetIndex: index, presetName: name, saved: false });
      // After another 2s of holding, actually save
      saveTimer.current = setTimeout(() => {
        const rounded = {
          FL: Math.round(state.pressures.FL),
          FR: Math.round(state.pressures.FR),
          RL: Math.round(state.pressures.RL),
          RR: Math.round(state.pressures.RR),
        };
        airService.savePreset(index, state.pressures);
        setPresetTargets(prev => {
          const updated = [...prev];
          updated[index] = rounded;
          return updated;
        });
        setSaveModal(prev => prev ? { ...prev, saved: true } : null);
        setTimeout(() => setSaveModal(null), 1200);
      }, 2000);
    }, 2000);
  }, [state.pressures]);

  const handlePresetPressEnd = useCallback((index: number) => {
    if (!longPressTriggered.current) {
      // Short tap — apply preset
      clearLongPress();
      applyPreset(index);
    } else if (saveModal && !saveModal.saved) {
      // Released before save completed
      clearLongPress();
    }
  }, [saveModal, clearLongPress, applyPreset]);

  // Leak monitor state
  const [leakStatus, setLeakStatus] = useState<LeakStatus>({ valid: false });

  // Sync browser time to ESP32 on connect
  useEffect(() => {
    airService.syncTime();
  }, []);

  // Poll leak status (every 5 seconds — leak data changes slowly)
  useEffect(() => {
    const fetchLeak = async () => {
      const status = await airService.getLeakStatus();
      setLeakStatus(status);
    };
    fetchLeak();
    const interval = setInterval(fetchLeak, 5000);
    return () => clearInterval(interval);
  }, []);

  // Poll for status (400ms matches ESP32 web server interval)
  useEffect(() => {
    const interval = setInterval(async () => {
      const status = await airService.getStatus();
      setState(prev => ({ ...prev, ...status }));
      // Sync preset targets from ESP32 (custom presets saved to EEPROM)
      if (status.presets) {
        setPresetTargets(status.presets);
      }
    }, 400);
    return () => clearInterval(interval);
  }, []);

  // Preset target PSI values (mutable — updated when user saves a preset)
  const [presetTargets, setPresetTargets] = useState([
    { FL: 0, FR: 0, RL: 0, RR: 0 },        // Lay
    { FL: 80, FR: 80, RL: 50, RR: 50 },      // Cruise
    { FL: 100, FR: 100, RL: 80, RR: 80 },    // Max
  ]);

  const isPresetActive = (presetIndex: number): boolean => {
    const targets = presetTargets[presetIndex];
    if (!targets) return false;
    const threshold = 2;
    return (['FL', 'FR', 'RL', 'RR'] as Corner[]).every(
      c => Math.abs(state.pressures[c] - targets[c]) <= threshold
    );
  };

  const handleControl = useCallback((corner: Corner | 'ALL', direction: 'up' | 'down', start: boolean) => {
    const type = direction === 'up' ? 'inflate' : 'deflate';

    if (corner === 'ALL') {
      ['FL', 'FR', 'RL', 'RR'].forEach(c => airService.setSolenoid(c, type, start));
    } else {
      airService.setSolenoid(corner, type, start);
    }
  }, []);

  return (
    <div className="h-screen h-dvh w-full max-w-6xl mx-auto flex flex-col bg-dark-900 relative overflow-hidden font-sans sm:rounded-3xl sm:my-4 lg:my-8 sm:h-[95dvh] lg:h-[90dvh] sm:border sm:border-white/[0.06]">
      {/* Main Content Area - Scroll Snapping Enabled */}
      <main ref={mainRef} className="flex-1 overflow-y-auto relative snap-y snap-mandatory scroll-smooth overscroll-none">
        {/* Unit 1: Gauges, Tank, and Master Controls */}
        <section className="snap-start min-h-dvh flex flex-col p-2 sm:p-4 gap-2 sm:gap-4">
          {/* Glass Panel for Gauges and Controls */}
          <div className="glass-card rounded-2xl sm:rounded-3xl p-2 sm:p-8 relative overflow-hidden flex-1 flex flex-col justify-center">
            <div className="grid grid-cols-2 lg:grid-cols-4 gap-x-1 sm:gap-x-4 lg:gap-x-8 gap-y-1 sm:gap-y-12 relative z-10 items-center justify-items-center">
              {[
                { id: 'FL' as Corner, label: 'Front Left' },
                { id: 'FR' as Corner, label: 'Front Right' },
                { id: 'RL' as Corner, label: 'Rear Left' },
                { id: 'RR' as Corner, label: 'Rear Right' }
              ].map(corner => (
                <div key={corner.id} className="flex flex-col items-center gap-0 sm:gap-4">
                  <Gauge label={corner.label} value={state.pressures[corner.id]} target={state.targets[corner.id]} />
                  <div className="flex gap-3 sm:gap-6">
                    <ControlButton direction="up" label="Rise" onPressStart={() => handleControl(corner.id, 'up', true)} onPressEnd={() => handleControl(corner.id, 'up', false)} />
                    <ControlButton direction="down" label="Drop" onPressStart={() => handleControl(corner.id, 'down', true)} onPressEnd={() => handleControl(corner.id, 'down', false)} />
                  </div>
                </div>
              ))}
            </div>

            {/* ALL Rise / Drop Controls */}
            <div className="flex justify-center items-center gap-4 sm:gap-10 relative z-10 mt-2 sm:mt-4 pb-0 sm:pb-2">
              <motion.button
                whileTap={{ scale: 0.95 }}
                onMouseDown={() => handleControl('ALL', 'up', true)}
                onMouseUp={() => handleControl('ALL', 'up', false)}
                onMouseLeave={() => handleControl('ALL', 'up', false)}
                onTouchStart={(e) => { e.preventDefault(); handleControl('ALL', 'up', true); }}
                onTouchEnd={(e) => { e.preventDefault(); handleControl('ALL', 'up', false); }}
                className="flex items-center gap-1.5 sm:gap-3 px-4 py-2 sm:px-6 sm:py-3 rounded-xl bg-dark-700/60 border border-white/[0.08] shadow-[0_2px_8px_rgba(0,0,0,0.3)] transition-all active:bg-dark-600/80 active:shadow-inner hover:border-accent/20"
              >
                <ChevronUp className="w-4 h-4 sm:w-5 sm:h-5 text-text-secondary" strokeWidth={2.5} />
                <span className="text-[8px] sm:text-[10px] font-medium uppercase tracking-wider text-text-secondary">Rise All</span>
              </motion.button>

              <motion.button
                whileTap={{ scale: 0.95 }}
                onMouseDown={() => handleControl('ALL', 'down', true)}
                onMouseUp={() => handleControl('ALL', 'down', false)}
                onMouseLeave={() => handleControl('ALL', 'down', false)}
                onTouchStart={(e) => { e.preventDefault(); handleControl('ALL', 'down', true); }}
                onTouchEnd={(e) => { e.preventDefault(); handleControl('ALL', 'down', false); }}
                className="flex items-center gap-1.5 sm:gap-3 px-4 py-2 sm:px-6 sm:py-3 rounded-xl bg-dark-700/60 border border-white/[0.08] shadow-[0_2px_8px_rgba(0,0,0,0.3)] transition-all active:bg-dark-600/80 active:shadow-inner hover:border-accent/20"
              >
                <ChevronDown className="w-4 h-4 sm:w-5 sm:h-5 text-text-secondary" strokeWidth={2.5} />
                <span className="text-[8px] sm:text-[10px] font-medium uppercase tracking-wider text-text-secondary">Drop All</span>
              </motion.button>
            </div>
          </div>

          {/* Tank Status Panel */}
          <div className="glass-card rounded-xl sm:rounded-2xl p-2 sm:p-4 relative overflow-hidden shrink-0">
            <div className="relative z-10 space-y-1 sm:space-y-4">
              <div className="flex items-center justify-between px-1 sm:px-2">
                <div className="flex items-center gap-2 sm:gap-3">
                  <Activity className={`w-3 h-3 sm:w-4 sm:h-4 ${state.compressorActive ? 'text-accent animate-pulse' : 'text-text-muted/40'}`} />
                  <span className="text-[8px] sm:text-[10px] font-medium uppercase tracking-wider text-text-secondary">Tank</span>
                </div>
              </div>

              <HorizontalGauge
                label="Tank"
                value={state.pressures.tank}
                target={state.compressorActive ? 150 : 120}
              />

              {/* Pump Override Toggle + Pump LEDs */}
              <div className="flex items-center justify-between px-1 sm:px-2">
                <button
                  onClick={() => airService.togglePumpOverride()}
                  onTouchEnd={(e) => { e.preventDefault(); airService.togglePumpOverride(); }}
                  className="flex items-center gap-2 sm:gap-3 py-0.5 sm:py-2 rounded-lg"
                >
                  <div className="flex items-center gap-1.5 sm:gap-2">
                    <Power className={`w-3 h-3 sm:w-3.5 sm:h-3.5 ${state.pumpEnabled !== false ? 'text-text-muted' : 'text-danger'}`} />
                    <span className={`text-[7px] sm:text-[9px] font-medium uppercase tracking-wider ${state.pumpEnabled !== false ? 'text-text-muted' : 'text-danger'}`}>
                      Pumps
                    </span>
                  </div>

                  {/* Modern Toggle Switch */}
                  <div className={`w-10 h-5 sm:w-14 sm:h-7 rounded-full relative transition-all duration-300 ${state.pumpEnabled !== false ? 'toggle-track-active' : 'toggle-track'}`}>
                    <div
                      className={`absolute top-[3px] w-[14px] h-[14px] sm:w-[22px] sm:h-[22px] rounded-full transition-all duration-300 ${
                        state.pumpEnabled !== false
                          ? 'left-[calc(100%-17px)] sm:left-[calc(100%-25px)] bg-accent shadow-[0_0_8px_rgba(0,212,255,0.5)]'
                          : 'left-[3px] bg-text-muted'
                      }`}
                    />
                  </div>
                </button>

                {/* Dual Pump LED Indicators */}
                <div className="flex gap-3 sm:gap-5">
                  <div className="flex flex-col items-center gap-0.5 sm:gap-1">
                    <div className={`w-3 h-3 sm:w-4 sm:h-4 rounded-full transition-all duration-300 ${state.compressorActive ? 'bg-accent shadow-[0_0_12px_4px_rgba(0,212,255,0.4)]' : 'bg-dark-600'}`} />
                    <span className="text-[5px] sm:text-[7px] font-medium text-text-muted uppercase tracking-tighter">P1</span>
                  </div>
                  <div className="flex flex-col items-center gap-0.5 sm:gap-1">
                    <div className={`w-3 h-3 sm:w-4 sm:h-4 rounded-full transition-all duration-300 ${state.compressorActive && state.pressures.tank < 105 ? 'bg-accent shadow-[0_0_12px_4px_rgba(0,212,255,0.4)]' : 'bg-dark-600'}`} />
                    <span className="text-[5px] sm:text-[7px] font-medium text-text-muted uppercase tracking-tighter">P2</span>
                  </div>
                </div>
              </div>
            </div>
          </div>

          {/* Tank Maintenance Due Banner */}
          {state.tankMaint?.due && (
            <button
              onClick={() => setTankMaintModalOpen(true)}
              onTouchEnd={(e) => { e.preventDefault(); setTankMaintModalOpen(true); }}
              className="w-full rounded-xl p-2 sm:p-3 bg-danger/20 border border-danger/30 flex items-center justify-center gap-2 sm:gap-3 animate-pulse shrink-0"
            >
              <Wrench className="w-3.5 h-3.5 sm:w-4 sm:h-4 text-danger" />
              <span className="text-[8px] sm:text-[10px] font-semibold uppercase tracking-wider text-danger">Tank Service Due</span>
              <Wrench className="w-3.5 h-3.5 sm:w-4 sm:h-4 text-danger" />
            </button>
          )}
        </section>

        {/* Unit 2: Presets & Level Mode */}
        <section className="snap-start min-h-dvh p-3 sm:p-4 flex flex-col gap-3 sm:gap-4">
          <div className="glass-card rounded-2xl sm:rounded-3xl p-4 sm:p-8 relative overflow-hidden flex-1">
            <div className="relative z-10 h-full flex flex-col">
              {/* Header Bar */}
              <div className="flex items-center justify-between mb-4 sm:mb-6 -mx-4 -mt-4 sm:-mx-8 sm:-mt-8 px-4 sm:px-6 py-2.5 sm:py-3 bg-dark-800/50 border-b border-white/[0.06] rounded-t-2xl sm:rounded-t-3xl">
                <ImpalaSSLogo />
                <div className={`w-2.5 h-2.5 sm:w-3 sm:h-3 rounded-full ${state.connected ? 'bg-success shadow-[0_0_10px_rgba(34,197,94,0.6)]' : 'bg-danger shadow-[0_0_10px_rgba(239,68,68,0.6)]'} transition-all`} />
              </div>

              <div className="grid grid-cols-1 sm:grid-cols-2 gap-3 sm:gap-4 flex-1">
                {[
                  { name: 'Lay', index: 0 },
                  { name: 'Cruise', index: 1 },
                  { name: 'Max', index: 2 },
                ].map((preset) => {
                  const active = isPresetActive(preset.index);
                  return (
                    <button
                      key={preset.name}
                      onMouseDown={() => handlePresetPressStart(preset.index, preset.name)}
                      onMouseUp={() => handlePresetPressEnd(preset.index)}
                      onMouseLeave={() => clearLongPress()}
                      onTouchStart={() => handlePresetPressStart(preset.index, preset.name)}
                      onTouchEnd={(e) => { e.preventDefault(); handlePresetPressEnd(preset.index); }}
                      className={`p-4 sm:p-6 rounded-xl flex items-center justify-between transition-all active:scale-[0.98] group ${
                        active
                          ? 'bg-accent/10 border border-accent/30 shadow-[0_0_20px_rgba(0,212,255,0.1)]'
                          : 'bg-dark-700/40 border border-white/[0.06] hover:bg-dark-700/60 hover:border-white/[0.1]'
                      }`}
                    >
                      <span className={`font-semibold text-base sm:text-lg uppercase tracking-widest ${active ? 'text-accent' : 'text-text-secondary'}`}>{preset.name}</span>
                      <div className={`w-2.5 h-2.5 sm:w-3 sm:h-3 rounded-full transition-all duration-500 ${active ? 'bg-accent shadow-[0_0_10px_rgba(0,212,255,0.7)]' : 'bg-dark-600'}`} />
                    </button>
                  );
                })}
              </div>

            </div>
          </div>

          {/* Level Mode Control */}
          <LevelControl
            level={state.level ?? 0}
            onSetLevel={(mode) => airService.setLevelMode(mode)}
          />
        </section>

        {/* Unit 3: Leak Monitor */}
        <section className="snap-start min-h-dvh p-3 sm:p-4 flex flex-col gap-3 sm:gap-4">
          <div className="glass-card rounded-2xl sm:rounded-3xl p-4 sm:p-8 relative overflow-hidden flex-1 flex flex-col">
            <div className="relative z-10 h-full flex flex-col">
              {/* Header Bar */}
              <div className="flex items-center justify-between mb-4 sm:mb-6 -mx-4 -mt-4 sm:-mx-8 sm:-mt-8 px-4 sm:px-6 py-2.5 sm:py-3 bg-dark-800/50 border-b border-white/[0.06] rounded-t-2xl sm:rounded-t-3xl">
                <span className="text-[9px] sm:text-[11px] font-medium uppercase tracking-wider text-text-secondary">Leak Monitor</span>
                {leakStatus.valid && (
                  <span className="text-[8px] sm:text-[10px] font-medium text-text-muted tracking-wider">
                    {formatElapsed(leakStatus.elapsed || 0)} ago
                  </span>
                )}
              </div>

              {!leakStatus.valid ? (
                <div className="flex-1 flex flex-col items-center justify-center gap-3">
                  <div className="w-12 h-12 sm:w-16 sm:h-16 rounded-2xl bg-dark-700/50 flex items-center justify-center border border-white/[0.06]">
                    <span className="text-2xl sm:text-3xl opacity-30">💧</span>
                  </div>
                  <p className="text-[10px] sm:text-xs text-text-muted font-medium uppercase tracking-wider">No snapshot data yet</p>
                  <p className="text-[8px] sm:text-[10px] text-text-muted/60 font-medium text-center max-w-[200px]">
                    A pressure snapshot will be saved automatically while the system runs
                  </p>
                </div>
              ) : (
                <div className="flex-1 flex flex-col">
                  {/* Column headers */}
                  <div className="flex items-center gap-2 sm:gap-4 px-1 sm:px-2 mb-1.5 sm:mb-2">
                    <div className="w-3 sm:w-4" />
                    <span className="w-8 sm:w-10" />
                    <div className="flex-1 grid grid-cols-3 gap-1 text-center">
                      <span className="text-[7px] sm:text-[8px] font-medium text-text-muted uppercase tracking-wider">Snap</span>
                      <span className="text-[7px] sm:text-[8px] font-medium text-text-muted uppercase tracking-wider">Now</span>
                      <span className="text-[7px] sm:text-[8px] font-medium text-text-muted uppercase tracking-wider">Rate</span>
                    </div>
                  </div>

                  {/* Sensor rows */}
                  <div className="space-y-1.5 sm:space-y-2 flex-1">
                    {LEAK_SENSOR_NAMES.map((name, i) => {
                      const sensorStatus = leakStatus.status?.[i] ?? 0;
                      const snapshot = leakStatus.snapshot?.[i] ?? 0;
                      const current = leakStatus.current?.[i] ?? 0;
                      const rate = leakStatus.rates?.[i] ?? 0;
                      const statusColor = sensorStatus === 2
                        ? 'bg-danger shadow-[0_0_8px_rgba(239,68,68,0.5)]'
                        : sensorStatus === 1
                          ? 'bg-warning shadow-[0_0_8px_rgba(245,158,11,0.4)]'
                          : 'bg-success';
                      const rateColor = sensorStatus === 2
                        ? 'text-danger'
                        : sensorStatus === 1
                          ? 'text-warning'
                          : 'text-text-secondary';

                      return (
                        <div key={name} className="flex items-center gap-2 sm:gap-4 bg-dark-700/30 rounded-xl p-2 sm:p-3 border border-white/[0.04]">
                          <div className={`w-2.5 h-2.5 sm:w-3 sm:h-3 rounded-full ${statusColor} shrink-0`} />
                          <span className="text-[10px] sm:text-xs font-medium text-text-muted uppercase tracking-wider w-8 sm:w-10 shrink-0">{name}</span>
                          <div className="flex-1 grid grid-cols-3 gap-1 text-center">
                            <div className="text-[11px] sm:text-sm font-mono font-medium text-text-secondary">{snapshot.toFixed(0)}</div>
                            <div className="text-[11px] sm:text-sm font-mono font-medium text-text-secondary">{current.toFixed(0)}</div>
                            <div className={`text-[11px] sm:text-sm font-mono font-medium ${rateColor}`}>
                              {snapshot < 5 ? '—' : rate > 0.005 ? `-${rate.toFixed(1)}` : '0'}
                            </div>
                          </div>
                        </div>
                      );
                    })}
                  </div>
                </div>
              )}

              {/* Reset Button */}
              <div className="flex justify-center mt-3 sm:mt-4 pt-2 sm:pt-3 border-t border-white/[0.06]">
                <motion.button
                  whileTap={{ scale: 0.95 }}
                  onClick={() => { airService.resetLeakMonitor(); setLeakStatus({ valid: false }); }}
                  className="px-5 sm:px-6 py-1.5 sm:py-2 rounded-xl bg-dark-700/60 border border-white/[0.08] shadow-[0_2px_8px_rgba(0,0,0,0.3)] transition-all active:bg-dark-600/80 hover:border-accent/20"
                >
                  <span className="text-[8px] sm:text-[10px] font-medium uppercase tracking-wider text-text-secondary">Reset Snapshot</span>
                </motion.button>
              </div>
            </div>
          </div>
        </section>
      </main>

      {/* Long-press Save Preset Modal */}
      {saveModal && (
        <div className="absolute inset-0 z-50 flex items-center justify-center bg-dark-900/70 backdrop-blur-md">
          <div className="glass-card rounded-2xl sm:rounded-3xl p-6 sm:p-8 mx-6 max-w-sm text-center">
            <div className="relative z-10">
              {saveModal.saved ? (
                <>
                  <div className="w-10 h-10 sm:w-12 sm:h-12 rounded-full bg-success/20 border border-success/40 mx-auto mb-3 sm:mb-4 flex items-center justify-center">
                    <span className="text-success text-lg sm:text-xl font-bold">✓</span>
                  </div>
                  <p className="text-sm sm:text-base font-semibold text-text-primary uppercase tracking-widest">Saved</p>
                  <p className="text-[10px] sm:text-xs text-text-muted mt-1 font-medium">
                    Pressures saved to {saveModal.presetName}
                  </p>
                </>
              ) : (
                <>
                  <div className="w-10 h-10 sm:w-12 sm:h-12 rounded-full bg-accent/20 border border-accent/40 mx-auto mb-3 sm:mb-4 animate-pulse flex items-center justify-center">
                    <span className="text-accent text-lg sm:text-xl">💾</span>
                  </div>
                  <p className="text-sm sm:text-base font-semibold text-text-primary uppercase tracking-widest">Keep Holding</p>
                  <p className="text-[10px] sm:text-xs text-text-muted mt-1 font-medium">
                    to save current pressures to {saveModal.presetName}
                  </p>
                </>
              )}
            </div>
          </div>
        </div>
      )}

      {/* Tank Maintenance Modal */}
      {tankMaintModalOpen && (
        <div className="absolute inset-0 z-50 flex items-center justify-center bg-dark-900/70 backdrop-blur-md">
          <div className="glass-card rounded-2xl sm:rounded-3xl p-6 sm:p-8 mx-6 max-w-sm text-center relative">
            <div className="relative z-10">
              <div className="w-10 h-10 sm:w-12 sm:h-12 rounded-full bg-warning/20 border border-warning/40 mx-auto mb-3 sm:mb-4 flex items-center justify-center">
                <Wrench className="w-5 h-5 sm:w-6 sm:h-6 text-warning" />
              </div>
              <p className="text-sm sm:text-base font-semibold text-text-primary uppercase tracking-widest">Tank Service Due</p>

              {/* Days info */}
              <div className="mt-2 sm:mt-3">
                {state.tankMaint?.daysRemaining !== undefined && state.tankMaint.daysRemaining <= 0 ? (
                  <p className="text-lg sm:text-xl font-bold text-danger">
                    {Math.abs(state.tankMaint.daysRemaining)} days overdue
                  </p>
                ) : (
                  <p className="text-lg sm:text-xl font-bold text-warning">
                    {state.tankMaint?.daysRemaining ?? '?'} days remaining
                  </p>
                )}
              </div>

              {/* Last service date */}
              {state.tankMaint?.lastService && (
                <p className="text-[10px] sm:text-xs text-text-muted mt-1.5 font-medium">
                  Last service: {new Date(state.tankMaint.lastService * 1000).toLocaleDateString()}
                </p>
              )}

              {/* Buttons */}
              <div className="flex gap-3 mt-4 sm:mt-6 justify-center">
                <motion.button
                  whileTap={{ scale: 0.95 }}
                  onClick={() => setTankMaintModalOpen(false)}
                  className="px-4 sm:px-5 py-2 sm:py-2.5 rounded-xl bg-dark-700/60 border border-white/[0.08] shadow-[0_2px_8px_rgba(0,0,0,0.3)] transition-all active:bg-dark-600/80 hover:border-white/[0.15]"
                >
                  <span className="text-[8px] sm:text-[10px] font-medium uppercase tracking-wider text-text-secondary">Dismiss</span>
                </motion.button>
                <motion.button
                  whileTap={{ scale: 0.95 }}
                  onClick={() => {
                    airService.resetTankMaint();
                    setTankMaintModalOpen(false);
                  }}
                  className="px-4 sm:px-5 py-2 sm:py-2.5 rounded-xl bg-success/20 border border-success/30 transition-all active:bg-success/30 hover:border-success/50"
                >
                  <span className="text-[8px] sm:text-[10px] font-medium uppercase tracking-wider text-success">Service Complete</span>
                </motion.button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
