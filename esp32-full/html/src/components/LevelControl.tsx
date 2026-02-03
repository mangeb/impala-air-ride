import React from 'react';

interface LevelControlProps {
  level: number; // 0=off, 1=front, 2=rear, 3=all
  onSetLevel: (mode: number) => void;
}

/**
 * Modern pill toggle switch.
 */
const ToggleSwitch: React.FC<{
  label: string;
  on: boolean;
  onToggle: () => void;
}> = ({ label, on, onToggle }) => (
  <button
    onClick={onToggle}
    onTouchEnd={(e) => { e.preventDefault(); onToggle(); }}
    className="flex flex-col items-center gap-1.5 sm:gap-2"
  >
    {/* Toggle track */}
    <div className={`w-12 h-6 sm:w-14 sm:h-7 rounded-full relative transition-all duration-300 ${on ? 'toggle-track-active' : 'toggle-track'}`}>
      {/* Toggle knob */}
      <div
        className={`absolute top-[3px] w-[18px] h-[18px] sm:w-[22px] sm:h-[22px] rounded-full transition-all duration-300 ${
          on
            ? 'left-[calc(100%-21px)] sm:left-[calc(100%-25px)] bg-accent shadow-[0_0_8px_rgba(0,212,255,0.5)]'
            : 'left-[3px] bg-text-muted'
        }`}
      />
    </div>

    {/* Label */}
    <span className={`text-[8px] sm:text-[10px] font-medium uppercase tracking-wider transition-colors ${on ? 'text-accent' : 'text-text-muted'}`}>
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
      onSetLevel(rearOn ? 2 : 0);
    } else {
      onSetLevel(rearOn ? 3 : 1);
    }
  };

  const toggleRear = () => {
    if (rearOn) {
      onSetLevel(frontOn ? 1 : 0);
    } else {
      onSetLevel(frontOn ? 3 : 2);
    }
  };

  const toggleAll = () => {
    if (allOn) {
      onSetLevel(0);
    } else {
      onSetLevel(3);
    }
  };

  return (
    <div className="glass-card rounded-2xl sm:rounded-3xl p-3 sm:p-4 shrink-0">
      <div className="relative z-10">
        {/* Header */}
        <div className="flex items-center justify-between px-1 sm:px-2 mb-3 sm:mb-4">
          <div className="flex items-center gap-2 sm:gap-3">
            <div className={`w-2 h-2 sm:w-2.5 sm:h-2.5 rounded-full transition-all duration-300 ${level > 0 ? 'bg-accent shadow-[0_0_8px_rgba(0,212,255,0.6)]' : 'bg-dark-600'}`} />
            <span className="text-[9px] sm:text-[11px] font-medium uppercase tracking-wider text-text-secondary">Level Mode</span>
          </div>
          <span className={`text-[8px] sm:text-[10px] font-medium uppercase tracking-wider ${level > 0 ? 'text-accent' : 'text-text-muted'}`}>
            {level === 0 ? 'Off' : level === 1 ? 'Front' : level === 2 ? 'Rear' : 'All'}
          </span>
        </div>

        {/* Toggle switches row */}
        <div className="flex items-center justify-center gap-6 sm:gap-10">
          <ToggleSwitch label="Front" on={frontOn} onToggle={toggleFront} />
          <ToggleSwitch label="Rear" on={rearOn} onToggle={toggleRear} />

          {/* Separator */}
          <div className="w-[1px] h-6 sm:h-7 bg-white/[0.06]" />

          <ToggleSwitch label="All" on={allOn} onToggle={toggleAll} />
        </div>
      </div>
    </div>
  );
};
