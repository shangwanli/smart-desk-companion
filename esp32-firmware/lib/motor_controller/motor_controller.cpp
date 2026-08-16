/**
 * 非阻塞电机控制器 — L298N 迷你版 实现
 *
 * 差分驱动运动学 (右电机物理反向安装):
 *   FORWARD:  IN1=1 IN2=0  IN3=0 IN4=1  (左CW + 右CCW = 前进)
 *   BACKWARD: IN1=0 IN2=1  IN3=1 IN4=0  (左CCW + 右CW = 后退)
 *   LEFT:     IN1=0 IN2=1  IN3=0 IN4=1  (左CCW + 右CCW = 左转)
 *   RIGHT:    IN1=1 IN2=0  IN3=1 IN4=0  (左CW + 右CW = 右转)
 */

#include "motor_controller.h"
#include "config.h"

// 方向序列: [IN1, IN2, IN3, IN4]
// 方向序列与原始 jiqiren.ino 完全一致
// 差分驱动：右电机物理反向安装，所以前进时右轮需 CCW
static const uint8_t MOTOR_SEQ[][4] = {
  {0,0,0,0}, // M_STOP
  {1,0,0,1}, // M_FORWARD   (左 CW + 右 CCW = 前进)
  {0,1,1,0}, // M_BACKWARD  (左 CCW + 右 CW = 后退)
  {0,1,0,1}, // M_LEFT      (左 CCW + 右 CCW = 左转)
  {1,0,1,0}, // M_RIGHT     (左 CW + 右 CW = 右转)
  {0,1,0,0}, // M_LEFT_BWD
  {0,0,0,1}, // M_RIGHT_BWD
  {1,0,0,0}, // M_LEFT_FWD
  {0,0,1,0}, // M_RIGHT_FWD
};

void MotorController::begin() {
  // 方向引脚
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  // PWM 通道
  ledcSetup(PWM_CH_ENA, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_ENB, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_ENA, PWM_CH_ENA);
  ledcAttachPin(PIN_ENB, PWM_CH_ENB);

  stop();
}

void MotorController::stop() {
  _brake();
  _running = false;
  _currentCmd = M_STOP;
}

void MotorController::execute(MotorCmd cmd, uint8_t speed, uint32_t duration_ms) {
  if (duration_ms > MOTOR_MAX_DURATION_MS) {
    duration_ms = MOTOR_MAX_DURATION_MS;
  }

  const uint8_t *seq = MOTOR_SEQ[cmd];
  _softStartInit(seq[0], seq[1], seq[2], seq[3], speed, speed);

  _running = true;
  _startTime = millis();
  _duration = duration_ms;
  _currentCmd = cmd;
  _currentSpeed = speed;
}

void MotorController::update() {
  if (_ramping) {
    _softStartTick();
  }

  if (!_running) return;

  if (millis() - _startTime >= _duration) {
    _brake();
    _running = false;
  }
}

void MotorController::setMotor(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4, uint8_t speed) {
  // 直接控制，不走超时（探索模式用）
  _setDirection(in1, in2, in3, in4);
  _setPWM(speed, speed);
  _running = false;
}

// ---- Private ----

void MotorController::_setDirection(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4) {
  digitalWrite(PIN_IN1, in1);
  digitalWrite(PIN_IN2, in2);
  digitalWrite(PIN_IN3, in3);
  digitalWrite(PIN_IN4, in4);
}

void MotorController::_setPWM(uint8_t leftSpeed, uint8_t rightSpeed) {
  ledcWrite(PWM_CH_ENA, leftSpeed);
  ledcWrite(PWM_CH_ENB, rightSpeed);
}

void MotorController::_softStartInit(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4,
                                     uint8_t leftSpeed, uint8_t rightSpeed) {
  _rIn1 = in1; _rIn2 = in2; _rIn3 = in3; _rIn4 = in4;
  _rCur = 30;
  _rTargetL = leftSpeed;
  _rTargetR = rightSpeed;
  _ramping = true;
  _rampLastTick = millis();
  _setDirection(in1, in2, in3, in4);
  _setPWM(30, 30);
}

void MotorController::_softStartTick() {
  if (millis() - _rampLastTick < (MOTOR_RAMP_MS / 10)) return;
  _rampLastTick = millis();

  uint8_t tgt = max(_rTargetL, _rTargetR);
  if (_rCur >= tgt) {
    _ramping = false;
    return;
  }

  _rCur += 25;
  if (_rCur > tgt) _rCur = tgt;
  _setPWM(min(_rCur, _rTargetL), min(_rCur, _rTargetR));
}

void MotorController::_brake() {
  _ramping = false;
  ledcWrite(PWM_CH_ENA, 0);
  ledcWrite(PWM_CH_ENB, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
}
