// pH control for mix tank - NaHCO3 dosing (integrated version)
// Based on notes dated Oct 26, 2026
// !! PROVISIONAL: the pH lead will replace this file. Keep these two names and rules:
// !!     void phInit();      bool runPhStage();   (true = pH in range, false = failed)
// !! Every value marked CONFIG must be replaced with your real value !!
//
// Changes from the standalone sketch (so the lead can see what moved):
//   - pins now come from pins.h (PUMP_PIN is now PH_PUMP_PIN, PH_SENSOR_PIN is in pins.h)
//   - TANK_VOLUME_L removed: uses TANK_L from config.h
//   - MIX_TIME_MS removed: uses the shared 10 minute MIX_TIME_MS from config.h, run with mixerRun()
//   - setup() is now phInit(); loop() is now runPhStage(), which RETURNS instead of looping forever
//   - "halt forever" on too many pulses is now return false
//   - NEW: a pH reading outside a believable range returns false (a disconnected probe
//          can read as "in range" otherwise)

// ---------- CONFIG: hardware ----------
const float ADC_REF_V   = 5.0;     // Arduino Mega: 0-5 V, 10-bit ADC
const int   ADC_MAX     = 1023;    // 1023 = 10-bit; use 4095 for 12-bit boards

// ---------- CONFIG: calibration (from the sachets) ----------
// Run pH_A-Calibration.ino in each buffer and replace these with YOUR readings.
// Placeholders below are the example numbers from the course pH_B sketch.
// NOTE: this sensor's voltage goes DOWN as pH goes UP, so V_ACIDIC > V_NEUTRAL
// and the slope comes out negative. That is expected and the math handles it.
const float V_NEUTRAL = 1.540;     // v7: volts in pH 7.00 buffer
const float V_ACIDIC  = 2.093;     // v4: volts in pH 4.01 buffer
const float PH_NEUTRAL = 7.00;
const float PH_ACIDIC  = 4.01;

// ---------- CONFIG: dosing ----------
const float NAHCO3_CONC_MOL_L = 0.05;  // concentration of dosing solution (mol/L)
const float PUMP_FLOW_ML_S  = 5.0;     // pump flow rate in mL per second (measure this!)

// ---------- pH limits (from notes) ----------
const float PH_LOW_LIMIT  = 5.8;   // below this -> dose
const float PH_HIGH_OK    = 6.2;   // top of target band
const float PH_TARGET     = 6.0;   // what we aim for when dosing (middle of band)
const float PH_MAX_SAFE   = 6.5;   // never dose if we'd go past this

// ---------- timing / safety ----------
const unsigned long MAX_PULSE_MS = 10000UL;             // cap on any single pulse
const int MAX_PULSES_IN_ROW = 5;                        // stop if pH won't come into range
const float PH_SENSOR_MIN = 2.0;   // readings outside this range mean the probe is faulty
const float PH_SENSOR_MAX = 12.0;

// ---------- derived ----------
float slope_m;   // pH units per volt

// ---------- functions ----------
float readVoltage() {
  // average 30 readings to reduce noise (same as course calibration sketch)
  long sum = 0;
  for (int i = 0; i < 30; i++) {
    sum += analogRead(PH_SENSOR_PIN);
    delay(10);
  }
  return (sum / 30.0) * ADC_REF_V / ADC_MAX;
}

// pH_tank = 7.00 + m * (V_tank - V_neutral)
float readPH() {
  float v = readVoltage();
  return PH_NEUTRAL + slope_m * (v - V_NEUTRAL);
}

// returns pulse length in ms needed to bring pH down to PH_TARGET
unsigned long calcPulseMs(float currentPH) {
  float hCurrent = pow(10.0, -currentPH);        // [H+] now, mol/L
  float hTarget  = pow(10.0, -PH_TARGET);        // [H+] target, mol/L
  float molesAcid = (hCurrent - hTarget) * TANK_L;   // mol H+ to neutralize
  if (molesAcid <= 0) return 0;

  // 1 mol HCO3- neutralizes 1 mol H+  ->  V = n / C
  float vRequiredL = molesAcid / NAHCO3_CONC_MOL_L;
  float pulseMs = (vRequiredL * 1000.0 / PUMP_FLOW_ML_S) * 1000.0;

  if (pulseMs > MAX_PULSE_MS) pulseMs = MAX_PULSE_MS;
  return (unsigned long)pulseMs;
}

void phInit() {
  pinMode(PH_PUMP_PIN, OUTPUT);
  digitalWrite(PH_PUMP_PIN, LOW);

  // m = (7.00 - 4.01) / (V_neutral - V_acidic)
  slope_m = (PH_NEUTRAL - PH_ACIDIC) / (V_NEUTRAL - V_ACIDIC);
  Serial.print("Slope m = ");
  Serial.println(slope_m, 3);
}

// Returns true when pH is in range, false if it cannot be brought into range.
bool runPhStage() {
  int pulsesInRow = 0;

  while (true) {
    // 1) Measure pH
    float ph = readPH();
    showNumber("pH:", ph);

    if (ph < PH_SENSOR_MIN || ph > PH_SENSOR_MAX) {
      Serial.println("ERROR: pH reading not believable - check the probe");
      return false;
    }

    if (ph >= PH_LOW_LIMIT) {
      // in range (or above) - stage finished
      if (ph > PH_MAX_SAFE) Serial.println("WARNING: pH above safe max!");
      return true;
    }

    // too acidic -> dose
    if (pulsesInRow >= MAX_PULSES_IN_ROW) {
      Serial.println("ERROR: pH not reaching range after max pulses.");
      return false;   // main sketch turns everything off; check system manually
    }

    // 2) Execute a pulse that adds the needed volume
    unsigned long pulseMs = calcPulseMs(ph);
    Serial.print("Pulse ms = ");
    Serial.println(pulseMs);
    if (pulseMs > 0) {
      showMessage("Dosing", "NaHCO3");
      digitalWrite(PH_PUMP_PIN, HIGH);
      delay(pulseMs);
      digitalWrite(PH_PUMP_PIN, LOW);
      pulsesInRow++;
    }

    // 3) Mix period - let the 0.05 M solution fully combine with the tank
    mixerRun(MIX_TIME_MS);

    // 4) loop around -> re-measure and repeat as needed
  }
}