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
// 电机引脚 (TB6612FNG)
// ============================
#define PIN_LF    0   // 左轮前进
#define PIN_LB    1   // 左轮后退
#define PIN_RF    2   // 右轮前进
#define PIN_RB    3   // 右轮后退
#define PIN_STBY  10  // 使能

// PWM 通道 (ledc)
#define PWM_CH_LF   0
#define PWM_CH_LB   1
#define PWM_CH_RF   2
#define PWM_CH_RB   3
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
#define AUDIO_ENABLED          // 注释此行禁用音频模块
#define I2S_BCLK  4
#define I2S_WS    5
#define I2S_DIN   6            // INMP441 麦克风
#define I2S_DOUT  7            // MAX98357 扬声器
#define I2S_SAMPLE_RATE  16000

// ============================
// 电池检测
// ============================
#define PIN_BATTERY_ADC  20   // 分压后接 ADC
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
