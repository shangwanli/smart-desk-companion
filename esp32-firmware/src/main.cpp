#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <WebSocketsServer.h>

#include "config.h"
#include "FluxGarage_RoboEyes.h"
#include "motor_controller.h"
#ifdef AUDIO_ENABLED
#include "audio_streamer.h"
#endif

// ============================
// 全局对象
// ============================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RoboEyes<Adafruit_SSD1306> roboEyes(display);
MotorController motor;
WebSocketsServer ws(WS_PORT);
#ifdef AUDIO_ENABLED
  AudioStreamer audio;
#endif

// ============================
// 运行模式
// ============================
enum RobotMode {
  MODE_ACTIVE,      // AI 交互中
  MODE_STANDALONE,  // 随机探索
  MODE_SLEEP        // 休眠（闭眼，电机停）
};
RobotMode mode = MODE_SLEEP;  // 初始值避免与 enterMode(MODE_ACTIVE) 冲突

unsigned long lastInteraction = 0;  // 上次收到 PC 指令的时间
bool wsClientConnected = false;     // 是否有 WebSocket 客户端

// 延迟动作（解决表情动画中的 delay 问题）
struct DelayedAction {
  bool pending = false;
  MotorCmd cmd = M_STOP;
  uint8_t speed = 0;
  uint32_t duration = 0;
  unsigned long triggerAt = 0;
};
DelayedAction delayedAction;

// 电池状态
float batteryVoltage = 0.0;
bool batteryLow = false;

// ============================
// 前向声明
// ============================
void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len);
void handleJsonCommand(const char *json);
void doExploreTick();
void checkBattery();
void updateMode();
void enterMode(RobotMode newMode);
void connectWiFi();

// ============================
// WebSocket 事件处理
// ============================
void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WS] 客户端 #%u 已连接 (IP: %s)\n", num, ws.remoteIP(num).toString().c_str());
      wsClientConnected = true;
      lastInteraction = millis();
      if (mode == MODE_SLEEP) enterMode(MODE_ACTIVE);
      break;

    case WStype_DISCONNECTED:
      Serial.printf("[WS] 客户端 #%u 已断开\n", num);
      if (ws.connectedClients() == 0) {
        wsClientConnected = false;
      }
      break;

    case WStype_TEXT:
      handleJsonCommand((const char*)payload);
      break;

#ifdef AUDIO_ENABLED
    case WStype_BIN:
      // 解析二进制音频帧: [type:1][len:2 big-endian][PCM data...]
      // type=0x02 → PC 发来的 TTS 音频，写入扬声器
      if (len >= 3) {
        uint8_t frameType = payload[0];
        uint16_t dataLen = ((uint16_t)payload[1] << 8) | payload[2];
        if (frameType == 0x02 && dataLen + 3 == len) {
          audio.writeSpeaker(payload + 3, dataLen);
        }
      }
      break;
#endif

    default: break;
  }
}

// ============================
// JSON 指令解析
// ============================
// 简易 JSON 解析，避免引入完整 JSON 库增加体积
static void extractStr(const char *json, const char *key, char *out, size_t maxLen) {
  out[0] = '\0';
  char search[64];
  snprintf(search, sizeof(search), "\"%s\":\"", key);
  const char *p = strstr(json, search);
  if (!p) return;
  p += strlen(search);
  size_t i = 0;
  while (*p && *p != '"' && i < maxLen - 1) out[i++] = *p++;
  out[i] = '\0';
}

static long extractInt(const char *json, const char *key, long defVal) {
  char search[64];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char *p = strstr(json, search);
  if (!p) return defVal;
  p += strlen(search);
  return atol(p);
}

