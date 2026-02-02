import React from 'react';

interface LevelControlProps {
  level: number; // 0=off, 1=front, 2=rear, 3=all
  onSetLevel: (mode: number) => void;
}

/**
 * Vertical chrome toggle switch — taller/bigger than the pump toggle.
 * ON = lever at top, OFF = lever at bottom.
 */
const VerticalToggle: React.FC<{
  label: string;
  on: boolean;
  onToggle: () => void;
}> = ({ label, on, onToggle }) => (
  <button
    onClick={onToggle}
    onTouchEnd={(e) => { e.preventDefault(); onToggle(); }}
    className="flex flex-col items-center gap-1.5 sm:gap-2"
  >
    {/* Switch housing */}
    <div className="w-9 h-16 sm:w-12 sm:h-[5.5rem] rounded-sm bg-gradient-to-b from-impala-chrome to-impala-silver border-2 border-black/50 shadow-[0_2px_4px_rgba(0,0,0,0.5),inset_0_1px_0_rgba(255,255,255,0.8)] overflow-hidden relative">
      {/* Dark inset channel */}
      <div className="absolute inset-[3px] rounded-[1px] bg-gradient-to-b from-black/70 to-black/50 shadow-inner">
        {/* ON / OFF labels */}
        <span className={`absolute left-1/2 -translate-x-1/2 top-1 text-[5px] sm:text-[7px] font-black uppercase ${on ? 'text-green-400/90' : 'text-white/20'}`}>
          On
        </span>
        <span className={`absolute left-1/2 -translate-x-1/2 bottom-1 text-[5px] sm:text-[7px] font-black uppercase ${on ? 'text-white/20' : 'text-red-400/80'}`}>
          Off
        </span>

        {/* Chrome rocker lever */}
        <div
          className="absolute left-[2px] right-[2px] h-[40%] rounded-[1px] bg-gradient-to-b from-white via-impala-chrome to-impala-silver border border-black/30 shadow-[0_1px_3px_rgba(0,0,0,0.6),inset_0_1px_0_rgba(255,255,255,0.9)] transition-all duration-200"
          style={{ top: on ? '2px' : 'calc(60% - 2px)' }}
        >
          {/* Grip lines */}
          <div className="absolute inset-y-0 left-1/2 -translate-x-1/2 flex flex-col items-center justify-center gap-[2px]">
            <div className="w-4 sm:w-5 h-[0.5px] bg-black/15 rounded-full" />
            <div className="w-4 sm:w-5 h-[0.5px] bg-black/15 rounded-full" />
            <div className="w-4 sm:w-5 h-[0.5px] bg-black/15 rounded-full" />
          </div>
        </div>
      </div>
    </div>

    {/* Label below switch */}
    <span className={`text-[7px] sm:text-[9px] font-black uppercase tracking-[0.15em] sm:tracking-[0.2em] transition-colors ${on ? 'text-black/70' : 'text-black/40'}`}>
      {label}
    </span>
  </button>
);

export const LevelControl: React.FC<LevelControlProps> = ({ level, onSetLevel }) => {
  const frontOn = level === 1 || level === 3;
  const rearOn = level === 2 || level === 3;
  const allOn = level === 3;

  const toggleFront = () => {
    if (frontOn) {
      // Turn off front: if all was on → rear only, if front only → off
      onSetLevel(rearOn ? 2 : 0);
    } else {
      // Turn on front: if rear is on → all, otherwise → front only
      onSetLevel(rearOn ? 3 : 1);
    }
  };

  const toggleRear = () => {
    if (rearOn) {
      // Turn off rear: if all was on → front only, if rear only → off
      onSetLevel(frontOn ? 1 : 0);
    } else {
      // Turn on rear: if front is on → all, otherwise → rear only
      onSetLevel(frontOn ? 3 : 2);
    }
  };

  const toggleAll = () => {
    if (allOn) {
      onSetLevel(0); // All off
    } else {
      onSetLevel(3); // All on
    }
  };

  return (
    <div className="engine-turned rounded-[1rem] sm:rounded-[2rem] p-3 sm:p-4 border-2 sm:border-4 border-black/90 shadow-[0_10px_30px_rgba(0,0,0,0.8)] relative overflow-hidden shrink-0">
      <div className="absolute inset-0 bg-gradient-to-br from-white/30 via-transparent to-black/20 pointer-events-none" />

      <div className="relative z-10">
        {/* Header */}
        <div className="flex items-center justify-between px-1 sm:px-2 mb-3 sm:mb-4">
          <div className="flex items-center gap-2 sm:gap-3">
            {/* Level indicator LED */}
            <div className={`w-3 h-3 sm:w-4 sm:h-4 rounded-full border border-black/30 transition-all duration-300 ${level > 0 ? 'bg-green-500 shadow-[0_0_10px_4px_rgba(34,197,94,0.5)]' : 'bg-black/20'}`} />
            <span className="text-[7px] sm:text-[9px] font-black uppercase tracking-[0.2em] sm:tracking-[0.3em] text-black/60">Level Mode</span>
          </div>
          <span className={`text-[7px] sm:text-[8px] font-black uppercase tracking-wider ${level > 0 ? 'text-black/60' : 'text-black/30'}`}>
            {level === 0 ? 'Off' : level === 1 ? 'Front' : level === 2 ? 'Rear' : 'All'}
          </span>
        </div>

        {/* Toggle switches row */}
        <div className="flex items-start justify-center gap-6 sm:gap-10">
          <VerticalToggle label="Front" on={frontOn} onToggle={toggleFront} />
          <VerticalToggle label="Rear" on={rearOn} onToggle={toggleRear} />

          {/* Separator line */}
          <div className="w-[1px] h-16 sm:h-[5.5rem] bg-black/15 self-start mt-0" />

          <VerticalToggle label="All" on={allOn} onToggle={toggleAll} />
        </div>
      </div>
    </div>
  );
};
