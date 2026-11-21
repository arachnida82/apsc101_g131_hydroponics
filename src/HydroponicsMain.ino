/*
  Team 131 Automated Hydroponic Nutrient Balance System

  Stage order:
    1. Nutrients : read TDS, dose with the auger, mix, re-read, repeat
    2. pH        : measure pH, dose NaHCO3 if too acidic, mix, re-measure
    3. Capsule   : fill growth tank through the resin capsule, soak, return, compare TDS
    Gearbox / mixer: gearbox.ino works out the motor settings once at start-up,
                     then nutrients and pH call mixerRun() for each mixing period.

  Dependencies: AFMotor, Servo, Adafruit_GFX, Adafruit_SSD1306
*/

#include <math.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins.h"
#include "config.h"

/* ---------- Shared OLED (128x64, I2C 0x3C) ---------- */
Adafruit_SSD1306 display(128, 64, &Wire, -1);

/* ---------- Stage sequencing ---------- */
enum Stage { STAGE_NUTRIENTS, STAGE_PH, STAGE_CAPSULE, STAGE_DONE, STAGE_FAULT };
Stage stage = STAGE_NUTRIENTS;
const char *faultStage = "";   // name of the stage that failed (shown on the OLED)

/* ---------- Show one or two lines on the OLED (and Serial) ---------- */
void showMessage(const char line1[], const char line2[])
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  display.println(line2);
  display.display();

  Serial.print(line1);
  Serial.print(" ");
  Serial.println(line2);
}

/* ---------- Show a label and a number on the OLED (and Serial) ---------- */
void showNumber(const char label[], float value)
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(label);
  display.println(value, 1);
  display.display();

  Serial.print(label);
  Serial.print(" ");
  Serial.println(value, 1);
}

float readTDS()
{
  long total = 0;
  int i = 0;

  do { // average the readings 
    total = total + analogRead(TDS_PIN);
    delay(50);
    i++;
  } while (i < NUM_READINGS);

  float average = (float)total / NUM_READINGS;
  float voltage = average * VREF / 1023.0;

  /* temperature compensation (2% per degree from 25 C) */
  float coefficient = 1.0 + 0.02 * (WATER_TEMP - 25.0);
  float v = voltage / coefficient;

  /* convert voltage to ppm  */
  float tds = (133.42 * pow(v, 3) - 255.86 * pow(v, 2) + 857.39 * v) * 0.5;

  return tds; // in ppm
}

/* ---------- Turn everything off (used on a fault and when finished) ---------- */
void allOff()
{
  digitalWrite(PH_PUMP_PIN, LOW);
  digitalWrite(SUB_PUMP_PIN, LOW);
  digitalWrite(DIA_PUMP_PIN, LOW);
  augerStop();   // ntr_dis.ino
  mixerStop();   // gearbox.ino
}

/* ---------- Stop everything and remember which stage failed ---------- */
void enterFault(const char *where)
{
  allOff();
  faultStage = where;
  stage = STAGE_FAULT;
  Serial.print("FAULT in stage: ");
  Serial.println(where);
}

void setup()
{
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
  }

  /* each init sets its own pins to a safe (off) state */
  nutrientsInit();
  phInit();
  capsuleInit();
  bool gearboxOk = gearboxInit();

  allOff();
  showMessage("System", "Starting");
  delay(2000);

  /* gearbox model says the motor cannot drive the mixer: do not start the cycle */
  if (!gearboxOk) {
    enterFault("Gearbox");
  }
}

void loop()
{
  switch (stage) {

    case STAGE_NUTRIENTS:
      showMessage("Stage 1", "Nutrients");
      if (runNutrientStage()) {
        Serial.println("Nutrients done -> pH stage");
        stage = STAGE_PH;
      } else {
        enterFault("Nutrients");
      }
      break;

    case STAGE_PH:
      showMessage("Stage 2", "pH");
      if (runPhStage()) {
        Serial.println("pH done -> capsule stage");
        stage = STAGE_CAPSULE;
      } else {
        enterFault("pH");
      }
      break;

    case STAGE_CAPSULE:
      showMessage("Stage 3", "Capsule");
      if (runCapsuleStage()) {
        Serial.println("Capsule done -> cycle complete");
        stage = STAGE_DONE;
      } else {
        enterFault("Capsule");
      }
      break;

    case STAGE_DONE:
      allOff();
      showMessage("Cycle", "complete");
      delay(5000);
      break;

    case STAGE_FAULT:
      allOff();
      showMessage("FAULT", faultStage);
      delay(2000);
      break;
  }
}