void handleJsonCommand(const char *json) {
  lastInteraction = millis();

  // 清除待执行的延迟动作（新指令覆盖旧动作链）
  delayedAction.pending = false;

  // 切换到活跃模式
  if (mode != MODE_ACTIVE) enterMode(MODE_ACTIVE);

  char type[16] = {0};
  char cmd[16] = {0};
  char mood[16] = {0};

  extractStr(json, "type", type, sizeof(type));
  extractStr(json, "cmd", cmd, sizeof(cmd));
  extractStr(json, "mood", mood, sizeof(mood));

  // ---- 运动指令 ----
  if (strcmp(type, "motion") == 0 || strcmp(type, "move") == 0) {
    uint8_t speed = (uint8_t)extractInt(json, "speed", 200);
    uint32_t dur = (uint32_t)extractInt(json, "duration_ms", 2000);

    if (strcmp(cmd, "forward") == 0) {
      motor.execute(M_FORWARD, speed, dur);
    } else if (strcmp(cmd, "backward") == 0) {
      motor.execute(M_BACKWARD, speed, dur);
    } else if (strcmp(cmd, "left") == 0) {
      motor.execute(M_LEFT, speed, dur);
    } else if (strcmp(cmd, "right") == 0) {
      motor.execute(M_RIGHT, speed, dur);
    } else if (strcmp(cmd, "stop") == 0) {
      motor.stop();
    } else if (strcmp(cmd, "explore") == 0) {
      enterMode(MODE_STANDALONE);
    }
    Serial.printf("[CMD] motion: %s speed=%d dur=%d\n", cmd, speed, dur);
  }

  // ---- 表情指令 ----
  else if (strcmp(type, "expression") == 0) {
    if (strcmp(mood, "happy") == 0) {
      roboEyes.setMood(HAPPY);
      motor.execute(M_FORWARD, 150, 350);
      // 前进结束后延迟 50ms 再后退（非阻塞）
      delayedAction.pending = true;
      delayedAction.cmd = M_BACKWARD;
      delayedAction.speed = 150;
      delayedAction.duration = 300;
      delayedAction.triggerAt = millis() + 400;
    } else if (strcmp(mood, "angry") == 0) {
      roboEyes.setMood(ANGRY);
      motor.execute(M_FORWARD, 255, 300);
    } else if (strcmp(mood, "tired") == 0) {
      roboEyes.setMood(TIRED);
      motor.execute(M_BACKWARD, 100, 400);
    } else if (strcmp(mood, "confused") == 0) {
      roboEyes.anim_confused();
    } else if (strcmp(mood, "laugh") == 0) {
      roboEyes.anim_laugh();
    } else {
      roboEyes.setMood(0);
    }
    Serial.printf("[CMD] expression: %s\n", mood);
  }

  // ---- 动画指令 ----
  else if (strcmp(type, "animation") == 0) {
    if (strcmp(cmd, "laugh") == 0) roboEyes.anim_laugh();
    else if (strcmp(cmd, "confused") == 0) roboEyes.anim_confused();
    else if (strcmp(cmd, "blink") == 0) roboEyes.blink();
    Serial.printf("[CMD] animation: %s\n", cmd);
  }

  // ---- 屏幕控制 ----
  else if (strcmp(type, "screen") == 0) {
    if (strcmp(cmd, "off") == 0) {
      roboEyes.close();
    } else if (strcmp(cmd, "on") == 0) {
      roboEyes.open();
    }
    Serial.printf("[CMD] screen: %s\n", cmd);
  }

  // ---- 配置指令 ----
  else if (strcmp(type, "config") == 0) {
    // 预留：音量、速度等配置
    Serial.printf("[CMD] config: key=%s value=%ld\n", cmd, extractInt(json, "value", 0));
  }
}

// ============================
// WiFi 连接
// ============================
void connectWiFi() {
  Serial.printf("正在连接 WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi connecting...");
  display.print("SSID: ");
  display.println(WIFI_SSID);
  display.display();

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi 已连接! IP: %s\n", WiFi.localIP().toString().c_str());

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.print("IP:");
    display.println(WiFi.localIP());
    display.display();
    delay(1500);
  } else {
    Serial.println("WiFi 连接失败!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Failed!");
    display.println("Check credentials");
    display.display();
    delay(2000);
  }
}

// ============================
// 模式切换
// ============================
void enterMode(RobotMode newMode) {
  if (mode == newMode) return;

  Serial.printf("[MODE] %d -> %d\n", mode, newMode);
  mode = newMode;

  switch (newMode) {
    case MODE_ACTIVE:
      roboEyes.open();
      roboEyes.setMood(0);
      roboEyes.setIdleMode(ON, 2, 2);
      roboEyes.setAutoblinker(ON, 3, 2);
      break;

    case MODE_STANDALONE:
      roboEyes.open();
      roboEyes.setMood(0);
      roboEyes.setIdleMode(ON, 2, 2);
      roboEyes.setAutoblinker(ON, 3, 2);
      motor.stop();
      break;

    case MODE_SLEEP:
      roboEyes.close();
      roboEyes.setIdleMode(OFF);
      roboEyes.setAutoblinker(OFF);
      motor.stop();
      break;
  }
}

