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
}
