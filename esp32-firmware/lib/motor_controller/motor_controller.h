#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>

// ============================
// 电机指令类型
// ============================
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

// ============================
// 非阻塞电机控制器
// ============================
class MotorController {
public:
  void begin();
  void update();

  // 执行有超时的动作（非阻塞）
  void execute(MotorCmd cmd, uint8_t speed = 200, uint32_t duration_ms = 2000);

  // 立即停机
  void stop();

  // 是否正在运行
  bool isRunning() { return _running; }

  // Raw motor pin control (for explore mode etc)
  void setMotor(uint8_t lf, uint8_t lb, uint8_t rf, uint8_t rb, uint8_t speed = 255);

private:
  void _drivePins(uint8_t lf, uint8_t lb, uint8_t rf, uint8_t rb, uint8_t speed);
  void _softStartInit(uint8_t lf, uint8_t lb, uint8_t rf, uint8_t rb, uint8_t targetSpeed);
  void _softStartTick();
  void _brake();

  bool _running = false;
  uint32_t _startTime = 0;
  uint32_t _duration = 0;
  MotorCmd _currentCmd = M_STOP;
  uint8_t _currentSpeed = 0;

  // 非阻塞软启动状态
  bool _ramping = false;
  uint8_t _rampLf, _rampLb, _rampRf, _rampRb;
  uint8_t _rampCur, _rampTarget;
  unsigned long _rampLastTick = 0;
};

#endif
