/*
 * Nutrient Dispenser (augerscrewv13, integrated version)
 *
 * What it does (runNutrientStage):
 *   1. Reads the TDS of the mixing tank.
 *   2. If TDS is below target, works out how many grams of nutrient to add
 *      and turns the auger (SG90 continuous-rotation servo) for that long.
 *   3. Reverses the auger a quarter turn so grains don't dribble out.
 *   4. Runs the mixer for MIX_TIME_MS, re-reads TDS, and repeats until on target.
 *      If TDS didn't rise (jam / empty bottle) it clears the auger and tries again,
 *      but gives up after MAX_NUTRIENT_ATTEMPTS and returns false.
 *
 * Shared things this file uses: readTDS(), showMessage(), showNumber() (main file),
 * mixerRun() (gearbox.ino), TANK_L and MIX_TIME_MS (config.h), AUGER_PIN (pins.h).
 */

#include <Servo.h>

Servo auger;

const float TDS_TARGET   = 600.0;   // TODO placeholder: ppm wanted in the mixing tank (set from your sensor / plant needs)
const float TDS_TOL      = 50.0;    // stop when within +/- this many ppm
const float PPM_PER_GPL  = 500.0;   // ppm rise per 1 g of nutrient per litre CALIBRATE

// Auger model: ~0.29 g per turn ~50 rpm means ~0.24 g/s
// so ~4100 ms of turning per gram CALIBRATE
const float MS_PER_GRAM  = 4100.0;
const int   QUARTER_MS   = 300;     // time for ~1/4 turn at ~50 rpm (anti-leak reverse)

const int   STOP_US      = 1500;    // servo stopped ADJUST
const int   FWD_US       = 1600;    // slow forward (~50 rpm) - pushes nutrient out
const int   REV_US       = 1400;    // slow reverse

const float DOSE_FRACTION = 0.8;    // add 80% of the estimate each time to avoid overshoot
const float MIN_DOSE_G    = 0.1;    // smallest dose worth turning the auger for
const float MIN_RISE_PPM  = 10.0;   // smaller rise than this = nothing came out
const int   MAX_NUTRIENT_ATTEMPTS = 8;   // NEW: give up after this many doses (jam / empty bottle)

void nutrientsInit()
{
    auger.attach(AUGER_PIN);
    auger.writeMicroseconds(STOP_US);
}

// Stop the auger (also called by allOff() in the main file)
void augerStop()
{
    auger.writeMicroseconds(STOP_US);
}

// Turn the auger forward for a given number of grams, then pull back 1/4 turn
void runAuger(float grams)
{
    auger.writeMicroseconds(FWD_US);
    delay((unsigned long)(grams * MS_PER_GRAM));
    auger.writeMicroseconds(REV_US);     // anti-leak: pull grains back from the outlet
    delay(QUARTER_MS);
    auger.writeMicroseconds(STOP_US);
}

// Returns true when TDS is within tolerance of the target, false if it never gets there.
bool runNutrientStage()
{
    float tdsNow = readTDS();
    showNumber("Start TDS:", tdsNow);
    delay(2000);

    int attempts = 0;

    while (tdsNow < TDS_TARGET - TDS_TOL) {
        if (attempts >= MAX_NUTRIENT_ATTEMPTS) {
            Serial.println("ERROR: TDS not reaching target after max doses (jam or empty bottle?)");
            return false;
        }
        attempts++;

        // grams needed = ppm short x litres / (ppm per g/L)
        float grams = (TDS_TARGET - tdsNow) * TANK_L / PPM_PER_GPL * DOSE_FRACTION;
        if (grams < MIN_DOSE_G) grams = MIN_DOSE_G;

        Serial.print("Dosing "); Serial.print(grams, 2); Serial.println(" g");
        showMessage("Dosing", "nutrients");
        runAuger(grams);

        mixerRun(MIX_TIME_MS);            // mix and dissolve (10 min), then let the water settle
        float tdsNew = readTDS();
        showNumber("TDS:", tdsNew);

        if (tdsNew - tdsNow < MIN_RISE_PPM) {
            // Nothing came out (jam or empty bottle): back off half a turn and try again
            Serial.println("No rise - clearing auger and retrying");
            auger.writeMicroseconds(REV_US);
            delay(2 * QUARTER_MS);
            auger.writeMicroseconds(STOP_US);
        }
        tdsNow = tdsNew;
    }

    if (tdsNow > TDS_TARGET + TDS_TOL) {
        Serial.println("WARNING: TDS is above target (nutrients cannot be removed)");
    }

    showNumber("TDS done:", tdsNow);
    Serial.println("Nutrients done");
    delay(2000);
    return true;
}
