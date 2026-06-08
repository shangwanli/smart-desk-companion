/**
 * I2S 音频流实现 —— ESP32-C3 全双工
 *
 * 架构:
 *   - I2S 工作在 Master 全双工模式（RX + TX 共享 BCLK/WS）
 *   - DMA 缓冲: 4 × 256 samples (每帧 ~16ms)
 *   - Capture Task: I2S DMA → mic ring buffer
 *   - Playback Task: speaker ring buffer → I2S DMA
 */

#include "audio_streamer.h"
#include <cstring>

// ============================
// RingBuf 实现
// ============================

RingBuf::RingBuf(size_t size) {
  _size = size + 1;  // +1 避免 head==tail 二义性
  _buf = new uint8_t[_size];
  reset();
}

RingBuf::~RingBuf() {
  delete[] _buf;
}

void RingBuf::reset() {
  _head = 0;
  _tail = 0;
}

size_t RingBuf::available() {
  return (_head >= _tail) ? (_head - _tail) : (_size - _tail + _head);
}

size_t RingBuf::freeSpace() {
  size_t used = available();
  return (_size - 1) - used;
}

size_t RingBuf::write(const uint8_t *data, size_t len) {
  size_t space = freeSpace();
  if (len > space) len = space;
  if (len == 0) return 0;

  size_t first = _size - _head;
  if (first >= len) {
    memcpy(_buf + _head, data, len);
    _head = (_head + len) % _size;
  } else {
    memcpy(_buf + _head, data, first);
    memcpy(_buf, data + first, len - first);
    _head = len - first;
  }
  return len;
}

size_t RingBuf::read(uint8_t *dest, size_t maxLen) {
  size_t avail = available();
  if (maxLen > avail) maxLen = avail;
  if (maxLen == 0) return 0;

  size_t first = _size - _tail;
  if (first >= maxLen) {
    memcpy(dest, _buf + _tail, maxLen);
    _tail = (_tail + maxLen) % _size;
  } else {
    memcpy(dest, _buf + _tail, first);
    memcpy(dest + first, _buf, maxLen - first);
    _tail = maxLen - first;
  }
  return maxLen;
}

// ============================
// AudioStreamer 实现
// ============================

bool AudioStreamer::begin() {
  // 1. 配置 I2S
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX);
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("[AUDIO] i2s_driver_install failed: %d\n", err);
    return false;
  }

  // 2. 配置引脚
  i2s_pin_config_t pin = {};
  pin.bck_io_num = AUDIO_BCLK;
  pin.ws_io_num = AUDIO_WS;
  pin.data_out_num = AUDIO_DOUT;
  pin.data_in_num = AUDIO_DIN;
  pin.mck_io_num = I2S_PIN_NO_CHANGE;

  err = i2s_set_pin(I2S_NUM_0, &pin);
  if (err != ESP_OK) {
    Serial.printf("[AUDIO] i2s_set_pin failed: %d\n", err);
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  // 3. 启动 FreeRTOS 任务
  _running = true;

  xTaskCreate(
    _captureTask, "audio_cap", 2048,
    this, 2, &_captureHandle);
  xTaskCreate(
    _playbackTask, "audio_play", 2048,
    this, 2, &_playbackHandle);

  Serial.println("[AUDIO] I2S full-duplex started");
  Serial.printf("[AUDIO]   sample_rate=%d, chunk=%d samples, ring=%d bytes\n",
    AUDIO_SAMPLE_RATE, AUDIO_CHUNK_SAMPLES, AUDIO_RING_SIZE);

  return true;
}

void AudioStreamer::stop() {
  _running = false;
  if (_captureHandle) { vTaskDelete(_captureHandle); _captureHandle = nullptr; }
  if (_playbackHandle) { vTaskDelete(_playbackHandle); _playbackHandle = nullptr; }
  i2s_driver_uninstall(I2S_NUM_0);
  Serial.println("[AUDIO] stopped");
}

// ---- 麦克风数据读取 (main loop 调用) ----

size_t AudioStreamer::micAvailable() {
  return _micBuf.available();
}

size_t AudioStreamer::readMic(uint8_t *buf, size_t maxLen) {
  return _micBuf.read(buf, maxLen);
}

// ---- 扬声器数据写入 (WebSocket 回调调用) ----

size_t AudioStreamer::speakerAvailable() {
  return _spkBuf.available();
}

size_t AudioStreamer::readSpeaker(uint8_t *buf, size_t maxLen) {
  return _spkBuf.read(buf, maxLen);
}

size_t AudioStreamer::writeSpeaker(const uint8_t *data, size_t len) {
  return _spkBuf.write(data, len);
}

size_t AudioStreamer::speakerFreeSpace() {
  return _spkBuf.freeSpace();
}

// ============================
// FreeRTOS 任务
// ============================

void AudioStreamer::_captureTask(void *pv) {
  AudioStreamer *self = static_cast<AudioStreamer*>(pv);
  uint8_t buf[AUDIO_CHUNK_BYTES];

  while (self->_running) {
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0, buf, AUDIO_CHUNK_BYTES, &bytesRead, pdMS_TO_TICKS(100));
    if (err == ESP_OK && bytesRead > 0) {
      self->_micBuf.write(buf, bytesRead);
    }
    // ESP_ERR_TIMEOUT → 正常，下一轮继续
    taskYIELD();
  }
  vTaskDelete(nullptr);
}

void AudioStreamer::_playbackTask(void *pv) {
  AudioStreamer *self = static_cast<AudioStreamer*>(pv);
  uint8_t buf[AUDIO_CHUNK_BYTES];

  // 先写一些静音数据给 I2S，启动 DMA 发送
  memset(buf, 0, AUDIO_CHUNK_BYTES);
  i2s_write(I2S_NUM_0, buf, AUDIO_CHUNK_BYTES, nullptr, pdMS_TO_TICKS(50));

  while (self->_running) {
    size_t avail = self->_spkBuf.available();
    if (avail >= AUDIO_CHUNK_BYTES) {
      self->_spkBuf.read(buf, AUDIO_CHUNK_BYTES);
      i2s_write(I2S_NUM_0, buf, AUDIO_CHUNK_BYTES, nullptr, pdMS_TO_TICKS(50));
    } else if (avail > 0) {
      // 不够一帧，补静音凑够
      memset(buf, 0, AUDIO_CHUNK_BYTES);
      self->_spkBuf.read(buf, avail);
      i2s_write(I2S_NUM_0, buf, AUDIO_CHUNK_BYTES, nullptr, pdMS_TO_TICKS(50));
    } else {
      // 无数据 → 发送静音保持 I2S 时钟
      memset(buf, 0, AUDIO_CHUNK_BYTES);
      i2s_write(I2S_NUM_0, buf, AUDIO_CHUNK_BYTES, nullptr, pdMS_TO_TICKS(100));
    }
    taskYIELD();
  }
  vTaskDelete(nullptr);
}
