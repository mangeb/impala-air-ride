/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { motion } from 'motion/react';
import { Activity, Power, ChevronUp, ChevronDown } from 'lucide-react';
import { Gauge } from './components/Gauge';
import { HorizontalGauge } from './components/HorizontalGauge';
import { ControlButton } from './components/ControlButton';
import { LevelControl } from './components/LevelControl';
import { ImpalaSSLogo } from './components/ImpalaSSLogo';
import { airService } from './services/airService';
import { SystemState, Corner, LeakStatus } from './types';

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
    <div className="h-screen h-dvh w-full max-w-6xl mx-auto flex flex-col bg-impala-black shadow-2xl relative overflow-hidden font-sans vinyl-texture sm:rounded-[3rem] sm:my-4 lg:my-8 sm:h-[95dvh] lg:h-[90dvh] sm:border-8 sm:border-black/40">
      {/* Main Content Area - Scroll Snapping Enabled */}
      <main ref={mainRef} className="flex-1 overflow-y-auto relative snap-y snap-mandatory scroll-smooth overscroll-none">
        {/* Unit 1: Gauges, Tank, and Master Controls */}
        <section className="snap-start min-h-dvh flex flex-col p-1.5 sm:p-4 gap-1.5 sm:gap-6">
          {/* Engine-Turned Aluminum Panel for Gauges and Controls */}
          <div className="engine-turned rounded-[1.25rem] sm:rounded-[2.5rem] p-1.5 sm:p-8 border-2 sm:border-4 border-black/90 shadow-[0_15px_40px_rgba(0,0,0,0.9)] relative overflow-hidden flex-1 flex flex-col justify-center">
            <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none" />
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
            <div className="flex justify-center items-center gap-4 sm:gap-10 relative z-10 mt-1 sm:mt-4 pb-0 sm:pb-2">
              <motion.button
                whileTap={{ scale: 0.92, y: 1 }}
                onMouseDown={() => handleControl('ALL', 'up', true)}
                onMouseUp={() => handleControl('ALL', 'up', false)}
                onMouseLeave={() => handleControl('ALL', 'up', false)}
                onTouchStart={(e) => { e.preventDefault(); handleControl('ALL', 'up', true); }}
                onTouchEnd={(e) => { e.preventDefault(); handleControl('ALL', 'up', false); }}
                className="flex items-center gap-1.5 sm:gap-3 px-3 py-1.5 sm:px-6 sm:py-3 rounded-xl bg-gradient-to-b from-white via-impala-chrome to-impala-silver border-2 border-black/40 shadow-[0_4px_6px_rgba(0,0,0,0.6),inset_0_1px_1px_rgba(255,255,255,0.8)] transition-all active:shadow-inner active:brightness-90"
              >
                <div className="bg-black rounded-md p-0.5 sm:p-1 shadow-inner">
                  <ChevronUp className="w-4 h-4 sm:w-6 sm:h-6 text-white" strokeWidth={4} />
                </div>
                <span className="text-[7px] sm:text-[10px] font-black uppercase tracking-[0.15em] sm:tracking-[0.2em] text-black/60">Rise All</span>
              </motion.button>

              <motion.button
                whileTap={{ scale: 0.92, y: 1 }}
                onMouseDown={() => handleControl('ALL', 'down', true)}
                onMouseUp={() => handleControl('ALL', 'down', false)}
                onMouseLeave={() => handleControl('ALL', 'down', false)}
                onTouchStart={(e) => { e.preventDefault(); handleControl('ALL', 'down', true); }}
                onTouchEnd={(e) => { e.preventDefault(); handleControl('ALL', 'down', false); }}
                className="flex items-center gap-1.5 sm:gap-3 px-3 py-1.5 sm:px-6 sm:py-3 rounded-xl bg-gradient-to-b from-white via-impala-chrome to-impala-silver border-2 border-black/40 shadow-[0_4px_6px_rgba(0,0,0,0.6),inset_0_1px_1px_rgba(255,255,255,0.8)] transition-all active:shadow-inner active:brightness-90"
              >
                <div className="bg-black rounded-md p-0.5 sm:p-1 shadow-inner">
                  <ChevronDown className="w-4 h-4 sm:w-6 sm:h-6 text-white" strokeWidth={4} />
                </div>
                <span className="text-[7px] sm:text-[10px] font-black uppercase tracking-[0.15em] sm:tracking-[0.2em] text-black/60">Drop All</span>
              </motion.button>
            </div>
          </div>

          {/* Tank Status - Full width panel */}
          <div className="engine-turned rounded-[1rem] sm:rounded-[2rem] p-1 sm:p-4 border-2 sm:border-4 border-black/90 shadow-[0_10px_30px_rgba(0,0,0,0.8)] relative overflow-hidden shrink-0">
            <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none" />

            <div className="relative z-10 space-y-1 sm:space-y-4">
              <div className="flex items-center justify-between px-1 sm:px-2">
                <div className="flex items-center gap-2 sm:gap-3">
                  <Activity className={`w-3 h-3 sm:w-4 sm:h-4 ${state.compressorActive ? 'text-impala-red animate-pulse' : 'text-black/40'}`} />
                  <span className="text-[7px] sm:text-[9px] font-black uppercase tracking-[0.2em] sm:tracking-[0.3em] text-black/60">Tank</span>
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
                    <Power className={`w-3 h-3 sm:w-3.5 sm:h-3.5 ${state.pumpEnabled !== false ? 'text-black/50' : 'text-red-600'}`} />
                    <span className={`text-[7px] sm:text-[8px] font-black uppercase tracking-[0.15em] ${state.pumpEnabled !== false ? 'text-black/50' : 'text-red-600'}`}>
                      Pumps
                    </span>
                  </div>

                  {/* Vintage Chrome Rocker Switch */}
                  <div className="w-10 h-5 sm:w-14 sm:h-7 rounded-sm bg-gradient-to-b from-impala-chrome to-impala-silver border-2 border-black/50 shadow-[0_2px_4px_rgba(0,0,0,0.5),inset_0_1px_0_rgba(255,255,255,0.8)] overflow-hidden relative">
                    {/* Dark inset channel */}
                    <div className="absolute inset-[3px] rounded-[1px] bg-gradient-to-b from-black/70 to-black/50 shadow-inner">
                      {/* ON / OFF labels */}
                      <span className={`absolute left-1 top-1/2 -translate-y-1/2 text-[5px] sm:text-[6px] font-black uppercase ${state.pumpEnabled !== false ? 'text-green-400/80' : 'text-white/20'}`}>
                        On
                      </span>
                      <span className={`absolute right-0.5 sm:right-1 top-1/2 -translate-y-1/2 text-[5px] sm:text-[6px] font-black uppercase ${state.pumpEnabled !== false ? 'text-white/20' : 'text-red-400/80'}`}>
                        Off
                      </span>

                      {/* Chrome rocker lever */}
                      <div className={`absolute top-[2px] bottom-[2px] w-[45%] rounded-[1px] transition-all duration-200 bg-gradient-to-b from-white via-impala-chrome to-impala-silver border border-black/30 shadow-[0_1px_3px_rgba(0,0,0,0.6),inset_0_1px_0_rgba(255,255,255,0.9)] ${state.pumpEnabled !== false ? 'left-[2px]' : 'left-[calc(55%-2px)]'}`}>
                        {/* Grip lines */}
                        <div className="absolute inset-x-0 top-1/2 -translate-y-1/2 flex flex-col items-center gap-[1.5px]">
                          <div className="w-3 h-[0.5px] bg-black/15 rounded-full" />
                          <div className="w-3 h-[0.5px] bg-black/15 rounded-full" />
                          <div className="w-3 h-[0.5px] bg-black/15 rounded-full" />
                        </div>
                      </div>
                    </div>
                  </div>
                </button>

                {/* Dual Pump LED Indicators — same size as connection LED */}
                <div className="flex gap-3 sm:gap-5">
                  <div className="flex flex-col items-center gap-0.5 sm:gap-1">
                    <div className={`w-4 h-4 sm:w-6 sm:h-6 rounded-full transition-all duration-300 border border-black/40 ${state.compressorActive ? 'bg-amber-500 shadow-[0_0_16px_6px_rgba(245,158,11,0.6),0_0_30px_rgba(245,158,11,0.3)]' : 'bg-black/20'}`} />
                    <span className="text-[5px] sm:text-[7px] font-black text-black/40 uppercase tracking-tighter">P1</span>
                  </div>
                  <div className="flex flex-col items-center gap-0.5 sm:gap-1">
                    <div className={`w-4 h-4 sm:w-6 sm:h-6 rounded-full transition-all duration-300 border border-black/40 ${state.compressorActive && state.pressures.tank < 105 ? 'bg-amber-500 shadow-[0_0_16px_6px_rgba(245,158,11,0.6),0_0_30px_rgba(245,158,11,0.3)]' : 'bg-black/20'}`} />
                    <span className="text-[5px] sm:text-[7px] font-black text-black/40 uppercase tracking-tighter">P2</span>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </section>

        {/* Unit 2: Presets & Level Mode - Below the fold */}
        <section className="snap-start min-h-dvh p-3 sm:p-4 flex flex-col gap-3 sm:gap-6">
          <div className="engine-turned rounded-[2rem] sm:rounded-[2.5rem] p-4 sm:p-8 border-4 border-black/90 shadow-[0_15px_40px_rgba(0,0,0,0.9)] relative overflow-hidden flex-1">
            <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none" />

            <div className="relative z-10 h-full flex flex-col">
              {/* Chrome Badge Bar */}
              <div className="flex items-center justify-between mb-4 sm:mb-6 -mx-4 -mt-4 sm:-mx-8 sm:-mt-8 px-4 sm:px-6 py-2 sm:py-3 bg-gradient-to-b from-impala-chrome to-impala-silver border-b-4 border-black/60 rounded-t-[calc(2rem-4px)] sm:rounded-t-[calc(2.5rem-4px)] shadow-md">
                <ImpalaSSLogo />
                <div className={`w-5 h-5 sm:w-6 sm:h-6 rounded-full border border-black/20 ${state.connected ? 'bg-green-500 shadow-[0_0_16px_6px_rgba(34,197,94,0.6),0_0_30px_rgba(34,197,94,0.3)]' : 'bg-red-600 shadow-[0_0_16px_6px_rgba(220,38,38,0.6),0_0_30px_rgba(220,38,38,0.3)]'} transition-all`} />
              </div>

              <div className="grid grid-cols-1 sm:grid-cols-2 gap-3 sm:gap-4 flex-1">
                {[
                  { name: 'Lay', icon: '⬇️', index: 0 },
                  { name: 'Cruise', icon: '🚗', index: 1 },
                  { name: 'Max', icon: '⬆️', index: 2 },
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
                      className="bg-black/10 border-2 border-black/20 p-4 sm:p-6 rounded-2xl flex items-center justify-between transition-all hover:bg-black/20 active:scale-[0.98] shadow-md group"
                    >
                      <div className="flex items-center gap-4 sm:gap-6">
                        <span className="text-2xl sm:text-3xl grayscale opacity-60 group-hover:grayscale-0 group-hover:opacity-100 transition-all">{preset.icon}</span>
                        <span className="font-black text-lg sm:text-xl text-black/70 uppercase tracking-widest">{preset.name}</span>
                      </div>
                      <div className="w-8 h-8 sm:w-10 sm:h-10 rounded-full border-2 border-black/20 flex items-center justify-center bg-white/50">
                        <div className={`w-2.5 h-2.5 sm:w-3 sm:h-3 rounded-full transition-all duration-500 ${active ? 'bg-impala-red shadow-[0_0_10px_rgba(204,0,0,0.8)]' : 'bg-black/15'}`} />
                      </div>
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
        <section className="snap-start min-h-dvh p-3 sm:p-4 flex flex-col gap-3 sm:gap-6">
          <div className="engine-turned rounded-[2rem] sm:rounded-[2.5rem] p-4 sm:p-8 border-4 border-black/90 shadow-[0_15px_40px_rgba(0,0,0,0.9)] relative overflow-hidden flex-1 flex flex-col">
            <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none" />

            <div className="relative z-10 h-full flex flex-col">
              {/* Chrome Badge Bar */}
              <div className="flex items-center justify-between mb-4 sm:mb-6 -mx-4 -mt-4 sm:-mx-8 sm:-mt-8 px-4 sm:px-6 py-2 sm:py-3 bg-gradient-to-b from-impala-chrome to-impala-silver border-b-4 border-black/60 rounded-t-[calc(2rem-4px)] sm:rounded-t-[calc(2.5rem-4px)] shadow-md">
                <span className="text-[8px] sm:text-[11px] font-black uppercase tracking-[0.2em] text-black/60">Leak Monitor</span>
                {leakStatus.valid && (
                  <span className="text-[8px] sm:text-[10px] font-bold text-black/40 tracking-wider">
                    {formatElapsed(leakStatus.elapsed || 0)} ago
                  </span>
                )}
              </div>

              {!leakStatus.valid ? (
                <div className="flex-1 flex flex-col items-center justify-center gap-3">
                  <div className="w-12 h-12 sm:w-16 sm:h-16 rounded-full bg-black/10 flex items-center justify-center">
                    <span className="text-2xl sm:text-3xl opacity-40">💧</span>
                  </div>
                  <p className="text-[10px] sm:text-xs text-black/40 font-bold uppercase tracking-widest">No snapshot data yet</p>
                  <p className="text-[8px] sm:text-[10px] text-black/30 font-medium text-center max-w-[200px]">
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
                      <span className="text-[7px] sm:text-[8px] font-black text-black/30 uppercase tracking-wider">Snap</span>
                      <span className="text-[7px] sm:text-[8px] font-black text-black/30 uppercase tracking-wider">Now</span>
                      <span className="text-[7px] sm:text-[8px] font-black text-black/30 uppercase tracking-wider">Rate</span>
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
                        ? 'bg-red-500 shadow-[0_0_8px_rgba(239,68,68,0.6)]'
                        : sensorStatus === 1
                          ? 'bg-amber-500 shadow-[0_0_8px_rgba(245,158,11,0.5)]'
                          : 'bg-green-500';
                      const rateColor = sensorStatus === 2
                        ? 'text-red-700'
                        : sensorStatus === 1
                          ? 'text-amber-700'
                          : 'text-black/60';

                      return (
                        <div key={name} className="flex items-center gap-2 sm:gap-4 bg-black/[0.07] rounded-xl p-2 sm:p-3 border border-black/[0.06]">
                          <div className={`w-3 h-3 sm:w-4 sm:h-4 rounded-full ${statusColor} shrink-0`} />
                          <span className="text-[10px] sm:text-xs font-black text-black/50 uppercase tracking-wider w-8 sm:w-10 shrink-0">{name}</span>
                          <div className="flex-1 grid grid-cols-3 gap-1 text-center">
                            <div className="text-[11px] sm:text-sm font-bold text-black/60">{snapshot.toFixed(0)}</div>
                            <div className="text-[11px] sm:text-sm font-bold text-black/60">{current.toFixed(0)}</div>
                            <div className={`text-[11px] sm:text-sm font-bold ${rateColor}`}>
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
              <div className="flex justify-center mt-3 sm:mt-4 pt-2 sm:pt-3 border-t border-black/10">
                <motion.button
                  whileTap={{ scale: 0.95 }}
                  onClick={() => { airService.resetLeakMonitor(); setLeakStatus({ valid: false }); }}
                  className="px-5 sm:px-6 py-1.5 sm:py-2 rounded-xl bg-gradient-to-b from-white via-impala-chrome to-impala-silver border-2 border-black/40 shadow-[0_2px_4px_rgba(0,0,0,0.4),inset_0_1px_1px_rgba(255,255,255,0.8)] active:shadow-inner"
                >
                  <span className="text-[7px] sm:text-[9px] font-black uppercase tracking-[0.15em] text-black/50">Reset Snapshot</span>
                </motion.button>
              </div>
            </div>
          </div>
        </section>
      </main>

      {/* Long-press Save Preset Modal */}
      {saveModal && (
        <div className="absolute inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
          <div className="engine-turned rounded-2xl sm:rounded-3xl p-6 sm:p-8 border-4 border-black/80 shadow-[0_20px_60px_rgba(0,0,0,0.9)] mx-6 max-w-sm text-center">
            <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none rounded-2xl sm:rounded-3xl" />
            <div className="relative z-10">
              {saveModal.saved ? (
                <>
                  <div className="w-10 h-10 sm:w-12 sm:h-12 rounded-full bg-green-500 shadow-[0_0_20px_rgba(34,197,94,0.8)] mx-auto mb-3 sm:mb-4 flex items-center justify-center">
                    <span className="text-white text-lg sm:text-xl font-black">✓</span>
                  </div>
                  <p className="text-sm sm:text-base font-black text-black/70 uppercase tracking-widest">Saved!</p>
                  <p className="text-[10px] sm:text-xs text-black/40 mt-1 font-bold uppercase tracking-wider">
                    Pressures saved to {saveModal.presetName}
                  </p>
                </>
              ) : (
                <>
                  <div className="w-10 h-10 sm:w-12 sm:h-12 rounded-full bg-impala-red/80 shadow-[0_0_20px_rgba(204,0,0,0.6)] mx-auto mb-3 sm:mb-4 animate-pulse flex items-center justify-center">
                    <span className="text-white text-lg sm:text-xl">💾</span>
                  </div>
                  <p className="text-sm sm:text-base font-black text-black/70 uppercase tracking-widest">Keep Holding</p>
                  <p className="text-[10px] sm:text-xs text-black/40 mt-1 font-bold uppercase tracking-wider">
                    to save current pressures to {saveModal.presetName}
                  </p>
                </>
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

