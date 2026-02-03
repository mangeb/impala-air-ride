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
  tankMaint?: TankMaintStatus;
  simLeak?: SimLeakStatus;
}

export interface LeakStatus {
  valid: boolean;
  elapsed?: number;      // seconds since snapshot
  snapshot?: number[];   // [FL, FR, RL, RR, Tank]
  current?: number[];    // [FL, FR, RL, RR, Tank]
  rates?: number[];      // PSI/hr drop rate (positive = losing pressure)
  status?: number[];     // 0=ok, 1=warn, 2=leak
}

export interface TankMaintStatus {
  valid: boolean;
  lastService?: number;    // epoch seconds
  due?: boolean;           // true if overdue
  daysRemaining?: number;  // positive = days left, negative = days overdue
  timeSynced?: boolean;    // whether ESP32 has time sync
}

export interface SimLeakStatus {
  active: boolean;
  target: number;          // -1=none, 0=FL, 1=FR, 2=RL, 3=RR, 4=tank
  targetName?: string;     // "FL", "FR", "RL", "RR", "TANK"
  rate?: number;
}
