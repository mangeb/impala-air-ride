// ESP32 serves the page, so use relative URLs (same origin)
// When running in dev mode (Vite), fall back to env var or default IP
const BASE_URL = import.meta.env.DEV
  ? `http://${import.meta.env.VITE_ESP32_IP || '192.168.4.1'}`
  : '';

// Corner name to ESP32 bag index mapping
const CORNER_TO_BAG: Record<string, number> = { FL: 0, FR: 1, RL: 2, RR: 3 };

// Local state for offline simulation (when ESP32 not reachable)
let simulatedState = {
  pressures: { FL: 45, FR: 45, RL: 30, RR: 30, tank: 145 },
  targets: { FL: 45, FR: 45, RL: 30, RR: 30 },
  solenoids: {
    FL: { inflate: false, deflate: false },
    FR: { inflate: false, deflate: false },
    RL: { inflate: false, deflate: false },
    RR: { inflate: false, deflate: false },
  },
  compressorActive: false,
  connected: false
};

// Simulation loop for offline/demo mode (runs at ~60fps)
setInterval(() => {
  if (simulatedState.connected) return; // Skip simulation when ESP32 is live

  // 1. Tank Pressure Decay
  if (simulatedState.pressures.tank > 0) {
    simulatedState.pressures.tank -= 0.01;
  }

  // 2. Dual Pump Logic
  const currentPSI = simulatedState.pressures.tank;
  if (currentPSI >= 150) {
    simulatedState.compressorActive = false;
    simulatedState.pressures.tank = 150;
  } else if (currentPSI < 120 || simulatedState.compressorActive) {
    simulatedState.compressorActive = true;
    const efficiencyFactor = Math.max(0.3, 1 - (currentPSI / 200));
    const pumpCount = currentPSI < 105 ? 2.0 : 1.0;
    const baseFillRate = 0.06;
    const actualFill = baseFillRate * efficiencyFactor * pumpCount;
    simulatedState.pressures.tank = Math.min(150, simulatedState.pressures.tank + actualFill);
    if (simulatedState.pressures.tank >= 150) {
      simulatedState.compressorActive = false;
    }
  }

  // 3. Manual Solenoid Control & Target Tracking
  const corners: (keyof typeof simulatedState.solenoids)[] = ['FL', 'FR', 'RL', 'RR'];
  corners.forEach(corner => {
    const s = simulatedState.solenoids[corner];
    let current = simulatedState.pressures[corner];

    if (s.inflate) {
      const deltaP = Math.max(0, simulatedState.pressures.tank - current);
      const fillSpeed = 0.05 * Math.sqrt(deltaP);
      if (deltaP > 1) {
        simulatedState.pressures[corner] += fillSpeed;
        simulatedState.pressures.tank -= fillSpeed * 0.4;
      }
      simulatedState.targets[corner] = simulatedState.pressures[corner];
    } else if (s.deflate) {
      const dumpSpeed = 0.04 * Math.sqrt(Math.max(0, current));
      simulatedState.pressures[corner] = Math.max(0, current - dumpSpeed);
      simulatedState.targets[corner] = simulatedState.pressures[corner];
    } else {
      const target = simulatedState.targets[corner];
      const diff = target - current;
      if (Math.abs(diff) > 0.3) {
        if (diff > 0) {
          const deltaP = Math.max(0, simulatedState.pressures.tank - current);
          const fillSpeed = 0.02 * Math.sqrt(deltaP);
          if (deltaP > 2) {
            simulatedState.pressures[corner] += fillSpeed;
            simulatedState.pressures.tank -= fillSpeed * 0.4;
          }
        } else {
          const dumpSpeed = 0.015 * Math.sqrt(Math.max(0, current));
          simulatedState.pressures[corner] -= dumpSpeed;
        }
      }
    }

    // Jitter simulation
    simulatedState.pressures[corner] += (Math.random() - 0.5) * 0.005;
  });
}, 16);

// Parse ESP32 /s response into our SystemState format
function parseEsp32Status(data: any) {
  return {
    pressures: {
      FL: data.bags[0],
      FR: data.bags[1],
      RL: data.bags[2],
      RR: data.bags[3],
      tank: data.tank,
    },
    targets: {
      FL: data.targets[0],
      FR: data.targets[1],
      RL: data.targets[2],
      RR: data.targets[3],
    },
    compressorActive: data.pump?.includes('ON') || false,
    connected: true,
    level: data.level || 0,
    lockout: data.lockout || false,
    pumpEnabled: data.pumpEnabled !== false,
    pump: data.pump || '',
    runtime: data.runtime || '',
    maint: data.maint || null,
    maintOverdue: data.maintOverdue || false,
    timeouts: data.timeouts || [false, false, false, false],
    // Custom preset targets from ESP32 EEPROM
    presets: data.presets ? data.presets.map((p: number[]) => ({
      FL: p[0], FR: p[1], RL: p[2], RR: p[3]
    })) : undefined,
  };
}

