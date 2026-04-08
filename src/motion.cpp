#include "motion.h"
#include "log_buffer.h"

MotionController motion;

// ─────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────

void MotionController::init() {
  // Enable stepper drivers immediately
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // LOW = enabled on A4988
  _motorsEnabled = true;

  // Apply direction inversions from settings
  _leftMotor.setPinsInverted(settings.invert_left_dir, false, false);
  _rightMotor.setPinsInverted(settings.invert_right_dir, false, false);

  // Hardware speed ceiling — actual per-move speed is set in _startNextSegment
  _leftMotor.setMaxSpeed(20000);
  _rightMotor.setMaxSpeed(20000);

  // Register both motors with the coordinator
  _steppers.addStepper(_leftMotor);
  _steppers.addStepper(_rightMotor);

  // Restore last known position, or default to center of draw area
  if (settings.last_left_cable > 0.1f && settings.last_right_cable > 0.1f) {
    Serial.println("[Motion] Restoring persisted position");
    setCableLengths(settings.last_left_cable, settings.last_right_cable);
  } else {
    float cx = settings.anchor_width_mm / 2.0f;
    float cy = settings.draw_height_mm / 2.0f;
    Serial.println("[Motion] No persistence — defaulting to center");
    setPenPosition(cx, cy);
  }

  // Planning head starts at current position
  _planX = _penX;
  _planY = _penY;

  Serial.printf("[Motion] Init OK. Pen: (%.1f, %.1f) mm\n", _penX, _penY);
  Serial.printf("[Motion] Cables: L=%.1f  R=%.1f mm\n", _leftCable, _rightCable);
}

// ─────────────────────────────────────────────────────────
// update() — call every loop() iteration, no blocking
// ─────────────────────────────────────────────────────────

void MotionController::update() {
  if (_state == MOTION_PAUSED)
    return;

  if (_state == MOTION_RUNNING) {
    // MultiStepper::run() advances both motors one step each if due.
    // Returns true while either motor still has distance to go.
    if (_steppers.run())
      return;

    // Both motors reached their targets — segment complete.
    _state = MOTION_IDLE;
    syncPosition(); // confirm _penX/_penY/_leftCable/_rightCable from actual steps
    _persistenceDirty = true;
  }

  // Pop next segment from queue
  if (!_queueEmpty()) {
    _startNextSegment();
    return;
  }

  // Auto-save position after 10 s of idle
  if (_persistenceDirty) {
    unsigned long now = millis();
    if (now - _lastAutoSave > 10000) {
      _lastAutoSave = now;
      _persistenceDirty = false;
      settings.last_pen_x = _penX;
      settings.last_pen_y = _penY;
      settings.last_left_cable = _leftCable;
      settings.last_right_cable = _rightCable;
      settings.save();
      Serial.println("[Motion] Position saved");
    }
  } else {
    _lastAutoSave = millis();
  }
}

// ─────────────────────────────────────────────────────────
// _startNextSegment — pop one segment and hand it to MultiStepper
// ─────────────────────────────────────────────────────────

void MotionController::_startNextSegment() {
  const Segment &seg = _queue[_tail];
  _tail = (_tail + 1) % QUEUE_SIZE;

  long pos[2];
  pos[0] = Kinematics::mmToSteps(seg.leftLen, settings.steps_per_mm);
  pos[1] = Kinematics::mmToSteps(seg.rightLen, settings.steps_per_mm);

  // MultiStepper scales both motors' speeds so they arrive simultaneously.
  // setMaxSpeed on each stepper sets the upper bound for that motor.
  float speed_steps = (seg.feedRate / 60.0f) * settings.steps_per_mm;
  speed_steps = constrain(speed_steps, 10.0f, 20000.0f);
  _leftMotor.setMaxSpeed(speed_steps);
  _rightMotor.setMaxSpeed(speed_steps);

  _steppers.moveTo(pos);
  _state = MOTION_RUNNING;

  // Update logical position eagerly — pen is "heading here"
  _penX = seg.targetX;
  _penY = seg.targetY;
  _leftCable = seg.leftLen;
  _rightCable = seg.rightLen;
}

