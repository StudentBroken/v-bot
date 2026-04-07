#include "motion.h"
#include "log_buffer.h"

MotionController motion;

// ── FreeRTOS Task Wrapper ──
void motionTask(void *parameter) {
  while (true) {
    motion._taskWork();
    // Yield to avoid watchdog trigger if idle (though _taskWork handles
    // waiting)
    vTaskDelay(1);
  }
}

void MotionController::init() {
  // Setup enable pin
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // FORCE ENABLE IMMEDIATELY
  _motorsEnabled = true;

  // Max speeds for the steppers (hardware limits, not move limits)
  // We set high max speed here, actual speed is controlled by runSpeed()
  float maxStepsPerSec = 20000.0f;
  _leftMotor.setMaxSpeed(maxStepsPerSec);
  _rightMotor.setMaxSpeed(maxStepsPerSec);

  // Acceleration is not used with runSpeed(), but good to set defaults
  _leftMotor.setAcceleration(settings.acceleration * settings.steps_per_mm);
  _rightMotor.setAcceleration(settings.acceleration * settings.steps_per_mm);

  _leftMotor.setPinsInverted(settings.invert_left_dir, false, false);
  _rightMotor.setPinsInverted(settings.invert_right_dir, false, false);

  // Restore position
  float centerX = settings.anchor_width_mm / 2.0f;
  float centerY = settings.draw_height_mm / 2.0f;

  if (settings.last_left_cable > 0.1f && settings.last_right_cable > 0.1f) {
    Serial.println("[Motion] Restoring persisted position...");
    setCableLengths(settings.last_left_cable, settings.last_right_cable);
  } else if (centerX > 100 && centerY > 100) {
    Serial.println("[Motion] No persistence, setting to Center...");
    setPenPosition(centerX, centerY);
  }

  Serial.println("[Motion] Initialized & Motors ENABLED");
  Serial.printf("[Motion] Boot Position: (%.1f, %.1f)\n", _penX, _penY);
  syncPosition();
  _persistenceDirty = false;

  // ── Start Motion Task ──
  xTaskCreatePinnedToCore(motionTask,    // Function
                          "MotionTask",  // Name
                          8192,          // Stack size
                          NULL,          // Param
                          MOT_TASK_PRIO, // Priority
                          &_taskHandle,  // Handle
                          MOT_TASK_CORE  // Core
  );
  Serial.printf("[Motion] Task started on Core %d\n", MOT_TASK_CORE);
}

// ── Main Loop Update ──
void MotionController::update() {
  // This runs on the MAIN task/core.
  // It handles persistence and status, but NOT step generation.

  unsigned long now = millis();

  // Auto-Save Persistence
  if (_state == MOTION_IDLE && _persistenceDirty) {
    if (now - _lastAutoSave > 10000) { // 10 seconds idle
      _lastAutoSave = now;
      _persistenceDirty = false;

      // Atomic read of position/cables?
      // Since we are IDLE, values should be stable.
      settings.last_pen_x = _penX;
      settings.last_pen_y = _penY;
      settings.last_left_cable = _leftCable;
      settings.last_right_cable = _rightCable;

      Serial.println("[Motion] Auto-saving persisted position...");
      settings.save();
    }
  } else {
    _lastAutoSave = now;
  }
}

// ── Task Worker (The Engine) ──
void MotionController::_taskWork() {
  if (!_taskShouldRun) {
    vTaskDelay(10);
    return;
  }

  // 1. Check for velocity override (Jogging)
  if (_state == MOTION_DRIVING) {
    // Implement velocity control here in the task?
    // Or just run existing non-blocking logic?
    // For now, let's keep velocity logic simple or implement later.
    // Ideally, we convert velocity to short segments and queue them?
    // No, direct driving is better for low latency.

    // TODO: Move velocity driving logic here.
    // For now, fallback to IDLE if not driving.
  }

  // 2. Process Queue
  if (_state == MOTION_RUNNING || _state == MOTION_IDLE) {
    MotionCommand cmd;
    if (_popCommand(cmd)) {
      _state = MOTION_RUNNING;
      _processCommand(cmd);
    } else {
      _state = MOTION_IDLE;
      // Idle wait
      vTaskDelay(1);
    }
  }
}

