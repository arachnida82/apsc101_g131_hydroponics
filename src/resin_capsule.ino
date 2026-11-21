/*
  Team 131 Resin Capsule Stage (integrated version)
  Follows the capsule software flowchart:
  read C_before -> fill through capsule -> soak -> return -> read C_after -> calculate drop

  Hardware:
    Submersible pump (mixing tank -> capsule -> growth tank) on SUB_PUMP_PIN
    Diaphragm pump (growth tank -> mixing tank) on DIA_PUMP_PIN
    TDS sensor on TDS_PIN, water level sensor on LEVEL_PIN   (all in pins.h)
    128x64 OLED on I2C (set up in the main file)

  runCapsuleStage() returns true when the stage finished, false if a pump safety timer fired.
  readTDS(), showMessage() and showNumber() now live in the main file.
*/

#include <math.h>

/* ---------- Constants ---------- */
const int   FULL_LEVEL    = 500;    // level reading when the tank is full (calibrate) ( placeholder for now)
const int   EMPTY_LEVEL   = 100;    // level reading when the tank is empty (calibrate) ( placeholder for now)
const int   SOAK_TIME     = 120;    // soak time in seconds (Section 5.3)
const unsigned long MAX_PUMP_TIME = 40000UL;  // safety stop in ms (65 mL at 2.5 mL/s is about 26 s)

void capsuleInit()
{
  pinMode(SUB_PUMP_PIN, OUTPUT);
  pinMode(DIA_PUMP_PIN, OUTPUT);
  digitalWrite(SUB_PUMP_PIN, LOW);
  digitalWrite(DIA_PUMP_PIN, LOW);
}

/* ---------- Fill the growth tank through the capsule ---------- */
/* Pumps until the level sensor reads full. Returns false if the safety timer fires first. */
bool fillGrowthTank()
{
  unsigned long start = millis();

  digitalWrite(SUB_PUMP_PIN, HIGH);

  while (analogRead(LEVEL_PIN) < FULL_LEVEL) {
    if (millis() - start >= MAX_PUMP_TIME) {
      digitalWrite(SUB_PUMP_PIN, LOW);
      Serial.println("ERROR: fill stopped by safety timer (tank never read full)");
      return false;
    }
    delay(100);
  }

  digitalWrite(SUB_PUMP_PIN, LOW);
  return true;
}

/* ---------- Soak with a countdown on the OLED ---------- */
void soak()
{
  int secondsLeft;

  for (secondsLeft = SOAK_TIME; secondsLeft > 0; secondsLeft--) {
    showNumber("Soaking", secondsLeft);
    delay(1000);
  }
}

/* ---------- Return the water to the mixing tank ---------- */
/* Pumps until the level sensor reads empty. Returns false if the safety timer fires first. */
bool returnToMixingTank()
{
  unsigned long start = millis();

  digitalWrite(DIA_PUMP_PIN, HIGH);

  while (analogRead(LEVEL_PIN) > EMPTY_LEVEL) {
    if (millis() - start >= MAX_PUMP_TIME) {
      digitalWrite(DIA_PUMP_PIN, LOW);
      Serial.println("ERROR: return stopped by safety timer (tank never read empty)");
      return false;
    }
    delay(100);
  }

  digitalWrite(DIA_PUMP_PIN, LOW);
  return true;
}

/* ---------- The capsule stage ---------- */
bool runCapsuleStage()
{
  float cBefore, cAfter, percentDrop, k;

  /* START */
  showMessage("Capsule", "Starting");
  delay(2000);

  /* read C_before */
  cBefore = readTDS();
  showNumber("Before:", cBefore);
  delay(2000);

  /* fill through the capsule */
  showMessage("Filling", "");
  if (!fillGrowthTank()) return false;

  /* soak */
  soak();

  /* return */
  showMessage("Returning", "");
  if (!returnToMixingTank()) return false;

  /* read C_after */
  delay(2000);   // let the probe settle
  cAfter = readTDS();
  showNumber("After:", cAfter);
  delay(2000);

  /* calculate the drop */
  if (cBefore > 0) {
    percentDrop = (cBefore - cAfter) / cBefore * 100.0;
  } else {
    percentDrop = 0;
  }

  /* first order model from Section 5.3: k = -ln(C_after / C_before) / t */
  if (cBefore > 0 && cAfter > 0) {
    k = -log(cAfter / cBefore) / SOAK_TIME;
  } else {
    k = 0;
  }

  Serial.print("Percent drop: ");
  Serial.println(percentDrop, 1);
  Serial.print("k (1/s): ");
  Serial.println(k, 5);

  showNumber("Drop %:", percentDrop);
  delay(2000);
  /* END */
  return true;
}