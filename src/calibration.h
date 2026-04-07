#pragma once
#include <Arduino.h>

enum CalibrationStep {
  CAL_IDLE,           // 0 - Not calibrating
  CAL_WAIT_RETRACTED, // 1 - Waiting for user to confirm strings retracted
  CAL_RELEASING,      // 2 - Releasing string (non-blocking)
  CAL_WAIT_WIDTH,     // 3 - Waiting for user to input anchor width
  CAL_COMPLETE        // 4 - Done
};

class Calibration {
public:
  void init();
  void update(); // Call every loop() — handles async release

  // Wizard steps
  void start();
  void confirmRetracted(float release_mm = -1);
  void setAnchorWidth(float width_mm);
  void cancel();

  // Manual cable control (any time)
  void setLengths(float left, float right);
  void manualRelease(float mm);
  void manualRetract(float mm);

  CalibrationStep getStep() const { return _step; }
  const char *getStepDescription() const;

private:
  CalibrationStep _step = CAL_IDLE;
  bool _releasing = false;
};

extern Calibration calibration;