void MotionController::_processCommand(const MotionCommand &cmd) {
  if (cmd.type == MotionCommand::DWELL) {
    vTaskDelay(cmd.param1);
    return;
  }

  if (cmd.type == MotionCommand::STOP) {
    _leftMotor.stop();
    _rightMotor.stop();
    return;
  }

  if (cmd.type == MotionCommand::MOVE) {
    // Interpolate? No, the queue *contains* segments.
    // The "queueMove" function handles interpolation and pushes segments.
    // So here we just execute the segment.

    float targetX = cmd.x;
    float targetY = cmd.y;

    // Calculate lengths
    float leftLen, rightLen;
    Kinematics::cartesianToLengths(
        targetX, targetY + settings.pen_offset_y_mm, settings.anchor_width_mm,
        settings.gondola_width_mm, leftLen, rightLen);

    // Coordinate move
    _moveMotorsToLengths(leftLen, rightLen, cmd.feedRate);

    // Update internal state
    _penX = targetX;
    _penY = targetY;
    _leftCable = leftLen;
    _rightCable = rightLen;
    _persistenceDirty = true;
  }
}

void MotionController::_moveMotorsToLengths(float leftLen, float rightLen,
                                            float feedRate_mm_min) {
  long leftSteps = Kinematics::mmToSteps(leftLen, settings.steps_per_mm);
  long rightSteps = Kinematics::mmToSteps(rightLen, settings.steps_per_mm);

  if (leftSteps == _leftMotor.currentPosition() &&
      rightSteps == _rightMotor.currentPosition()) {
    return;
  }

  _leftMotor.moveTo(leftSteps);
  _rightMotor.moveTo(rightSteps);

  _updateSpeedForFeedrate(feedRate_mm_min);

  // BLOCKING RUN (inside Task)
  while (_leftMotor.distanceToGo() != 0 || _rightMotor.distanceToGo() != 0) {
    if (_abort) { // Emergency stop flag
      break;
    }
    _leftMotor.runSpeedToPosition();
    _rightMotor.runSpeedToPosition();
  }
}

void MotionController::_updateSpeedForFeedrate(float feedRate_mm_min) {
  float speed_mm_s = feedRate_mm_min / 60.0f;
  float speed_steps = speed_mm_s * settings.steps_per_mm;

  long leftDist = _leftMotor.distanceToGo();
  long rightDist = _rightMotor.distanceToGo();
  long maxDist = max(abs(leftDist), abs(rightDist));

  if (maxDist > 0) {
    float leftRatio = max(0.01f, (float)abs(leftDist) / maxDist);
    float rightRatio = max(0.01f, (float)abs(rightDist) / maxDist);

    float sL = speed_steps * leftRatio;
    float sR = speed_steps * rightRatio;

    // Apply signs
    if (leftDist < 0)
      sL = -sL;
    if (rightDist < 0)
      sR = -sR;

    _leftMotor.setSpeed(sL); // setSpeed sets the speed for runSpeed()
    _rightMotor.setSpeed(sR);
  }
}

// ── Queue Management ──

