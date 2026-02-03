import React from 'react';
import { motion } from 'motion/react';

interface HorizontalGaugeProps {
  value: number;
  target?: number | null;
  label: string;
  min?: number;
  max?: number;
  unit?: string;
}

export const HorizontalGauge: React.FC<HorizontalGaugeProps> = ({
  value,
  target = null,
  min = 0,
  max = 150,
  unit = 'PSI'
}) => {
  const percentage = Math.min(100, Math.max(0, ((value - min) / (max - min)) * 100));
  const targetPercentage = target !== null ? Math.min(100, Math.max(0, ((target - min) / (max - min)) * 100)) : null;

  return (
    <div className="relative w-full h-12 sm:h-20 rounded-xl overflow-hidden flex flex-col justify-end p-1 sm:p-2" style={{ background: 'rgba(15, 23, 42, 0.8)', border: '1px solid rgba(255,255,255,0.06)', boxShadow: 'inset 0 2px 6px rgba(0,0,0,0.4)' }}>
      {/* Background Markings */}
      <div className="absolute inset-0 flex items-end px-2 sm:px-4 pb-4 sm:pb-6 opacity-20">
        <div className="w-full h-full flex justify-between items-end border-b border-white/20">
          {[...Array(16)].map((_, i) => (
            <div
              key={i}
              className={`bg-white/40 ${i % 5 === 0 ? 'w-[1.5px] h-4' : 'w-[0.5px] h-2'}`}
            />
          ))}
        </div>
      </div>

      {/* Numbers */}
      <div className="absolute inset-x-0 top-0 flex justify-between items-start px-2 sm:px-4 pt-1 sm:pt-2 opacity-40">
        {[0, 50, 100, 150].map((num) => (
          <span key={num} className="text-[7px] sm:text-[10px] font-mono font-medium text-text-secondary">{num}</span>
        ))}
      </div>

      {/* Target Indicator */}
      {targetPercentage !== null && (
        <div className="absolute inset-y-0 left-2 right-2 sm:left-4 sm:right-4 pointer-events-none z-10">
          <motion.div
            className="absolute top-0 h-full w-0.5 bg-accent shadow-[0_0_8px_rgba(0,212,255,0.5)]"
            initial={false}
            animate={{ left: `${targetPercentage}%` }}
            transition={{ type: 'spring', stiffness: 60, damping: 12 }}
          >
            <div className="absolute top-0 left-1/2 -translate-x-1/2 w-2 h-2 rounded-full bg-accent shadow-[0_0_4px_rgba(0,212,255,0.6)]" />
            <div className="absolute bottom-0 left-1/2 -translate-x-1/2 w-2 h-2 rounded-full bg-accent shadow-[0_0_4px_rgba(0,212,255,0.6)]" />
          </motion.div>
        </div>
      )}

      {/* Actual Pressure Indicator */}
      <div className="absolute inset-y-0 left-2 right-2 sm:left-4 sm:right-4 pointer-events-none z-0">
        <motion.div
          className="absolute top-2 bottom-2 w-[1px] bg-accent/30"
          initial={false}
          animate={{ left: `${percentage}%` }}
          transition={{ type: 'spring', stiffness: 40, damping: 15 }}
        >
          <div className="absolute top-0 left-1/2 -translate-x-1/2 w-1.5 h-1.5 rounded-full bg-accent/40" />
          <div className="absolute bottom-0 left-1/2 -translate-x-1/2 w-1.5 h-1.5 rounded-full bg-accent/40" />
        </motion.div>
      </div>
    </div>
  );
};
