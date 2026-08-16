/**
 * 非阻塞电机控制器 — L298N 迷你版
 *
 * L298N 引脚:
 *   IN1, IN2 — 左轮方向
 *   IN3, IN4 — 右轮方向
 *   ENA      — 左轮 PWM
 *   ENB      — 右轮 PWM
 */

#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>

// 电机指令 (与旧版兼容，枚举值不变)
enum MotorCmd : uint8_t {
  M_STOP = 0,
  M_FORWARD,
  M_BACKWARD,
  M_LEFT,
  M_RIGHT,
  M_LEFT_BWD,
  M_RIGHT_BWD,
  M_LEFT_FWD,
  M_RIGHT_FWD,
};

class MotorController {
public:
  void begin();
  void update();
  void execute(MotorCmd cmd, uint8_t speed = 200, uint32_t duration_ms = 2000);
  void stop();
  bool isRunning() { return _running; }

  // 原始控制 (探索模式用)
  // in1-in4: 方向引脚, speed: PWM 占空比
  void setMotor(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4, uint8_t speed = 255);

private:
  void _setDirection(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4);
  void _setPWM(uint8_t leftSpeed, uint8_t rightSpeed);
  void _softStartInit(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4,
                      uint8_t leftSpeed, uint8_t rightSpeed);
  void _softStartTick();
  void _brake();

  bool _running = false;
  uint32_t _startTime = 0;
  uint32_t _duration = 0;
  MotorCmd _currentCmd = M_STOP;
  uint8_t _currentSpeed = 0;

  // 非阻塞软启动
  bool _ramping = false;
  uint8_t _rIn1, _rIn2, _rIn3, _rIn4;
  uint8_t _rCur, _rTargetL, _rTargetR;
  unsigned long _rampLastTick = 0;
};

#endif
