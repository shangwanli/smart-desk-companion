#ifndef CONFIG_H
#define CONFIG_H

// ============================
// WiFi 配置（STA 模式连路由器）
// ============================
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// WebSocket 端口
#define WS_PORT 81

// ============================
// 电机引脚 (L298N 迷你) — 与原参考 jiqiren.ino 完全一致
// IN1=LF=GPIO0, IN2=LB=GPIO1, IN3=RF=GPIO2, IN4=RB=GPIO3
// Motor-A (OUT1/2)=左轮, Motor-B (OUT3/4)=右轮
// ============================
#define PIN_IN1   0   // 左轮方向 1 (原 LF)
#define PIN_IN2   1   // 左轮方向 2 (原 LB)
#define PIN_IN3   2   // 右轮方向 1 (原 RF)
#define PIN_IN4   3   // 右轮方向 2 (原 RB)
#define PIN_ENA   10  // 左轮 PWM (原 STBY)
#define PIN_ENB   21  // 右轮 PWM (新增，原参考无)

// PWM 通道 (ledc)
#define PWM_CH_ENA  0
#define PWM_CH_ENB  1
#define PWM_FREQ    1000    // PWM 频率 1kHz
#define PWM_RES     8       // 8-bit 分辨率 (0-255)

// 电机保护
#define MOTOR_MAX_DURATION_MS  3000   // 单次运行最长 3 秒
#define MOTOR_RAMP_MS          150    // 软启动/刹车渐变时间

// ============================
// OLED 显示屏 (I2C)
// ============================
#define OLED_SDA   8
#define OLED_SCL   9
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

// ============================
// I2S 音频
// ============================
// #define AUDIO_ENABLED       // 语音模块待单独测试，暂禁用
#define I2S_BCLK  4
#define I2S_WS    5
#define I2S_DIN   6            // INMP441 麦克风
#define I2S_DOUT  7            // MAX98357 扬声器
#define I2S_SAMPLE_RATE  16000

// ============================
// 电池检测
// 注意: ESP32-C3 的 ADC 仅在 GPIO0-5 可用，GPIO20 不支持 ADC。
//       如需电池监测，可选:
//       1) 外接 I2C ADC (如 ADS1115) 挂在 OLED 的 I2C 总线上
//       2) 分时复用 GPIO5 (ADC2_CH0) — 需暂停 I2S 期间采样
//       目前电池检测已禁用，取消注释并修改引脚可启用。
// ============================
// #define PIN_BATTERY_ADC  20   // GPIO20 无 ADC 功能，已禁用
#define BATTERY_LOW_MV    3300
#define BATTERY_CHECK_INTERVAL_MS  5000

// ============================
// 模式 & 屏保
// ============================
#define STANDBY_TIMEOUT_MS      (5 * 60 * 1000)   // 5 分钟无交互 → 独立模式
#define SLEEP_TIMEOUT_MS        (10 * 60 * 1000)  // 10 分钟无交互 → 休眠
#define OLED_SAVER_TIMEOUT_MS   (10 * 60 * 1000)  // 10 分钟闭眼

// ============================
// 随机探索模式参数
// ============================
#define EXPLORE_TICK_MS     40    // 探索模式每 tick 间隔
#define EXPLORE_MOVE_CHANCE  3    // 每 100 次 tick 中有几次动作

#endif
