#include "motor_controller.h"
#include "config.h"

// ============================
// Motor pin sequence lookup
// ============================
// Each row: LF, LB, RF, RB
static const uint8_t MOTOR_SEQ[][4] = {
  {0,0,0,0}, // M_STOP
  {1,0,1,0}, // M_FORWARD  (LF+RF = 两轮前进)
  {0,1,0,1}, // M_BACKWARD (LB+RB = 两轮后退)
  {0,1,1,0}, // M_LEFT     (LB+RF = 左退右进)
  {1,0,0,1}, // M_RIGHT    (LF+RB = 左进右退)
  {0,1,0,0}, // M_LEFT_BWD  (左轮后退)
  {0,0,0,1}, // M_RIGHT_BWD (右轮后退)
  {1,0,0,0}, // M_LEFT_FWD  (左轮前进)
  {0,0,1,0}, // M_RIGHT_FWD (右轮前进)
};

void MotorController::begin() {
  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, LOW);

  // 配置 4 路 PWM
  ledcSetup(PWM_CH_LF, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_LB, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_RF, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_RB, PWM_FREQ, PWM_RES);

  ledcAttachPin(PIN_LF, PWM_CH_LF);
  ledcAttachPin(PIN_LB, PWM_CH_LB);
  ledcAttachPin(PIN_RF, PWM_CH_RF);
  ledcAttachPin(PIN_RB, PWM_CH_RB);

  stop();
  digitalWrite(PIN_STBY, HIGH);
}

void MotorController::stop() {
  _brake();
  _running = false;
  _currentCmd = M_STOP;
}

void MotorController::execute(MotorCmd cmd, uint8_t speed, uint32_t duration_ms) {
  // 限制时长
  if (duration_ms > MOTOR_MAX_DURATION_MS) {
    duration_ms = MOTOR_MAX_DURATION_MS;
  }

  const uint8_t *seq = MOTOR_SEQ[cmd];
  _softStartInit(seq[0], seq[1], seq[2], seq[3], speed);

  _running = true;
  _startTime = millis();
  _duration = duration_ms;
  _currentCmd = cmd;
  _currentSpeed = speed;
}

void MotorController::update() {
  // 软启动渐变（非阻塞）
  if (_ramping) {
    _softStartTick();
  }

  if (!_running) return;

  uint32_t elapsed = millis() - _startTime;
  if (elapsed >= _duration) {
    _brake();
    _running = false;
  }
}

void MotorController::setMotor(uint8_t lf, uint8_t lb, uint8_t rf, uint8_t rb, uint8_t speed) {
  _drivePins(lf, lb, rf, rb, speed);
  _running = false; // raw control has no timeout
}

// ---- Private ----

void MotorController::_drivePins(uint8_t lf, uint8_t lb, uint8_t rf, uint8_t rb, uint8_t speed) {
  ledcWrite(PWM_CH_LF, lf ? speed : 0);
  ledcWrite(PWM_CH_LB, lb ? speed : 0);
  ledcWrite(PWM_CH_RF, rf ? speed : 0);
  ledcWrite(PWM_CH_RB, rb ? speed : 0);
}

void MotorController::_softStartInit(uint8_t lf, uint8_t lb, uint8_t rf, uint8_t rb, uint8_t target) {
  _rampLf = lf; _rampLb = lb; _rampRf = rf; _rampRb = rb;
  _rampCur = 30;  // 起始速度
  _rampTarget = target;
  _ramping = true;
  _rampLastTick = millis();
  _drivePins(lf, lb, rf, rb, _rampCur);
}

void MotorController::_softStartTick() {
  unsigned long now = millis();
  if (now - _rampLastTick < (MOTOR_RAMP_MS / 10)) return;
  _rampLastTick = now;

  if (_rampCur >= _rampTarget) {
    _ramping = false;
    return;
  }

  _rampCur += 25;
  if (_rampCur > _rampTarget) _rampCur = _rampTarget;
  _drivePins(_rampLf, _rampLb, _rampRf, _rampRb, _rampCur);
}

void MotorController::_brake() {
  _ramping = false;
  ledcWrite(PWM_CH_LF, 0);
  ledcWrite(PWM_CH_LB, 0);
  ledcWrite(PWM_CH_RF, 0);
  ledcWrite(PWM_CH_RB, 0);
}
