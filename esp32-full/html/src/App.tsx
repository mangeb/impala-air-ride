/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { motion } from 'motion/react';
import { Settings, Activity, ShieldAlert, ChevronUp, ChevronDown } from 'lucide-react';
import { Gauge } from './components/Gauge';
import { HorizontalGauge } from './components/HorizontalGauge';
import { ControlButton } from './components/ControlButton';
import { ImpalaSSLogo } from './components/ImpalaSSLogo';
import { airService } from './services/airService';
import { SystemState, Corner } from './types';

export default function App() {
  const [state, setState] = useState<SystemState>({
    pressures: { FL: 0, FR: 0, RL: 0, RR: 0, tank: 0 },
    targets: { FL: 45, FR: 45, RL: 30, RR: 30 },
    compressorActive: false,
    connected: false
  });

  const [showTankDetails, setShowTankDetails] = useState(false);

  const applyPreset = useCallback((index: number) => {
    airService.applyPreset(index);
  }, []);

  // Poll for status (400ms matches ESP32 web server interval)
  useEffect(() => {
    const interval = setInterval(async () => {
      const status = await airService.getStatus();
      setState(prev => ({ ...prev, ...status }));
    }, 400);
    return () => clearInterval(interval);
  }, []);

  const handleControl = useCallback((corner: Corner | 'ALL', direction: 'up' | 'down', start: boolean) => {
    const type = direction === 'up' ? 'inflate' : 'deflate';
    
    if (corner === 'ALL') {
      ['FL', 'FR', 'RL', 'RR'].forEach(c => airService.setSolenoid(c, type, start));
    } else {
      airService.setSolenoid(corner, type, start);
    }
  }, []);

  return (
    <div className="min-h-screen w-full max-w-6xl mx-auto flex flex-col bg-impala-black shadow-2xl relative overflow-hidden font-sans vinyl-texture sm:rounded-[3rem] sm:my-4 lg:my-8 sm:h-[95vh] lg:h-[90vh] sm:border-8 sm:border-black/40">
      {/* Chrome Trim Header */}
      <header className="h-24 relative z-30 flex flex-col">
        <div className="h-16 bg-gradient-to-b from-impala-chrome to-impala-silver border-b-4 border-black/60 flex items-center justify-between px-6 shadow-xl">
          <div className="flex-1 flex justify-start">
            <ImpalaSSLogo />
          </div>
          <div className="flex items-center gap-4">
            <div className={`w-2.5 h-2.5 rounded-full ${state.connected ? 'bg-green-500 shadow-[0_0_10px_rgba(34,197,94,0.8)]' : 'bg-red-600 shadow-[0_0_10px_rgba(220,38,38,0.8)]'} transition-all`} />
            <button 
              className="w-10 h-10 rounded-lg bg-black/10 flex items-center justify-center border border-black/20 hover:bg-black/20 transition-colors"
            >
              <Settings className="w-5 h-5 text-black/60" />
            </button>
          </div>
        </div>
        {/* Pinstripe Detail */}
        <div className="h-8 flex items-center justify-center px-6">
          <div className="w-full h-[1px] bg-white/10" />
        </div>
      </header>

      {/* Main Content Area - Scroll Snapping Enabled */}
      <main className="flex-1 overflow-y-auto relative snap-y snap-mandatory scroll-smooth">
        {/* Unit 1: Gauges, Tank, and Master Controls */}
        <section className="snap-start min-h-full flex flex-col p-4 space-y-6">
          {/* Engine-Turned Aluminum Panel for Gauges and Individual Controls */}
          <div className="engine-turned rounded-[2.5rem] p-4 sm:p-8 border-4 border-black/90 shadow-[0_15px_40px_rgba(0,0,0,0.9)] relative overflow-hidden flex-1 flex flex-col justify-center">
            <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none" />
            <div className="grid grid-cols-2 lg:grid-cols-4 gap-x-4 lg:gap-x-8 gap-y-12 relative z-10 items-center justify-items-center">
              {[
                { id: 'FL' as Corner, label: 'Front Left' },
                { id: 'FR' as Corner, label: 'Front Right' },
                { id: 'RL' as Corner, label: 'Rear Left' },
                { id: 'RR' as Corner, label: 'Rear Right' }
              ].map(corner => (
                <div key={corner.id} className="flex flex-col items-center gap-4">
                  <Gauge label={corner.label} value={state.pressures[corner.id]} target={state.targets[corner.id]} />
                  <div className="flex gap-6 pb-2">
                    <ControlButton direction="up" label="Rise" onPressStart={() => handleControl(corner.id, 'up', true)} onPressEnd={() => handleControl(corner.id, 'up', false)} />
                    <ControlButton direction="down" label="Drop" onPressStart={() => handleControl(corner.id, 'down', true)} onPressEnd={() => handleControl(corner.id, 'down', false)} />
                  </div>
                </div>
              ))}
            </div>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-6 items-stretch">
            {/* Tank Status - Styled as a secondary aluminum panel */}
            <div className="engine-turned rounded-[2rem] p-4 border-4 border-black/90 shadow-[0_10px_30px_rgba(0,0,0,0.8)] relative overflow-hidden flex flex-col justify-center">
              <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none" />
              
              <div className="relative z-10 space-y-4">
                <div className="flex items-center justify-between px-2">
                  <div className="flex items-center gap-3">
                    <Activity className={`w-4 h-4 ${state.compressorActive ? 'text-impala-red animate-pulse' : 'text-black/40'}`} />
                    <span className="text-[9px] font-black uppercase tracking-[0.3em] text-black/60">Reservoir Status</span>
                  </div>
                  
                  {/* Dual Pump LED Indicators - Integrated into panel */}
                  <div className="flex gap-6">
                    <div className="flex flex-col items-center gap-1">
                      <div className={`w-4 h-4 rounded-full transition-all duration-300 border-2 border-black/40 ${state.compressorActive ? 'bg-amber-500 shadow-[0_0_12px_rgba(245,158,11,0.9)]' : 'bg-black/20'}`} />
                      <span className="text-[6px] font-black text-black/40 uppercase tracking-tighter">P1</span>
                    </div>
                    <div className="flex flex-col items-center gap-1">
                      <div className={`w-4 h-4 rounded-full transition-all duration-300 border-2 border-black/40 ${state.compressorActive && state.pressures.tank < 105 ? 'bg-amber-500 shadow-[0_0_12px_rgba(245,158,11,0.9)]' : 'bg-black/20'}`} />
                      <span className="text-[6px] font-black text-black/40 uppercase tracking-tighter">P2</span>
                    </div>
                  </div>
                </div>

                <HorizontalGauge 
                  label="Tank" 
                  value={state.pressures.tank} 
                  target={state.compressorActive ? 150 : 120}
                />
              </div>
            </div>

            {/* Master Controls - Adapts to screen size */}
            <div className="flex justify-center items-center gap-8 sm:gap-16 bg-black/20 rounded-[2rem] p-6 border border-white/5">
              <div className="flex flex-col items-center gap-3">
                <span className="text-[8px] font-black uppercase tracking-[0.3em] text-white/20">Master Rise</span>
                <motion.button
                  whileTap={{ scale: 0.9, y: 2 }}
                  onMouseDown={() => handleControl('ALL', 'up', true)}
                  onMouseUp={() => handleControl('ALL', 'up', false)}
                  onMouseLeave={() => handleControl('ALL', 'up', false)}
                  onTouchStart={(e) => { e.preventDefault(); handleControl('ALL', 'up', true); }}
                  onTouchEnd={(e) => { e.preventDefault(); handleControl('ALL', 'up', false); }}
                  className="w-16 h-16 sm:w-24 sm:h-24 rounded-xl bg-gradient-to-b from-impala-chrome to-impala-silver border-2 border-black/60 flex items-center justify-center shadow-xl group"
                >
                  <div className="bg-black rounded-lg p-1.5 sm:p-3">
                    <ChevronUp className="w-6 h-6 sm:w-10 sm:h-10 text-white" strokeWidth={4} />
                  </div>
                </motion.button>
              </div>
              <div className="flex flex-col items-center gap-3">
                <span className="text-[8px] font-black uppercase tracking-[0.3em] text-white/20">Master Drop</span>
                <motion.button
                  whileTap={{ scale: 0.9, y: 2 }}
                  onMouseDown={() => handleControl('ALL', 'down', true)}
                  onMouseUp={() => handleControl('ALL', 'down', false)}
                  onMouseLeave={() => handleControl('ALL', 'down', false)}
                  onTouchStart={(e) => { e.preventDefault(); handleControl('ALL', 'down', true); }}
                  onTouchEnd={(e) => { e.preventDefault(); handleControl('ALL', 'down', false); }}
                  className="w-16 h-16 sm:w-24 sm:h-24 rounded-xl bg-gradient-to-b from-impala-chrome to-impala-silver border-2 border-black/60 flex items-center justify-center shadow-xl group"
                >
                  <div className="bg-black rounded-lg p-1.5 sm:p-3">
                    <ChevronDown className="w-6 h-6 sm:w-10 sm:h-10 text-white" strokeWidth={4} />
                  </div>
                </motion.button>
              </div>
            </div>
          </div>
        </section>

        {/* Unit 2: Presets - Below the fold */}
        <section className="snap-start min-h-full p-4 flex flex-col space-y-6">
          <div className="engine-turned rounded-[2.5rem] p-6 sm:p-8 border-4 border-black/90 shadow-[0_15px_40px_rgba(0,0,0,0.9)] relative overflow-hidden flex-1">
            <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none" />
            
            <div className="relative z-10 h-full flex flex-col">
              <div className="flex flex-col mb-8">
                <h2 className="text-2xl font-serif italic text-black/80 font-black">Stance Presets</h2>
                <div className="w-12 h-1 bg-impala-red mt-1" />
              </div>

              <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 flex-1">
                {[
                  { name: 'Lay', icon: '⬇️', index: 0 },
                  { name: 'Cruise', icon: '🚗', index: 1 },
                  { name: 'Max', icon: '⬆️', index: 2 },
                ].map((preset) => (
                  <button
                    key={preset.name}
                    onClick={() => applyPreset(preset.index)}
                    className="bg-black/10 border-2 border-black/20 p-6 rounded-2xl flex items-center justify-between transition-all hover:bg-black/20 active:scale-[0.98] shadow-md group"
                  >
                    <div className="flex items-center gap-6">
                      <span className="text-3xl grayscale opacity-60 group-hover:grayscale-0 group-hover:opacity-100 transition-all">{preset.icon}</span>
                      <span className="font-black text-xl text-black/70 uppercase tracking-widest">{preset.name}</span>
                    </div>
                    <div className="w-10 h-10 rounded-full border-2 border-black/20 flex items-center justify-center bg-white/50">
                      <div className="w-3 h-3 rounded-full bg-impala-red shadow-[0_0_10px_rgba(204,0,0,0.8)]" />
                    </div>
                  </button>
                ))}
              </div>

              {/* Ride Height Memory */}
              <div className="grid grid-cols-2 gap-4">
                <button
                  onClick={() => airService.saveHeight()}
                  className="bg-black/10 border-2 border-black/20 p-4 rounded-2xl flex items-center justify-center transition-all hover:bg-black/20 active:scale-[0.98] shadow-md"
                >
                  <span className="font-black text-sm text-black/70 uppercase tracking-widest">Save Height</span>
                </button>
                <button
                  onClick={() => airService.restoreHeight()}
                  className="bg-black/10 border-2 border-black/20 p-4 rounded-2xl flex items-center justify-center transition-all hover:bg-black/20 active:scale-[0.98] shadow-md"
                >
                  <span className="font-black text-sm text-black/70 uppercase tracking-widest">Restore</span>
                </button>
              </div>

              {/* Emergency Dump */}
              <div className="mt-8 pt-8 border-t border-black/10">
                <button
                  onClick={() => applyPreset(0)}
                  className="w-full py-5 bg-impala-red text-white rounded-xl flex items-center justify-center gap-3 font-black uppercase tracking-[0.3em] text-[10px] hover:brightness-110 transition-all shadow-xl"
                >
                  <ShieldAlert className="w-5 h-5" />
                  Emergency Dump
                </button>
              </div>
            </div>
          </div>
        </section>
      </main>
    </div>
  );
}