bool MotionController::queueMove(float x, float y, float feedRate, bool rapid) {
  // Segmentation Logic (Moved from moveTo)
  float dx = x - _penX; // Use _penX (logical pos) which tracks queue tip if we
                        // manage it correctly
  // Wait, _penX tracks *executed* position in the task.
  // Ideally, queueMove should track the "planned" position.
  // For simplicity, let's assume we segment based on the LAST QUEUED position?
  // Or just rely on small segments being passed in?
  //
  // Standard Approach:
  // The caller (GCodeParser) often sends lines. We break lines into segments
  // here. But we need to know where we are starting from. Let's add a `_planX`,
  // `_planY` to track the planning head.

  // Actually, to avoid complexity, let's just use _penX/_penY for now, BUT this
  // ignores queued moves. FIX: We need `_planX` and `_planY` initialized to
  // _penX/_penY. Since we are rewriting, let's add static variables or just use
  // the current class members IF we update them immediately after queueing? NO,
  // `_penX` is updated when the task *executes*. So we MUST track
  // `_planX`/`_planY` separately.

  // Hack for now: Use `_penX` but this assumes queue is empty? No that's bad.
  // Let's assume the GCode parser sends small segments?
  // No, G0 X100 is one command.

  // Okay, let's implement local planning variables.
  // But wait, `moveTo` in the old code did segmentation.
  // We should do segmentation HERE, effectively blocking if the queue is full.

  // To properly segment, we need the start point.
  // If the queue is empty, start point is _penX, _penY.
  // If queue is not empty, start point is the target of the last command in
  // queue. This requires reading the tail of the queue.

  float startX, startY;
  if (_isQueueEmpty()) {
    startX = _penX;
    startY = _penY;
  } else {
    // Peek at last added command
    int lastIdx = (_head - 1 + MOT_BUF_SIZE) % MOT_BUF_SIZE;
    startX = _queue[lastIdx].x;
    startY = _queue[lastIdx].y;
  }

  // Calculate Distance
  float dy = y - startY; // Ignoring pen offset here? y is target Y?
  // In `moveTo`, `y` was passed. `settings.pen_offset_y_mm` was added inside.
  // Let's stay consistent. The arguments x, y are "Drawing Coordinates".

  float dx = x - startX;
  float dist = sqrtf(dx * dx + dy * dy);

  if (dist < 0.05f)
    return true; // Too small

  int segments = (int)(dist / settings.segment_length_mm);
  if (segments < 1)
    segments = 1;

  float segDx = dx / segments;
  float segDy = dy / segments;

  float curX = startX;
  float curY = startY;

  for (int i = 0; i < segments; i++) {
    curX += segDx;
    curY += segDy;

    MotionCommand cmd;
    cmd.type = MotionCommand::MOVE;
    cmd.x = curX;
    cmd.y = curY;
    cmd.feedRate = feedRate;
    cmd.param1 = 0;

    // SPINFULLY WAIT if queue is full
    while (_isQueueFull()) {
      vTaskDelay(1); // Block planner (Main Task) until space frees up
    }

    _pushCommand(cmd);
  }
  return true;
}

bool MotionController::_isQueueFull() const {
  int nextHead = (_head + 1) % MOT_BUF_SIZE;
  return (nextHead == _tail);
}

bool MotionController::_isQueueEmpty() const { return (_head == _tail); }

void MotionController::_pushCommand(const MotionCommand &cmd) {
  _queue[_head] = cmd;
  _head = (_head + 1) % MOT_BUF_SIZE;
}

bool MotionController::_popCommand(MotionCommand &cmd) {
  if (_isQueueEmpty())
    return false;
  cmd = _queue[_tail];
  _tail = (_tail + 1) % MOT_BUF_SIZE;
  return true;
}

void MotionController::clearQueue() { _head = _tail = 0; }

// ── Legacy / Direct Wrappers ──

void MotionController::moveTo(float x, float y, float feedRate_mm_min) {
  queueMove(x, y, feedRate_mm_min);
}

void MotionController::moveToRapid(float x, float y) {
  queueMove(x, y, settings.max_speed_mm_min, true);
}

void MotionController::stop() {
  _abort = true;
  clearQueue();
  // Also stop motors physically
  _leftMotor.stop();
  _rightMotor.stop();
  // Wait for task updates?
  // _abort flag should break the blocking loop in _taskWork
}

void MotionController::emergencyStop() {
  stop();
  _leftMotor.runToPosition();
  _rightMotor.runToPosition();
  Serial.println("[Motion] E-STOP");
}

bool MotionController::isBusy() const {
  return !_isQueueEmpty() || _state == MOTION_RUNNING;
}

void MotionController::pause() {
  // How to pause? We need to stop popping from queue.
  // _state = MOTION_PAUSED;
  // We can just set a flag.
  // NOTE: We don't have a specific Pause flag logic in _taskWork yet.
  // Let's implement it.
  _state = MOTION_PAUSED;
}

