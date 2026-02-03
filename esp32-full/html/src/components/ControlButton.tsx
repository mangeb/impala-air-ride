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
        relative w-9 h-9 sm:w-14 sm:h-14 rounded-xl flex flex-col items-center justify-center
        bg-dark-700/80 border border-white/[0.08]
        shadow-[0_2px_8px_rgba(0,0,0,0.3)]
        transition-all active:bg-dark-600/80 active:shadow-inner
        hover:border-accent/20
      `}
    >
      {direction === 'up' ? (
        <ChevronUp className="w-4 h-4 sm:w-6 sm:h-6 text-text-secondary" strokeWidth={2.5} />
      ) : (
        <ChevronDown className="w-4 h-4 sm:w-6 sm:h-6 text-text-secondary" strokeWidth={2.5} />
      )}
    </motion.button>
  );
};
