#include "calibration.h"
#include "kinematics.h"
#include "motion.h"
#include "settings.h"

Calibration calibration;

void Calibration::init() { _step = CAL_IDLE; }

void Calibration::start() {
  _step = CAL_WAIT_RETRACTED;
  _releasing = false;
  motion.disableMotors(); // Allow manual retraction
  Serial.println(
      "[Cal] Calibration started - Motors disabled for manual retraction");
  Serial.println(
      "[Cal] Step 1: Manually retract both strings fully, then confirm");
}

void Calibration::confirmRetracted(float release_mm) {
  if (_step != CAL_WAIT_RETRACTED)
    return;

  Serial.println("[Cal] Step 2: Strings retracted confirmed, zeroing motors");

  // Zero both motor positions — this is our reference point
  motion.setCableLengths(0, 0);
  motion.enableMotors(); // Ensure motors are enabled for release

  float mm = (release_mm > 0) ? release_mm : settings.calibration_release_mm;

  // Start releasing string (non-blocking — release happens via update())
  Serial.printf("[Cal] Releasing %.0fmm of string...\n", mm);

  _step = CAL_RELEASING;
  _releasing = true;

  // Trigger the release — this is non-blocking
  motion.releaseString(mm);

  // State is now CAL_RELEASING. update() handles the transition when motion
  // stops.
}

void Calibration::update() {
  if (_step == CAL_RELEASING) {
    // If release motion finished
    if (!motion.isBusy()) {
      _releasing = false;
      motion.lockMotors();
      _step = CAL_WAIT_WIDTH;

      Serial.println("[Cal] Step 3: Motors locked. Position anchors on wall.");
      Serial.println("[Cal] Measure anchor spacing and enter width.");
    }
  }
}

void Calibration::setAnchorWidth(float width_mm) {
  if (_step != CAL_WAIT_WIDTH)
    return;

  settings.anchor_width_mm = width_mm;
  settings.save();

  // Both cables are the released length
  float cableLen = settings.calibration_release_mm;
  motion.setCableLengths(cableLen, cableLen);

  // Pen position is computed by forward kinematics inside setCableLengths
  Serial.printf("[Cal] Anchor width set to %.0fmm\n", width_mm);
  Serial.printf("[Cal] Pen position: (%.1f, %.1f)\n", motion.getPenX(),
                motion.getPenY());

  _step = CAL_COMPLETE;
  Serial.println("[Cal] Calibration complete!");
}

void Calibration::cancel() {
  _step = CAL_IDLE;
  _releasing = false;
  Serial.println("[Cal] Calibration cancelled");
}

void Calibration::manualRelease(float mm) {
  Serial.printf("[Cal] Manual release %.1fmm\n", mm);
  motion.releaseString(mm);
}

void Calibration::manualRetract(float mm) {
  Serial.printf("[Cal] Manual retract %.1fmm\n", mm);
  motion.releaseString(-mm);
}

void Calibration::setLengths(float left, float right) {
  Serial.printf("[Cal] Manual set lengths: L=%.1f R=%.1f\n", left, right);
  motion.setCableLengths(left, right);
  // Ensure we are not in a wizard state
  _step = CAL_IDLE;
  _releasing = false;
  motion.lockMotors(); // Engage motors to hold this position
}

const char *Calibration::getStepDescription() const {
  switch (_step) {
  case CAL_IDLE:
    return "Not calibrating";
  case CAL_WAIT_RETRACTED:
    return "Manually retract both strings fully, then press Confirm";
  case CAL_RELEASING:
    return "Releasing string...";
  case CAL_WAIT_WIDTH:
    return "Position anchors on wall, measure spacing, enter width";
  case CAL_COMPLETE:
    return "Calibration complete!";
  default:
    return "Unknown";
  }
}
