#pragma once
#include "config.h"
#include "kinematics.h"
#include "settings.h"
#include <AccelStepper.h>
#include <MultiStepper.h>
#include <Arduino.h>

enum MotionState {
  MOTION_IDLE,
  MOTION_RUNNING,
  MOTION_PAUSED,
  MOTION_DRIVING // reserved for future velocity jog
};

class MotionController {
public:
  void init();
  void update(); // MUST be called every loop() iteration

  // Queue a move to Cartesian position (mm). Segments internally.
  void moveTo(float x, float y, float feedRate_mm_min);
  void moveToRapid(float x, float y);

  // Calibration: release / retract cable on both motors simultaneously (blocking)
  void releaseString(float mm);

  // Manual jog
  void jogMotors(float leftMm, float rightMm, float speed_mm_min = 0);
  void jogCartesian(float dx, float dy, float speed_mm_min = 0);
  // Velocity jog (mm/s): queues a short move in the velocity direction.
  // Call repeatedly from the WebUI joystick to maintain motion.
  void driveXY(float vx, float vy);

  // Motor enable / disable
  void lockMotors();
  void unlockMotors();
  void enableMotors();
  void disableMotors();
  bool areMotorsEnabled() const { return _motorsEnabled; }

  // Stop
  void emergencyStop();
  void stop();

  // State
  bool isBusy() const;
  MotionState getState() const { return _state; }
  void pause();
  void resume();

  // Logical position — returns the END of the last queued move (planning tip).
  // Use this for GCode coordinate math; it chains consecutive moves correctly
  // even when pipelining (parsing ahead before execution finishes).
  float getPenX() const { return _planX; }
  float getPenY() const { return _planY; }
  // Physical position — reads back from actual stepper steps.
  float getLivePenX();
  float getLivePenY();
  long getLeftSteps();
  long getRightSteps();
  float getLeftCableLength() const { return _leftCable; }
  float getRightCableLength() const { return _rightCable; }

  // Set logical position without moving motors
  void setPenPosition(float x, float y);
  void setCableLengths(float left, float right);
  void syncPosition();

  // Queue inspection
  bool queueMove(float x, float y, float feedRate, bool rapid = false);
  bool isQueueFull() const { return _queueFull(); }
  bool isQueueEmpty() const { return _queueEmpty(); }
  int getQueueSpace() const;
  void clearQueue();

private:
  // Two steppers, coordinated via MultiStepper
  AccelStepper _leftMotor{AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN};
  AccelStepper _rightMotor{AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN};
  MultiStepper _steppers;

  MotionState _state = MOTION_IDLE;
  bool _motorsEnabled = false;

  // Logical position (pen tip, drawing coordinates)
  float _penX = 0.0f;
  float _penY = 0.0f;
  float _leftCable = 0.0f;
  float _rightCable = 0.0f;

  // Pre-computed segment queue (cable lengths ready to execute)
  struct Segment {
    float leftLen;   // target left cable length (mm)
    float rightLen;  // target right cable length (mm)
    float feedRate;  // mm/min
    float targetX;   // drawing-space target (no pen offset)
    float targetY;
  };

  static const int QUEUE_SIZE = 64;
  Segment _queue[QUEUE_SIZE];
  int _head = 0; // next write slot
  int _tail = 0; // next read slot

  // Planning position — tracks the END of the last queued move.
  // Used by queueMove() so consecutive calls chain correctly even
  // before execution starts.
  float _planX = 0.0f;
  float _planY = 0.0f;

  bool _persistenceDirty = false;
  unsigned long _lastAutoSave = 0;

  bool _queueFull() const { return ((_head + 1) % QUEUE_SIZE) == _tail; }
  bool _queueEmpty() const { return _head == _tail; }

  // Load the next segment from the queue into MultiStepper
  void _startNextSegment();
};

extern MotionController motion;
