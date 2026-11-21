#ifndef PINS_H

#define PINS_H

/* Analog inputs */
const int TDS_PIN         = A1;   // TDS probe (shared by nutrient and capsule stages)
const int PH_SENSOR_PIN   = A3;   // pH sensor board (matches the course sketch)
const int LEVEL_PIN       = A2;   // growth tank water level sensor

/* Digital outputs */
const int AUGER_PIN       = 9;    // SG90 auger servo, shield servo header
const int PH_PUMP_PIN     = 22;   // NaHCO3 dosing pump (relay / MOSFET)
const int SUB_PUMP_PIN    = 23;   // submersible pump: mixing tank -> capsule -> growth tank
const int DIA_PUMP_PIN    = 24;   // diaphragm pump: growth tank -> mixing tank

/* Motor shield */
const int MIXER_MOTOR_PORT = 1;   // mixer DC motor on shield port M1

#endif


