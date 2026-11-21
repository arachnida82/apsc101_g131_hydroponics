#pragma once

#ifndef CONFIG_H
#define CONFIG_H

/* ---------- Mixing tank ---------- */
const float TANK_L = 0.500;   // litres of water in the mixing tank 

/* ---------- Mixing ---------- */
const unsigned long MIX_TIME_MS = 10UL * 60UL * 1000UL;  // 10 minutes of mixing after every dose (nutrients and pH)
const unsigned long SETTLE_MS   = 5000UL;                // let the water go still after the motor stops, before probes are read

/* ---------- TDS reading ---------- */
const int   NUM_READINGS = 10;     // TDS readings averaged together
const float VREF         = 5.0;    // Arduino Mega voltage
const float WATER_TEMP   = 25.0;   // water temperature in degrees C

#endif