// ─────────────────────────────────────────────────────────
// queueMove — segment a Cartesian line and push to queue
// ─────────────────────────────────────────────────────────

bool MotionController::queueMove(float x, float y, float feedRate, bool rapid) {
  float effectiveFeed = rapid ? settings.max_speed_mm_min : feedRate;

  float startX = _planX;
  float startY = _planY;
  float dx = x - startX;
  float dy = y - startY;
  float dist = sqrtf(dx * dx + dy * dy);

  if (dist < 0.05f) {
    // Too small to bother — just update plan position
    _planX = x;
    _planY = y;
    return true;
  }

  int segments = max(1, (int)ceilf(dist / settings.segment_length_mm));
  float segDx = dx / segments;
  float segDy = dy / segments;

  for (int i = 0; i < segments; i++) {
    // Backpressure: yield until there's room (1 ms at a time)
    while (_queueFull())
      delay(1);

    float tx = startX + segDx * (i + 1);
    float ty = startY + segDy * (i + 1);

    float leftLen, rightLen;
    Kinematics::cartesianToLengths(tx, ty + settings.pen_offset_y_mm,
                                   settings.anchor_width_mm,
                                   settings.gondola_width_mm,
                                   leftLen, rightLen);

    Segment seg;
    seg.leftLen  = leftLen;
    seg.rightLen = rightLen;
    seg.feedRate = effectiveFeed;
    seg.targetX  = tx;
    seg.targetY  = ty;

    _queue[_head] = seg;
    _head = (_head + 1) % QUEUE_SIZE;
  }

  _planX = x;
  _planY = y;
  return true;
}

// ─────────────────────────────────────────────────────────
// Public move API
// ─────────────────────────────────────────────────────────

void MotionController::moveTo(float x, float y, float feedRate_mm_min) {
  queueMove(x, y, feedRate_mm_min, false);
}

void MotionController::moveToRapid(float x, float y) {
  queueMove(x, y, settings.max_speed_mm_min, true);
}

bool MotionController::isBusy() const {
  return !_queueEmpty() || _state == MOTION_RUNNING;
}

void MotionController::stop() {
  clearQueue();
  _leftMotor.stop();
  _rightMotor.stop();
  _state = MOTION_IDLE;
}

void MotionController::emergencyStop() {
  clearQueue();
  // Immediately zero velocity — do NOT call runToPosition
  _leftMotor.setSpeed(0);
  _rightMotor.setSpeed(0);
  _leftMotor.stop();
  _rightMotor.stop();
  _state = MOTION_IDLE;
  syncPosition();
  Serial.println("[Motion] E-STOP");
}

void MotionController::pause() {
  _state = MOTION_PAUSED;
}

void MotionController::resume() {
  if (_state == MOTION_PAUSED)
    _state = MOTION_IDLE;
}

void MotionController::clearQueue() {
  _head = _tail = 0;
  // Reset planning head to actual position
  _planX = _penX;
  _planY = _penY;
}

int MotionController::getQueueSpace() const {
  return (QUEUE_SIZE - 1) - ((_head - _tail + QUEUE_SIZE) % QUEUE_SIZE);
}

// ─────────────────────────────────────────────────────────
// releaseString — blocking, used during calibration
// ─────────────────────────────────────────────────────────

void MotionController::releaseString(float mm) {
  // Wait for any queued moves to drain first
  while (isBusy())
    delay(1);

  float speed_steps = (settings.max_speed_mm_min / 60.0f) * settings.steps_per_mm;
  speed_steps = constrain(speed_steps, 10.0f, 10000.0f);

  _leftMotor.setMaxSpeed(speed_steps);
  _rightMotor.setMaxSpeed(speed_steps);

  long deltaSteps = Kinematics::mmToSteps(mm, settings.steps_per_mm);
  long leftTarget  = _leftMotor.currentPosition() + deltaSteps;
  long rightTarget = _rightMotor.currentPosition() + deltaSteps;

  long pos[2] = {leftTarget, rightTarget};
  _steppers.moveTo(pos);

  // Blocking run — safe here because it's calibration, not drawing
  while (_steppers.run())
    ;

  _leftCable  += mm;
  _rightCable += mm;
  syncPosition();
}