export const airService = {
  async getStatus() {
    try {
      const response = await fetch(`${BASE_URL}/s`, { signal: AbortSignal.timeout(1000) });
      if (!response.ok) throw new Error('Network response was not ok');
      // Sanitize response: ESP32 may produce "nan"/"inf" for corrupted EEPROM floats
      const text = await response.text();
      const sanitized = text.replace(/\bnan\b/gi, '0').replace(/\b-?inf\b/gi, '0');
      const data = JSON.parse(sanitized);
      simulatedState.connected = true;
      return parseEsp32Status(data);
    } catch (error) {
      simulatedState.connected = false;
      return { ...simulatedState, presets: undefined };
    }
  },

  // Hold-style solenoid control (matches ESP32 /b endpoint)
  async setSolenoid(corner: string, type: 'inflate' | 'deflate', active: boolean) {
    const bagNum = CORNER_TO_BAG[corner];
    if (bagNum === undefined) return;

    if (active) {
      // Start inflate/deflate: GET /b?n=<bag>&d=<dir>&h=1
      const dir = type === 'inflate' ? 1 : -1;
      try {
        await fetch(`${BASE_URL}/b?n=${bagNum}&d=${dir}&h=1`, { signal: AbortSignal.timeout(1000) });
      } catch (e) {
        // Offline simulation
        const c = corner as keyof typeof simulatedState.solenoids;
        if (simulatedState.solenoids[c]) {
          simulatedState.solenoids[c][type] = true;
        }
      }
    } else {
      // Release: GET /bh?n=<bag>
      try {
        await fetch(`${BASE_URL}/bh?n=${bagNum}`, { signal: AbortSignal.timeout(1000) });
      } catch (e) {
        // Offline simulation
        const c = corner as keyof typeof simulatedState.solenoids;
        if (simulatedState.solenoids[c]) {
          simulatedState.solenoids[c].inflate = false;
          simulatedState.solenoids[c].deflate = false;
        }
      }
    }
  },

  // Preset: GET /p?n=<preset>
  async applyPreset(presetNum: number) {
    try {
      await fetch(`${BASE_URL}/p?n=${presetNum}`, { signal: AbortSignal.timeout(1000) });
    } catch (e) {
      // Offline: apply preset values locally
      const presets = [
        { FL: 0, FR: 0, RL: 0, RR: 0 },       // Lay
        { FL: 80, FR: 80, RL: 50, RR: 50 },     // Cruise
        { FL: 100, FR: 100, RL: 80, RR: 80 },   // Max
      ];
      if (presets[presetNum]) {
        simulatedState.targets = { ...presets[presetNum] };
      }
    }
  },

  // Set target for a single corner: uses preset or individual bag control
  async setTarget(corner: string, pressure: number) {
    const bagNum = CORNER_TO_BAG[corner];
    if (bagNum === undefined) return;
    try {
      // Use a custom endpoint for per-corner target setting
      await fetch(`${BASE_URL}/bt?n=${bagNum}&t=${pressure}`, { signal: AbortSignal.timeout(1000) });
    } catch (e) {
      const c = corner as keyof typeof simulatedState.targets;
      if (simulatedState.targets[c] !== undefined) {
        simulatedState.targets[c] = pressure;
      }
    }
  },

  // Level mode: GET /l?m=<mode>
  async setLevelMode(mode: number) {
    try {
      await fetch(`${BASE_URL}/l?m=${mode}`, { signal: AbortSignal.timeout(1000) });
    } catch (e) {
      // Offline: no-op
    }
  },

  // Save current pressures to a preset: GET /sp?n=<preset>&fl=<psi>&fr=<psi>&rl=<psi>&rr=<psi>
  async savePreset(presetNum: number, pressures: { FL: number; FR: number; RL: number; RR: number; tank?: number }) {
    const fl = Math.round(pressures.FL);
    const fr = Math.round(pressures.FR);
    const rl = Math.round(pressures.RL);
    const rr = Math.round(pressures.RR);
    try {
      await fetch(`${BASE_URL}/sp?n=${presetNum}&fl=${fl}&fr=${fr}&rl=${rl}&rr=${rr}`, { signal: AbortSignal.timeout(1000) });
    } catch (e) {
      // Offline: update local preset values
      const presets = [
        { FL: 0, FR: 0, RL: 0, RR: 0 },
        { FL: 80, FR: 80, RL: 50, RR: 50 },
        { FL: 100, FR: 100, RL: 80, RR: 80 },
      ];
      if (presets[presetNum]) {
        presets[presetNum] = { FL: fl, FR: fr, RL: rl, RR: rr };
      }
    }
  },

  // Toggle pump override: GET /po
  async togglePumpOverride() {
    try {
      await fetch(`${BASE_URL}/po`, { signal: AbortSignal.timeout(1000) });
    } catch (e) {
      // Offline: no-op
    }
  },
};
