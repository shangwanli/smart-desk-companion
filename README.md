# 智能桌面伴侣 (Smart Desk Companion)

一款 AI 语音交互桌面机器人。基于 **ESP32-C3** + **N20 蜗杆电机** + **OLED 眼睛动画**，通过 WiFi/WebSocket 与 PC 端 AI 程序联动，实现语音对话、情绪表情和自主运动。

> 灵感与硬件参考来自「桌面机器人」（Huy Vector 原作，千秋我不见转译优化）。原参考代码保留在 `jiqiren/` 目录，本项目的固件与 PC 程序为全新实现。

---

## 功能特性

- 🤖 **表情动画** — OLED 显示活灵活现的机器人眼睛，支持开心/愤怒/困倦/困惑/大笑等多种情绪
- 🎙️ **AI 语音交互** — PC 端完成 ASR → LLM → TTS 完整链路，机器人负责拾音与播放
- 🚗 **运动控制** — 双轮差速驱动，支持前进/后退/左右转 + 随机探索模式
- 🔋 **电源管理** — 锂电池供电、低压检测、OLED 屏保省电
- 📡 **WebSocket 实时通信** — 双向 JSON 指令 + 二进制音频帧

---

## 系统架构

```
┌─────────────────────┐        WiFi/WebSocket        ┌─────────────────────┐
│    PC 端 (Python)    │ ◄══════════════════════════► │   机器人 (ESP32-C3)   │
│                     │   JSON 指令 (表情/运动/配置)    │                     │
│  VAD → ASR → LLM    │ ───────────────────────────► │  OLED 眼睛动画        │
│       ↓ TTS         │                              │  L298N 电机驱动        │
│  情绪→表情/动作映射   │ ◄─────────────────────────── │  I2S 麦克风/扬声器     │
│                     │   二进制音频帧 (PCM) + 状态     │  电池检测             │
└─────────────────────┘                              └─────────────────────┘
```

**数据流（语音对话）**：
```
用户说话 → 麦克风 → Silero VAD 检测 → faster-whisper 语音识别(ASR)
        → 大模型(LLM) 生成回复 → edge-tts 语音合成 → 扬声器播放
        → 情绪分析 → WebSocket 发送表情/动作指令 → 机器人响应
```

---

## 目录结构

```
smart-desk-companion/
├── jiqiren/                        # 原始参考代码（不改动）
│   ├── jiqiren.ino                 #   参考固件 (AP 模式 + HTTP)
│   └── FluxGarage_RoboEyes.h       #   眼睛动画库
│
├── esp32-firmware/                  # 机器人固件 (PlatformIO)
│   ├── platformio.ini
│   ├── include/
│   │   └── config.h                #   全局配置（引脚/WiFi/参数）
│   ├── src/
│   │   └── main.cpp                #   主程序入口
│   └── lib/
│       ├── FluxGarage_RoboEyes/    #   眼睛动画库
│       ├── motor_controller/       #   非阻塞电机控制
│       └── audio_streamer/         #   I2S 音频采集/播放
│
├── pc-assistant/                    # PC 端 AI 程序 (Python)
│   ├── main.py                     #   入口 (命令行)
│   ├── server/ws_client.py         #   WebSocket 客户端
│   ├── audio/                      #   音频接收/播放
│   ├── ai/                         #   ASR / LLM / TTS
│   ├── controller/                 #   表情/运动指令生成
│   └── utils/vad.py                #   语音活动检测
│
└── docs/
    └── BOM.md                      # 材料清单
```

---

## 硬件清单

完整清单见 [`docs/BOM.md`](docs/BOM.md)，核心组件：

| 组件 | 型号 |
|------|------|
| 主控 | ESP32-C3 Super Mini |
| 电机驱动 | L298N 迷你版 |
| 电机 | N20 蜗杆减速电机 3V ×2 |
| 屏幕 | 0.96" OLED 128×64 I2C |
| 麦克风 | INMP441 (I2S) |
| 功放 | MAX98357 (I2S) |
| 电池 | 14250 锂电池 3.7V |

---

## 引脚分配

| GPIO | 功能 |
|------|------|
| 0 | 电机 IN1（左轮方向1） |
| 1 | 电机 IN2（左轮方向2） |
| 2 | 电机 IN3（右轮方向1） |
| 3 | 电机 IN4（右轮方向2） |
| 4 | I2S BCLK（音频位时钟） |
| 5 | I2S WS（音频字选时钟） |
| 6 | I2S DIN（INMP441 麦克风） |
| 7 | I2S DOUT（MAX98357 扬声器） |
| 8 | OLED SDA（I2C 数据） |
| 9 | OLED SCL（I2C 时钟） |

---

## 通信协议

连接地址：`ws://<ESP32_IP>:81`

### PC → 机器人（JSON）

```json
{"type": "motion", "cmd": "forward", "speed": 200, "duration_ms": 2000}
{"type": "expression", "mood": "happy"}
{"type": "animation", "cmd": "laugh"}
{"type": "screen", "cmd": "off"}
```

### 机器人 → PC（JSON + 二进制）

```json
{"type": "status", "mode": 0, "battery_mv": 3800, "rssi": -45}
```

音频上行二进制帧：`[type=0x01][len(2字节大端)][PCM 数据...]`

---

## 快速开始

### 1. 编译固件

1. 安装 [VS Code](https://code.visualstudio.com/) + PlatformIO 插件
2. 打开 `esp32-firmware/` 目录
3. 修改 `include/config.h` 中的 WiFi 凭证：
   ```cpp
   #define WIFI_SSID     "你的WiFi名"
   #define WIFI_PASSWORD "你的WiFi密码"
   ```
4. 点击 PlatformIO 底部工具栏 **→ Upload** 烧录

> 烧录前需先关闭串口监视器（■ 按钮）；若失败可手动按 BOOT 键进入烧录模式。

### 2. 运行 PC 端程序

```bash
cd pc-assistant
pip install -e ".[ai]"

# 交互命令模式
python main.py --host 192.168.1.100

# 演示序列（表情+运动全流程）
python main.py --host 192.168.1.100 --demo

# AI 语音对话
python main.py --host 192.168.1.100 --voice
```

`192.168.1.100` 替换为机器人串口输出的 IP 地址。

---

## 运行模式

```
上电 → 电机自检 + 眼睛动画
      ↓
┌─ 活跃模式 (ACTIVE) ── PC 在线，AI 语音交互，表情联动
│        │ PC 断连 或 5 分钟无交互
│        ↓
├─ 独立模式 (STANDALONE) ── 随机探索移动，眼睛闲置动画
│        │ 10 分钟无交互
│        ↓
└─ 休眠模式 (SLEEP) ── 闭眼防烧屏，电机停，收到指令即唤醒
```

---

## 当前状态

- ✅ 电机控制（非阻塞 + 软启动 + 超时保护）
- ✅ OLED 眼睛动画（多种表情 + 自动眨眼）
- ✅ WebSocket 通信（JSON 指令 + 状态广播）
- ✅ WiFi 连接 + 断线重连
- 🔧 语音模块（I2S 音频，待单独测试）
- 🔧 电池电压检测（GPIO 需复用，待硬件确认）

---

## 致谢

- 原参考代码作者：Huy Vector
- 转译优化：千秋我不见（抖音）
- 眼睛动画库：[FluxGarage RoboEyes](https://github.com/FluxGarage/RoboEyes)