void updateMode() {
  unsigned long idle = millis() - lastInteraction;

  if (mode == MODE_ACTIVE && idle > STANDBY_TIMEOUT_MS && !wsClientConnected) {
    enterMode(MODE_STANDALONE);
  }
  else if (mode == MODE_STANDALONE && idle > SLEEP_TIMEOUT_MS) {
    enterMode(MODE_SLEEP);
  }
}

// ============================
// 探索模式 (独立运行)
// ============================
void doExploreTick() {
  static unsigned long lastTick = 0;
  static unsigned long moveEndAt = 0;
  static bool moving = false;

  if (millis() - lastTick < EXPLORE_TICK_MS) return;
  lastTick = millis();

  // 非阻塞：检查当前动作是否该停止了
  if (moving) {
    if (millis() >= moveEndAt) {
      motor.stop();
      moving = false;
    }
    return;
  }

  if (random(100) < EXPLORE_MOVE_CHANCE) {
    uint8_t cmd = random(1, 9);  // M_FORWARD .. M_RIGHT_FWD
    uint8_t dur = random(1, 12); // 15~180ms
    uint8_t spd = random(80, 160);

    switch (cmd) {
      case M_FORWARD:   motor.setMotor(1,0,0,1, spd); break;  // 前进
      case M_BACKWARD:  motor.setMotor(0,1,1,0, spd); break;  // 后退
      case M_LEFT:      motor.setMotor(0,1,0,1, spd); break;  // 左转
      case M_RIGHT:     motor.setMotor(1,0,1,0, spd); break;  // 右转
      case M_LEFT_FWD:  motor.setMotor(1,0,0,0, spd); break;  // 左轮前进
      case M_RIGHT_FWD: motor.setMotor(0,0,1,0, spd); break;  // 右轮前进
      case M_LEFT_BWD:  motor.setMotor(0,1,0,0, spd); break;  // 左轮后退
      case M_RIGHT_BWD: motor.setMotor(0,0,0,1, spd); break;  // 右轮后退
      default:          motor.setMotor(0,0,0,0, 0);   break;
    }

    moveEndAt = millis() + dur * 25;  // 非阻塞计时
    moving = true;
  }
}

// ============================
// 电池检测 (TODO: Phase 5)
// ============================
void checkBattery() {
#ifdef PIN_BATTERY_ADC
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < BATTERY_CHECK_INTERVAL_MS) return;
  lastCheck = millis();

  int raw = analogRead(PIN_BATTERY_ADC);
  // 分压比例: R1=100k, R2=47k → ratio = (100+47)/47 ≈ 3.13
  batteryVoltage = (raw / 4095.0) * 3.3 * 3.13;
  batteryLow = (batteryVoltage < (BATTERY_LOW_MV / 1000.0));
  Serial.printf("[BATT] raw=%d voltage=%.2fV%s\n",
    raw, batteryVoltage, batteryLow ? " LOW!" : "");
#endif
}

// ============================
// SETUP
// ============================
void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("\n\n==================================");
  Serial.println("  智能桌面伴侣 - 启动中...");
  Serial.println("==================================");

  // ---- 电机初始化 ----
  motor.begin();
  Serial.println("[OK] Motor controller");

  // ---- 启动自检：两轮前进 300ms → 后退 300ms → 停 ----
  Serial.println("[TEST] 电机自检...");
  motor.setMotor(1, 0, 0, 1, 255);  // 前进
  delay(300);
  motor.setMotor(0, 1, 1, 0, 255);  // 后退
  delay(300);
  motor.stop();
  Serial.println("[OK] 电机自检完成");

  // ---- OLED + RoboEyes（WiFi 之前初始化，眼睛先显示）----
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[FAIL] OLED 初始化失败！");
  } else {
    Serial.println("[OK] OLED");
  }

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);
  roboEyes.setMood(0);
  Serial.println("[OK] RoboEyes");

  // ---- WiFi ----
  connectWiFi();

  // ---- WebSocket ----
  ws.begin();
  ws.onEvent(onWsEvent);
  Serial.printf("[OK] WebSocket server on port %d\n", WS_PORT);

