/**
 * I2S 音频流模块 —— ESP32-C3 全双工音频
 *
 * INMP441 (麦克风) ← I2S DIN (GPIO6)
 * MAX98357 (扬声器) → I2S DOUT (GPIO7)
 * 共享 BCLK (GPIO4) + WS (GPIO5)
 *
 * 用法:
 *   AudioStreamer audio;
 *   audio.begin();
 *   // 读麦克风: audio.readMic(buf, len);
 *   // 写扬声器: audio.writeSpeaker(data, len);
 */

#ifndef AUDIO_STREAMER_H
#define AUDIO_STREAMER_H

#include <Arduino.h>
#include <driver/i2s.h>

// ---- I2S 引脚 ----
#define AUDIO_BCLK       4
#define AUDIO_WS         5
#define AUDIO_DIN        6
#define AUDIO_DOUT       7

// ---- 音频参数 ----
#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_BITS         16
#define AUDIO_CHANNELS     1
#define AUDIO_CHUNK_SAMPLES  256   // 每帧样本数 (~16ms)
#define AUDIO_CHUNK_BYTES    (AUDIO_CHUNK_SAMPLES * (AUDIO_BITS / 8))

// ---- 环形缓冲区 ----
#define AUDIO_RING_SIZE    (AUDIO_SAMPLE_RATE * 2)  // 1 秒缓冲 (32KB)

/**
 * 简易环形缓冲区（单生产者/单消费者，无锁）
 */
class RingBuf {
public:
  RingBuf(size_t size);
  ~RingBuf();

  size_t available();       // 可读字节数
  size_t freeSpace();       // 可写字节数
  size_t write(const uint8_t *data, size_t len);
  size_t read(uint8_t *dest, size_t maxLen);
  void reset();

private:
  uint8_t *_buf;
  size_t _size;
  volatile size_t _head = 0;
  volatile size_t _tail = 0;
};

/**
 * I2S 全双工音频流管理器
 */
class AudioStreamer {
public:
  bool begin();
  void stop();
  bool isRunning() { return _running; }

  // ---- 麦克风 (ESP32 → PC) ----
  size_t micAvailable();
  size_t readMic(uint8_t *buf, size_t maxLen);

  // ---- 扬声器 (PC → ESP32) ----
  size_t speakerAvailable();
  size_t readSpeaker(uint8_t *buf, size_t maxLen);
  size_t writeSpeaker(const uint8_t *data, size_t len);
  size_t speakerFreeSpace();

private:
  static void _captureTask(void *pv);
  static void _playbackTask(void *pv);

  bool _running = false;
  TaskHandle_t _captureHandle = nullptr;
  TaskHandle_t _playbackHandle = nullptr;

  RingBuf _micBuf{AUDIO_RING_SIZE};
  RingBuf _spkBuf{AUDIO_RING_SIZE};
};

#endif