// ─────────────────────────────────────────────────────────
// Position management
// ─────────────────────────────────────────────────────────

void MotionController::setCableLengths(float left, float right) {
  _leftCable  = left;
  _rightCable = right;
  _leftMotor.setCurrentPosition(Kinematics::mmToSteps(left,  settings.steps_per_mm));
  _rightMotor.setCurrentPosition(Kinematics::mmToSteps(right, settings.steps_per_mm));

  float x, y;
  if (Kinematics::lengthsToCartesian(left, right, settings.anchor_width_mm,
                                     settings.gondola_width_mm, x, y)) {
    _penX = x;
    _penY = y - settings.pen_offset_y_mm;
  }
  _planX = _penX;
  _planY = _penY;
}

void MotionController::setPenPosition(float x, float y) {
  float l, r;
  Kinematics::cartesianToLengths(x, y + settings.pen_offset_y_mm,
                                  settings.anchor_width_mm,
                                  settings.gondola_width_mm, l, r);
  setCableLengths(l, r);
}

void MotionController::syncPosition() {
  float lMm = Kinematics::stepsToMm(_leftMotor.currentPosition(),  settings.steps_per_mm);
  float rMm = Kinematics::stepsToMm(_rightMotor.currentPosition(), settings.steps_per_mm);
  _leftCable  = lMm;
  _rightCable = rMm;

  float x, y;
  if (Kinematics::lengthsToCartesian(lMm, rMm, settings.anchor_width_mm,
                                     settings.gondola_width_mm, x, y)) {
    _penX = x;
    _penY = y - settings.pen_offset_y_mm;
  }
}

float MotionController::getLivePenX() { return _penX; }
float MotionController::getLivePenY() { return _penY; }
long  MotionController::getLeftSteps()  { return _leftMotor.currentPosition(); }
long  MotionController::getRightSteps() { return _rightMotor.currentPosition(); }

// ─────────────────────────────────────────────────────────
// Motor enable / disable
// ─────────────────────────────────────────────────────────

void MotionController::lockMotors() {
  digitalWrite(ENABLE_PIN, LOW);
  _motorsEnabled = true;
}

void MotionController::unlockMotors() {
  _motorsEnabled = false;
}

void MotionController::enableMotors() {
  lockMotors();
}

void MotionController::disableMotors() {
  // Physically release motors so the gondola can be moved by hand
  digitalWrite(ENABLE_PIN, HIGH);
  _motorsEnabled = false;
}

// ─────────────────────────────────────────────────────────
// Jog helpers
// ─────────────────────────────────────────────────────────

void MotionController::jogMotors(float leftMm, float rightMm, float speed_mm_min) {
  // Independent cable jog: compute resulting Cartesian position and queue it
  float newLeft  = _leftCable  + leftMm;
  float newRight = _rightCable + rightMm;
  float x, y;
  if (Kinematics::lengthsToCartesian(newLeft, newRight, settings.anchor_width_mm,
                                     settings.gondola_width_mm, x, y)) {
    float feed = speed_mm_min > 0 ? speed_mm_min : settings.max_speed_mm_min;
    queueMove(x, y - settings.pen_offset_y_mm, feed);
  }
}

void MotionController::jogCartesian(float dx, float dy, float speed_mm_min) {
  float feed = speed_mm_min > 0 ? speed_mm_min : settings.max_speed_mm_min;
  queueMove(_penX + dx, _penY + dy, feed);
}

void MotionController::driveXY(float vx, float vy) {
  // Velocity jog: replace the current jog target with a fresh move.
  // vx / vy are in mm/s. We queue 0.5 s worth of travel each call.
  // The WebUI should call this at ~5-10 Hz while the joystick is held.
  float speed_mm_s = sqrtf(vx * vx + vy * vy);
  if (speed_mm_s < 0.1f) {
    stop();
    return;
  }

  float speed_mm_min = constrain(speed_mm_s * 60.0f, 60.0f, settings.max_speed_mm_min);
  float dt = 0.5f; // seconds of lookahead
  float tx = _penX + vx * dt;
  float ty = _penY + vy * dt;

  // Clear any stale jog segments so direction changes feel responsive
  clearQueue();
  queueMove(tx, ty, speed_mm_min);
}
