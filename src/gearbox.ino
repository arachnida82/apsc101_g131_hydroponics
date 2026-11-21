/*
  Team 131 Gearbox Model + Mixer control (integrated version)

  What it does:
    gearboxInit()  - runs once at start-up. Works out the gear ratio and efficiency,
                     uses the motor's speed-torque line to predict torque, speed and
                     power under the mixer load, prints the results, and saves the
                     motor power setting for later ("save instructions for motor").
                     Returns false if the motor cannot handle the load.
    mixerRun(ms)   - runs the mixer motor at the saved power setting for ms milliseconds,
                     then stops it. Nutrients and pH call this for every mixing period.
    mixerStop()    - releases the motor (also called by allOff() in the main file).

  Hardware: Adafruit Motor Shield (AFMotor library), DC motor on port MIXER_MOTOR_PORT (pins.h).
*/

#include <AFMotor.h>

/* ---------- Motor and shield ---------- */
AF_DCMotor motor(MIXER_MOTOR_PORT);

const int  MIN_PWM     = 100;      // lowest power setting that starts the gearbox (0-255)
const int  MAX_PWM     = 255;      // full power setting

const float STALL_TORQUE_GCM = 340.0;    // motor stall torque at full voltage (g*cm)
const float NO_LOAD_RPM      = 14500.0;  // motor no-load speed at full voltage (rpm)
const float SUPPLY_VOLTAGE   = 6.0;      // motor supply voltage (V), for reference

/* ---------- Gearbox ---------- */
// Layout: worm -> 40T gear. A 10T gear on the same dowel as the 40T
// drives the 50T gear, which sits on its own (mixer) dowel.
const int   WORM_STARTS      = 1;    // threads on the worm
const int   WORM_WHEEL_TEETH = 40;   // gear driven by the worm
const int   PINION_TEETH     = 10;   // small spur gear, same dowel as the 40T (driver)
const int   SPUR_GEAR_TEETH  = 50;   // large spur gear on the mixer dowel (driven)
const float WORM_EFFICIENCY  = 0.50; // worm stage efficiency
const float SPUR_EFFICIENCY  = 0.80; // spur stage efficiency (given value)

/* ---------- Load and targets ---------- */
const float LOAD_TORQUE_MNM   = 80.0; // torque the mixer needs at the output (mN*m), given value
const float MIN_SAFETY_FACTOR = 2.0;  // warn if stall torque / needed torque is below this

// Output speed  (rpm).
// Set to 0 to run at full power
const float TARGET_OUTPUT_RPM = 0.0;

// true = print every intermediate value, false = print only the main results
const bool SHOW_DETAILS = true;

/* ---------- Unit conversions ---------- */
const float GCM_TO_MNM  = 0.0980665;   // 1 g*cm = 0.0980665 mN*m
const float RPM_TO_RADS = 0.10471976;  // 1 rpm = 2*pi/60 rad/s
const float MNM_TO_NM   = 0.001;       // 1 mN*m = 0.001 N*m

/* ---------- Saved by gearboxInit(), used by mixerRun() ---------- */
int  mixerPwm   = 0;       // power setting to run the mixer at (0-255)
bool mixerReady = false;   // true only if the model says the motor can drive the load


/* Print one labelled value, e.g. "Output speed: 70.32 rpm" */
void showValue(const char label[], float value, const char unit[])
{
  Serial.print(label);
  Serial.print(" ");
  Serial.print(value, 2);
  Serial.print(" ");
  Serial.println(unit);
}

void showLine()
{
  Serial.println("----------------------------------------");
}

/*
  Power setting (0-255) needed for the motor to turn at targetRpm
  while carrying motorTorque.

  DC motor model: lowering the power setting to a fraction d of full
  scales both the no-load speed and the stall torque by d:
      speed = d * NO_LOAD_RPM - (NO_LOAD_RPM / stallTorque) * torque
  Solving for d:
      d = targetRpm / NO_LOAD_RPM + torque / stallTorque
  This is an estimate: the shield loses some voltage, so the real speed
  will be a bit lower.
*/
int pwmForSpeed(float targetRpm, float motorTorque, float stallTorque)
{
  float d = targetRpm / NO_LOAD_RPM + motorTorque / stallTorque;
  int pwm = (int)(MAX_PWM * d + 0.5);   // round to nearest whole number
  return constrain(pwm, 0, MAX_PWM);
}

/* Release the motor (safe to call at any time) */
void mixerStop()
{
  motor.run(RELEASE);
}

/* Run the mixer at the saved power setting for ms milliseconds, then stop and let the water settle. */
void mixerRun(unsigned long ms)
{
  if (!mixerReady) {
    Serial.println("Mixer not ready - skipping mix");
    return;
  }

  showMessage("Mixing", "");
  Serial.print("Mixer power setting: ");
  Serial.print(mixerPwm);
  Serial.print(" / 255 for ");
  Serial.print(ms / 1000UL);
  Serial.println(" s");

  motor.setSpeed(mixerPwm);
  motor.run(FORWARD);
  delay(ms);
  mixerStop();

  delay(SETTLE_MS);   // let the water go still before a probe is read
}

