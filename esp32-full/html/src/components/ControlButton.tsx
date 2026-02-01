import React from 'react';
import { ChevronUp, ChevronDown } from 'lucide-react';
import { motion } from 'motion/react';

interface ControlButtonProps {
  onPressStart: () => void;
  onPressEnd: () => void;
  direction: 'up' | 'down';
  label: string;
}

export const ControlButton: React.FC<ControlButtonProps> = ({ 
  onPressStart, 
  onPressEnd, 
  direction,
  label 
}) => {
  return (
    <motion.button
      whileTap={{ scale: 0.92, y: 1 }}
      onMouseDown={onPressStart}
      onMouseUp={onPressEnd}
      onMouseLeave={onPressEnd}
      onTouchStart={(e) => { e.preventDefault(); onPressStart(); }}
      onTouchEnd={(e) => { e.preventDefault(); onPressEnd(); }}
      className={`
        relative w-9 h-9 sm:w-14 sm:h-14 rounded-lg border-2 border-black/40 flex flex-col items-center justify-center
        bg-gradient-to-br from-white via-impala-chrome to-impala-silver
        shadow-[0_4px_6px_rgba(0,0,0,0.6),inset_0_1px_1px_rgba(255,255,255,0.8)]
        transition-all active:shadow-inner active:brightness-90
      `}
    >
      <div className="absolute inset-0 rounded-lg bg-gradient-to-tr from-transparent via-white/20 to-transparent pointer-events-none" />
      <div className="bg-black rounded-md p-0.5 sm:p-1 shadow-inner">
        {direction === 'up' ? (
          <ChevronUp className="w-4 h-4 sm:w-6 sm:h-6 text-white" strokeWidth={4} />
        ) : (
          <ChevronDown className="w-4 h-4 sm:w-6 sm:h-6 text-white" strokeWidth={4} />
        )}
      </div>
    </motion.button>
  );
};
