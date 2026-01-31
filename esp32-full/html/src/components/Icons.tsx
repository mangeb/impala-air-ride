import React from 'react';

export const ImpalaTopDown = ({ className }: { className?: string }) => (
  <svg 
    viewBox="0 0 100 200" 
    className={className} 
    fill="none" 
    stroke="currentColor" 
    strokeWidth="4" 
    strokeLinecap="round" 
    strokeLinejoin="round"
  >
    {/* Main Body Outline */}
    <path d="M30 20 C 30 10, 70 10, 70 20 L 75 60 L 80 160 C 80 180, 20 180, 20 160 L 25 60 Z" />
    {/* Windshield */}
    <path d="M32 65 C 32 55, 68 55, 68 65 L 65 85 C 65 80, 35 80, 35 85 Z" />
    {/* Rear Window */}
    <path d="M36 130 C 36 140, 64 140, 64 130 L 62 120 C 62 125, 38 125, 38 120 Z" />
    {/* Hood Lines */}
    <path d="M50 20 L 50 55" strokeWidth="2" opacity="0.5" />
    <path d="M35 30 L 40 55" strokeWidth="1" opacity="0.3" />
    <path d="M65 30 L 60 55" strokeWidth="1" opacity="0.3" />
    {/* Trunk Lines */}
    <path d="M50 145 L 50 175" strokeWidth="2" opacity="0.5" />
    {/* Bumpers */}
    <path d="M28 15 L 72 15" strokeWidth="6" />
    <path d="M20 175 L 80 175" strokeWidth="6" />
  </svg>
);

export const VintageWheel = ({ className }: { className?: string }) => (
  <svg 
    viewBox="0 0 100 100" 
    className={className} 
    fill="none" 
    stroke="currentColor" 
    strokeWidth="3"
  >
    <circle cx="50" cy="50" r="45" />
    <circle cx="50" cy="50" r="8" fill="currentColor" />
    {/* Spokes */}
    {[...Array(24)].map((_, i) => (
      <line 
        key={i}
        x1="50" 
        y1="50" 
        x2={50 + 40 * Math.cos((i * 15 * Math.PI) / 180)} 
        y2={50 + 40 * Math.sin((i * 15 * Math.PI) / 180)} 
        opacity="0.6"
        strokeWidth="1"
      />
    ))}
    <circle cx="50" cy="50" r="35" strokeWidth="1" opacity="0.4" />
    <circle cx="50" cy="50" r="20" strokeWidth="1" opacity="0.4" />
  </svg>
);