void MotionController::resume() {
  if (_state == MOTION_PAUSED) {
    _state = MOTION_IDLE; // Process will pick up
  }
}

// ── Direct / Manual Callbacks ──
// These should ideally bypass queue or lock it.

void MotionController::releaseString(float mm) {
  // This needs to be synchronous or special command?
  // Let's simple queue it as a special move? No, it's specific.
  // Since we are calibrating, we likely aren't printing.
  // We can just execute directly IF queue is empty.

  // SAFETY: Wait for queue empty
  while (!_isQueueEmpty())
    vTaskDelay(10);

  long steps = Kinematics::mmToSteps(mm, settings.steps_per_mm);
  _leftMotor.move(steps);
  _rightMotor.move(steps);

  // Blocking wait (since it's calibration, blocking Main is usually fine)
  // BUT we must ensure the TASK doesn't interfere.
  // Pause the task?
  _taskShouldRun = false;

  float speed_steps =
      (settings.max_speed_mm_min / 60.0f) * settings.steps_per_mm;
  _leftMotor.setMaxSpeed(speed_steps);
  _rightMotor.setMaxSpeed(speed_steps);

  while (_leftMotor.distanceToGo() != 0 || _rightMotor.distanceToGo() != 0) {
    _leftMotor.runSpeedToPosition();
    _rightMotor.runSpeedToPosition();
  }

  _leftCable += mm;
  _rightCable += mm;

  _taskShouldRun = true;
}

void MotionController::setCableLengths(float left, float right) {
  _leftCable = left;
  _rightCable = right;
  _leftMotor.setCurrentPosition(
      Kinematics::mmToSteps(left, settings.steps_per_mm));
  _rightMotor.setCurrentPosition(
      Kinematics::mmToSteps(right, settings.steps_per_mm));
  Kinematics::lengthsToCartesian(left, right, settings.anchor_width_mm,
                                 settings.gondola_width_mm, _penX, _penY);
  _penY -= settings.pen_offset_y_mm;
}

void MotionController::setPenPosition(float x, float y) {
  float l, r;
  Kinematics::cartesianToLengths(x, y + settings.pen_offset_y_mm,
                                 settings.anchor_width_mm,
                                 settings.gondola_width_mm, l, r);
  setCableLengths(l, r);
}

void MotionController::syncPosition() {
  long lSteps = _leftMotor.currentPosition();
  long rSteps = _rightMotor.currentPosition();
  float lMm = Kinematics::stepsToMm(lSteps, settings.steps_per_mm);
  float rMm = Kinematics::stepsToMm(rSteps, settings.steps_per_mm);
  _leftCable = lMm;
  _rightCable = rMm;
  float x, y;
  if (Kinematics::lengthsToCartesian(lMm, rMm, settings.anchor_width_mm,
                                     settings.gondola_width_mm, x, y)) {
    _penX = x;
    _penY = y - settings.pen_offset_y_mm;
  }
}

float MotionController::getLivePenX() { return _penX; } // Simplified
float MotionController::getLivePenY() { return _penY; }

long MotionController::getLeftSteps() { return _leftMotor.currentPosition(); }
long MotionController::getRightSteps() { return _rightMotor.currentPosition(); }

void MotionController::lockMotors() {
  digitalWrite(ENABLE_PIN, LOW);
  _motorsEnabled = true;
}
void MotionController::unlockMotors() {
  // digitalWrite(ENABLE_PIN, HIGH);
  _motorsEnabled = false;
}
void MotionController::enableMotors() { lockMotors(); }
void MotionController::disableMotors() { unlockMotors(); }

void MotionController::jogMotors(float leftMm, float rightMm,
                                 float speed_mm_min) {
  // Direct jog?
  releaseString(leftMm); // Basically same thing
}

void MotionController::jogCartesian(float dx, float dy, float speed_mm_min) {
  queueMove(_penX + dx, _penY + dy,
            speed_mm_min > 0 ? speed_mm_min : settings.max_speed_mm_min);
}

void MotionController::driveXY(float vx, float vy) {
  // TODO: Implement velocity driving
}
