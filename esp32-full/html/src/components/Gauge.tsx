import React from 'react';
import { motion } from 'motion/react';

interface GaugeProps {
  value: number;
  target?: number;
  label: string;
  min?: number;
  max?: number;
  unit?: string;
}

export const Gauge: React.FC<GaugeProps> = ({
  value,
  target,
  label,
  min = 0,
  max = 110,
  unit = 'PSI'
}) => {
  const percentage = ((value - min) / (max - min)) * 100;
  const targetPercentage = target !== undefined ? ((target - min) / (max - min)) * 100 : null;

  const rotation = (percentage / 100) * 240 - 120;
  const targetRotation = targetPercentage !== null ? (targetPercentage / 100) * 240 - 120 : null;

  // Major marks every 10 PSI from 0 to 110 = 12 marks
  const majorStep = 10;
  const majorMarks = Array.from({ length: Math.floor((max - min) / majorStep) + 1 }, (_, i) => min + i * majorStep);
  // Minor marks every 5 PSI (between majors)
  const minorStep = 5;
  const minorMarks = Array.from({ length: Math.floor((max - min) / minorStep) + 1 }, (_, i) => min + i * minorStep)
    .filter(v => !majorMarks.includes(v));

  const psiToAngle = (psi: number) => ((psi - min) / (max - min)) * 240 - 120;

  return (
    <div className="flex flex-col items-center justify-center p-0 sm:p-1">
      <div className="mb-0.5 sm:mb-2 text-center">
        <span className="text-text-secondary text-[7px] sm:text-[10px] uppercase tracking-[0.15em] sm:tracking-[0.2em] font-semibold">{label}</span>
      </div>

      {/* Gauge container */}
      <div
        className="relative rounded-full flex items-center justify-center"
        style={{ width: 'var(--gauge-size)', height: 'var(--gauge-size)' }}
      >
        {/* Outer ring - subtle glow */}
        <div className="absolute inset-0 rounded-full bg-dark-700 shadow-[0_0_20px_rgba(0,212,255,0.08),0_4px_16px_rgba(0,0,0,0.5)] border border-white/[0.06]" />

        {/* Inner bezel */}
        <div className="absolute inset-1.5 rounded-full bg-dark-900/80" />

        {/* Gauge Face */}
        <div className="absolute inset-2 rounded-full gauge-face flex items-center justify-center overflow-hidden">
          {/* Gauge Face Markings - Major ticks */}
          <div className="absolute inset-0">
            {majorMarks.map((psi) => (
              <div
                key={`major-${psi}`}
                className="absolute top-0 left-1/2 w-[1.5px] h-3 sm:h-4 bg-text-secondary/50 origin-bottom"
                style={{
                  transform: `translateX(-50%) rotate(${psiToAngle(psi)}deg)`,
                  transformOrigin: `50% var(--gauge-mark-origin)`
                }}
              />
            ))}
            {/* Minor ticks */}
            {minorMarks.map((psi) => (
              <div
                key={`minor-${psi}`}
                className="absolute top-0 left-1/2 w-[0.5px] h-2 sm:h-2.5 bg-text-muted/40 origin-bottom"
                style={{
                  transform: `translateX(-50%) rotate(${psiToAngle(psi)}deg)`,
                  transformOrigin: `50% var(--gauge-mark-origin)`
                }}
              />
            ))}
            {/* PSI number labels at major marks (desktop) */}
            {majorMarks.map((psi) => {
              const angle = psiToAngle(psi);
              const rad = (angle - 90) * (Math.PI / 180);
              return (
                <span
                  key={`label-${psi}`}
                  className="absolute text-[6px] sm:text-[8px] font-mono font-medium text-text-muted/60 leading-none hidden sm:block"
                  style={{
                    left: `calc(50% + ${Math.cos(rad)} * var(--gauge-label-radius))`,
                    top: `calc(50% + ${Math.sin(rad)} * var(--gauge-label-radius))`,
                    transform: 'translate(-50%, -50%)',
                  }}
                >
                  {psi}
                </span>
              );
            })}
            {/* Mobile PSI labels (every 20 PSI + max) */}
            {majorMarks.filter((psi) => psi % 20 === 0 || psi === max).map((psi) => {
              const angle = psiToAngle(psi);
              const rad = (angle - 90) * (Math.PI / 180);
              return (
                <span
                  key={`label-m-${psi}`}
                  className="absolute text-[5.5px] font-mono font-medium text-text-muted/50 leading-none sm:hidden"
                  style={{
                    left: `calc(50% + ${Math.cos(rad)} * var(--gauge-label-radius))`,
                    top: `calc(50% + ${Math.sin(rad)} * var(--gauge-label-radius))`,
                    transform: 'translate(-50%, -50%)',
                  }}
                >
                  {psi}
                </span>
              );
            })}
          </div>

          {/* Actual Pressure Indicator (Inner Needle) */}
          <motion.div
            className="absolute w-[1px] bg-accent/30 origin-bottom rounded-full z-0"
            animate={{ rotate: rotation }}
            transition={{ type: 'spring', stiffness: 40, damping: 15 }}
            style={{ height: 'var(--gauge-needle-minor-h)', bottom: '50%' }}
          />

          {/* Target Setpoint (Major Needle) */}
          {targetRotation !== null && (
            <motion.div
              className="absolute w-0.5 bg-accent origin-bottom rounded-full z-10"
              animate={{ rotate: targetRotation }}
              transition={{ type: 'spring', stiffness: 60, damping: 12 }}
              style={{ height: 'var(--gauge-needle-major-h)', bottom: '50%' }}
            >
              <div className="absolute -top-1 left-1/2 -translate-x-1/2 w-2 h-2 rounded-full bg-accent shadow-[0_0_6px_rgba(0,212,255,0.6)]" />
            </motion.div>
          )}

          {/* Center Cap */}
          <div className="absolute w-5 h-5 sm:w-6 sm:h-6 rounded-full bg-dark-700 border border-white/10 z-20 shadow-lg flex items-center justify-center">
            <div className="w-1.5 h-1.5 sm:w-2 sm:h-2 rounded-full bg-accent/30" />
          </div>

          {/* Digital Readout */}
          <div
            className="absolute flex flex-col items-center"
            style={{ bottom: 'var(--gauge-readout-bottom)' }}
          >
            <span
              className="text-text-primary font-mono font-bold leading-none tracking-tighter"
              style={{ fontSize: 'var(--gauge-readout-size)' }}
            >
              {Math.round(value)}
            </span>
            <span className="text-text-muted text-[5px] sm:text-[9px] font-medium uppercase tracking-[0.2em] hidden sm:block">{unit}</span>
          </div>
        </div>
      </div>
    </div>
  );
};