#ifdef AUDIO_ENABLED
  // ---- I2S 音频 ----
  if (audio.begin()) {
    Serial.println("[OK] I2S Audio (full-duplex)");
  } else {
    Serial.println("[WARN] I2S Audio init failed — audio disabled");
  }
#endif

  // ---- 随机种子 ----
  randomSeed(esp_random());

  // ---- 启动状态 ----
  lastInteraction = millis();
  mode = MODE_ACTIVE;  // 直接赋值，跳过 enterMode() 的重复初始化

  Serial.println("==================================");
  Serial.printf("  IP: %s:%d\n", WiFi.localIP().toString().c_str(), WS_PORT);
  Serial.println("  WebSocket 就绪，等待 PC 连接...");
  Serial.println("==================================\n");
}

// ============================
// LOOP (非阻塞)
// ============================
void loop() {
  ws.loop();
  roboEyes.update();
  motor.update();
  updateMode();

  // ---- 延迟动作执行（非阻塞表情动画链） ----
  if (delayedAction.pending && millis() >= delayedAction.triggerAt) {
    if (!motor.isRunning()) {
      motor.execute(delayedAction.cmd, delayedAction.speed, delayedAction.duration);
      delayedAction.pending = false;
    }
  }

  // ---- 低电量 OLED 提示 ----
#ifdef PIN_BATTERY_ADC
  {
    static bool wasBatteryLow = false;
    if (batteryLow != wasBatteryLow) {
      wasBatteryLow = batteryLow;
      if (batteryLow) {
        // 眼睛变困倦 → 自然提示电量不足
        roboEyes.setMood(TIRED);
        Serial.println("[BATT] 低电量！眼睛变困倦提示");
      } else {
        roboEyes.setMood(0);
      }
    }
  }
#endif

  // ---- Standalone 探索 ----
  if (mode == MODE_STANDALONE) {
    doExploreTick();
  }

  // ---- 定期状态输出 + WebSocket 广播 ----
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    lastStatus = millis();
    checkBattery();

    Serial.printf("[STATUS] mode=%d wsClients=%d idle=%lus batt=%.2fV\n",
      mode,
      ws.connectedClients(),
      (millis() - lastInteraction) / 1000,
      batteryVoltage);

    // 广播状态到 PC
    if (wsClientConnected) {
      char statusJson[256];
      snprintf(statusJson, sizeof(statusJson),
        "{\"type\":\"status\",\"mode\":%d,\"battery_mv\":%d,\"rssi\":%d,\"battery_low\":%s,\"idle_s\":%lu}",
        mode,
        (int)(batteryVoltage * 1000),
        (int)WiFi.RSSI(),
        batteryLow ? "true" : "false",
        (millis() - lastInteraction) / 1000);
      ws.broadcastTXT(statusJson);
    }
  }

  // ---- 麦克风音频 → PC (二进制帧) ----
#ifdef AUDIO_ENABLED
  {
    static unsigned long lastAudioSend = 0;
    if (wsClientConnected && audio.isRunning() && millis() - lastAudioSend > 30) {
      lastAudioSend = millis();
      size_t avail = audio.micAvailable();
      if (avail >= AUDIO_CHUNK_BYTES) {
        // 每次最多发送 4 帧 (~64ms)，减少协议开销
        size_t toSend = avail;
        if (toSend > AUDIO_CHUNK_BYTES * 4) toSend = AUDIO_CHUNK_BYTES * 4;
        // 帧头: [type=0x01:1][len:2 big-endian]
        uint8_t frame[AUDIO_CHUNK_BYTES * 4 + 3];
        frame[0] = 0x01;
        frame[1] = (toSend >> 8) & 0xFF;
        frame[2] = toSend & 0xFF;
        audio.readMic(frame + 3, toSend);
        ws.broadcastBIN(frame, toSend + 3);
      }
    }
  }
#endif

  // ---- WiFi 断连检查 ----
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 10000) {
      lastReconnect = millis();
      Serial.println("[WiFi] 断连，尝试重连...");
      WiFi.reconnect();
    }
  }

  // 让出 CPU（单核 ESP32-C3 需要 yield）
  yield();
}
