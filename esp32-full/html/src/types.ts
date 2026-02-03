export type Corner = 'FL' | 'FR' | 'RL' | 'RR';

export interface PressureData {
  FL: number;
  FR: number;
  RL: number;
  RR: number;
  tank: number;
}

export interface TargetData {
  FL: number;
  FR: number;
  RL: number;
  RR: number;
}

export interface SystemState {
  pressures: PressureData;
  targets: TargetData;
  compressorActive: boolean;
  connected: boolean;
  pumpEnabled?: boolean;
  level?: number;  // 0=off, 1=front, 2=rear, 3=all
  presets?: TargetData[];
}

export interface LeakStatus {
  valid: boolean;
  elapsed?: number;      // seconds since snapshot
  snapshot?: number[];   // [FL, FR, RL, RR, Tank]
  current?: number[];    // [FL, FR, RL, RR, Tank]
  rates?: number[];      // PSI/hr drop rate (positive = losing pressure)
  status?: number[];     // 0=ok, 1=warn, 2=leak
}