/* Runs once from setup(). Returns true if the motor can drive the mixer. */
bool gearboxInit()
{
  mixerStop();                           // make sure the motor starts off

  Serial.println("Team 131 Gearbox Model");
  showLine();

  /* ----- Motor values in mN*m ----- */
  float stallTorque = STALL_TORQUE_GCM * GCM_TO_MNM;      // tau_stall (mN*m)

  /* ----- Gear ratio and efficiency ----- */
  float wormRatio  = (float)WORM_WHEEL_TEETH / WORM_STARTS;
  float spurRatio  = (float)SPUR_GEAR_TEETH / PINION_TEETH;
  float ratio      = wormRatio * spurRatio;                // overall R
  float efficiency = WORM_EFFICIENCY * SPUR_EFFICIENCY;    // overall eta

  /* ----- Motor torque needed: tau1 = tauL / (R * eta) ----- */
  float motorTorque = LOAD_TORQUE_MNM / (ratio * efficiency);
  float safetyFactor = stallTorque / motorTorque;

  /* ----- Speed at full power: omega1 = omega_nl * (1 - tau1 / tau_stall) ----- */
  float fullPowerMotorRpm  = NO_LOAD_RPM * (1.0 - motorTorque / stallTorque);
  float fullPowerOutputRpm = fullPowerMotorRpm / ratio;

  /* ----- Choose the operating speed ----- */
  float outputSpeed;                      // omega2 (rpm)
  if (TARGET_OUTPUT_RPM > 0.0) {
    outputSpeed = TARGET_OUTPUT_RPM;
  } else {
    outputSpeed = fullPowerOutputRpm;
  }
  float motorSpeed = outputSpeed * ratio; // omega1 = omega2 * R (rpm)
  int   pwm = pwmForSpeed(motorSpeed, motorTorque, stallTorque);

  /* ----- Shaft after the worm stage ----- */
  float midSpeed  = motorSpeed / wormRatio;
  float midTorque = motorTorque * wormRatio * WORM_EFFICIENCY;

  /* ----- Output torque: tau2 = tau1 * R * eta ----- */
  float outputTorque = motorTorque * ratio * efficiency;

  /* ----- Power: P = torque (N*m) * speed (rad/s) ----- */
  float motorPower  = (motorTorque  * MNM_TO_NM) * (motorSpeed  * RPM_TO_RADS);
  float outputPower = (outputTorque * MNM_TO_NM) * (outputSpeed * RPM_TO_RADS);

  /* ----- Print results ----- */
  if (SHOW_DETAILS) {
    Serial.println("Inputs");
    showValue("Stall torque:", stallTorque, "mNm");
    showValue("No-load speed:", NO_LOAD_RPM, "rpm");
    showValue("Supply voltage:", SUPPLY_VOLTAGE, "V");
    showValue("Load torque:", LOAD_TORQUE_MNM, "mNm");
    showLine();
    Serial.println("Gearbox");
    showValue("Worm ratio:", wormRatio, ": 1");
    showValue("Spur ratio:", spurRatio, ": 1");
    showValue("Overall ratio:", ratio, ": 1");
    showValue("Overall efficiency:", efficiency * 100.0, "%");
    showLine();
  }

  Serial.println("Predicted performance");
  showValue("Motor torque needed:", motorTorque, "mNm");
  showValue("Safety factor (stall / needed):", safetyFactor, "");
  showValue("Max output speed (full power):", fullPowerOutputRpm, "rpm");
  showValue("Motor speed:", motorSpeed, "rpm");
  if (SHOW_DETAILS) {
    showValue("Intermediate shaft torque:", midTorque, "mNm");
    showValue("Intermediate shaft speed:", midSpeed, "rpm");
  }
  showValue("Output torque:", outputTorque, "mNm");
  showValue("Output speed:", outputSpeed, "rpm");
  showValue("Motor power:", motorPower, "W");
  showValue("Output power:", outputPower, "W");
  showLine();

  /* ----- Is torque below stall torque? (flowchart decision) ----- */
  bool ok = true;

  if (motorTorque >= stallTorque) {
    Serial.println("STOP: load needs more torque than the motor's stall torque.");
    ok = false;
  } else if (TARGET_OUTPUT_RPM > fullPowerOutputRpm) {
    Serial.println("STOP: target speed is faster than the motor can reach with this load.");
    ok = false;
  } else if (pwm < MIN_PWM) {
    Serial.println("STOP: target speed needs a power setting below MIN_PWM,");
    Serial.println("      so the gearbox may not start. Raise TARGET_OUTPUT_RPM.");
    ok = false;
  }

  if (ok && safetyFactor < MIN_SAFETY_FACTOR) {
    Serial.println("WARNING: safety factor is low, motor is close to stalling.");
  }

  /* ----- Save the instructions for mixerRun() ----- */
  mixerPwm   = pwm;
  mixerReady = ok;

  if (!ok) {
    Serial.println("Mixer disabled.");
  }
  return ok;
}
