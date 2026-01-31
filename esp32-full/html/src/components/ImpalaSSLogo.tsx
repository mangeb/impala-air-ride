import React from 'react';

export const ImpalaSSLogo: React.FC = () => {
  return (
    <div className="flex items-center bg-[#f0f0f0] border-[3px] border-red-800 p-1 shadow-[0_4px_6px_rgba(0,0,0,0.4)] relative overflow-hidden group">
      {/* Chrome sheen overlay */}
      <div className="absolute inset-0 bg-gradient-to-b from-white/60 via-transparent to-black/10 pointer-events-none" />
      
      {/* Inner white border for that stamped metal look */}
      <div className="absolute inset-[1px] border border-white/40 pointer-events-none" />

      <div className="flex items-center gap-4 px-3 relative z-10">
        <span className="text-black font-sans font-black text-[12px] tracking-[0.8em] uppercase leading-none drop-shadow-sm">
          Impala
        </span>
        
        <div className="flex items-center -ml-1">
          <span className="text-red-700 font-serif italic text-4xl font-black leading-none drop-shadow-[2px_2px_0px_rgba(255,255,255,0.8)] tracking-tighter filter brightness-90">
            SS
          </span>
        </div>
      </div>
    </div>
  );
};
