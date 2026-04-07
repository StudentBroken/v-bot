#pragma once
#include "config.h"
#include "kinematics.h"
#include "settings.h"
#include <AccelStepper.h>
#include <Arduino.h>

enum MotionState {
  MOTION_IDLE,
  MOTION_RUNNING,
  MOTION_PAUSED,
  MOTION_DRIVING // Continuous velocity control
};

class MotionController {
public:
  void init();
  void update(); // Call every loop iteration

  // Move pen to absolute Cartesian position (mm)
  void moveTo(float x, float y, float feedRate_mm_min);
  void moveToRapid(float x, float y);

  // Release a specific length of cable (mm) on both motors — for calibration
  void releaseString(float mm);

  // Manual jog individual motors
  void jogMotors(float leftMm, float rightMm, float speed_mm_min = 0);
  // Manual jog in Cartesian space
  void jogCartesian(float dx, float dy, float speed_mm_min = 0);

  // Lock motors (hold current position with torque)
  void lockMotors();
  void unlockMotors();
  void enableMotors();
  void disableMotors();
  bool areMotorsEnabled() const { return _motorsEnabled; }

  // Stop immediately
  void emergencyStop();
  // Smooth stop (decelerate)
  void stop();

  // State
  bool isBusy() const;
  MotionState getState() const { return _state; }
  void pause();
  void resume();

  // Position
  float getPenX() const { return _penX; }
  float getPenY() const { return _penY; }
  // Live position from current steps (Forward Kinematics)
  float getLivePenX();
  float getLivePenY();
  long getLeftSteps();
  long getRightSteps();

  // MovementPenPosition(float x, float y);

  void setPenPosition(float x, float y);

  // Force logical position to match physical
  // Velocity Control (Continuous Jog)
  void driveXY(float vx, float vy);

  // Getters
  // Getters
  float getLeftCableLength() const { return _leftCable; }
  float getRightCableLength() const { return _rightCable; }

  // Cable lengths (for calibration)
  void setCableLengths(float left, float right);

  void syncPosition();

private:
  AccelStepper _leftMotor{AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN};
  AccelStepper _rightMotor{AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN};

  // State
  volatile MotionState _state = MOTION_IDLE;
  bool _motorsEnabled = false;
  unsigned long _lastDriveTime = 0; // Watchdog for drive commands

  // Current logical position
  float _penX = 0.0f;
  float _penY = 0.0f;
  float _leftCable = 0.0f;
  float _rightCable = 0.0f;

  // Velocity Control (Smooth Ramp)
  float _targetVx = 0.0f;
  float _targetVy = 0.0f;
  float _currentVx = 0.0f;
  float _currentVy = 0.0f;
  unsigned long _lastLoopTime = 0;

  void _moveMotorsToLengths(float leftLen, float rightLen,
                            float feedRate_mm_min);
  void _updateSpeedForFeedrate(float feedRate_mm_min);

  // Motion Queue
  struct MoveCmd {
    float x, y, feedRate;
    bool rapid;
  };
  // Simple circular buffer or just array
  static const int QUEUE_SIZE = 32;
  MoveCmd _queue[QUEUE_SIZE];
  int _queueHead = 0;
  int _queueTail = 0;
  bool _abort = false;

  // Persistence (Auto-Save)
  bool _persistenceDirty = false;
  unsigned long _lastAutoSave = 0;

public:
  bool queueMove(float x, float y, float feedRate, bool rapid = false);
  bool isQueueFull() const;
  bool isQueueEmpty() const;
  int getQueueSpace() const;
};

extern MotionController motion;
