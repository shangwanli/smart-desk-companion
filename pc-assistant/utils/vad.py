"""
语音活动检测 (VAD)
支持两种模式:
  1. energy  - 基于音量能量（无需额外依赖）
  2. webrtc  - 基于 WebRTC VAD（需 pip install webrtcvad）
"""

from __future__ import annotations

import logging
from collections import deque

import numpy as np

logger = logging.getLogger(__name__)

# 音频参数
SAMPLE_RATE = 16000
FRAME_MS = 30  # VAD 帧长 (10/20/30 ms)
FRAME_SIZE = SAMPLE_RATE * FRAME_MS // 1000  # 每帧样本数

# 能量 VAD 参数
ENERGY_THRESHOLD = 0.02      # 音量阈值 (0-1)
SILENCE_FRAMES = 20          # 连续静音帧数算说话结束 (~600ms)
SPEECH_START_FRAMES = 5      # 连续有声帧数算说话开始 (~150ms)


class EnergyVAD:
    """基于能量的简单 VAD"""

    def __init__(self, threshold: float = ENERGY_THRESHOLD):
        self.threshold = threshold

    def is_speech(self, frame: bytes) -> bool:
        """判断一帧是否为语音"""
        samples = np.frombuffer(frame, dtype=np.int16)
        energy = np.sqrt(np.mean(samples.astype(np.float64) ** 2)) / 32768.0
        return energy > self.threshold


class VADPipeline:
    """VAD 管线：管理语音段起止检测"""

    def __init__(self, mode: str = "energy", aggressiveness: int = 2):
        self.mode = mode
        self.silence_frames = SILENCE_FRAMES
        self.speech_start_frames = SPEECH_START_FRAMES

        if mode == "webrtc":
            try:
                import webrtcvad
                self._vad = webrtcvad.Vad(aggressiveness)
            except ImportError:
                logger.warning("webrtcvad 未安装，回退到 energy 模式")
                self.mode = "energy"
                self._vad = EnergyVAD()
        else:
            self._vad = EnergyVAD()

        self.reset()

    @property
    def is_speaking(self) -> bool:
        """是否正在说话"""
        return self._speaking

    def reset(self):
        """重置状态"""
        self._speaking = False
        self._silence_count = 0
        self._speech_count = 0
        self._frames: list[bytes] = []

    def process(self, frame: bytes) -> str:
        """
        处理一帧音频，返回当前状态:
          "silence"  - 静音中
          "start"    - 刚开始说话
          "speaking" - 说话中
          "end"      - 说完话了（返回累积的音频数据）
        """
        is_speech = self._vad.is_speech(frame)

        if self._speaking:
            self._frames.append(frame)
            if is_speech:
                self._silence_count = 0
            else:
                self._silence_count += 1
                if self._silence_count >= self.silence_frames:
                    self._speaking = False
                    self._silence_count = 0
                    return "end"
            return "speaking"
        else:
            if is_speech:
                self._speech_count += 1
                self._frames.append(frame)
                if self._speech_count >= self.speech_start_frames:
                    self._speaking = True
                    self._speech_count = 0
                    return "start"
            else:
                self._speech_count = 0
                # 不积累静音帧，避免缓冲膨胀
                if self._frames:
                    self._frames = self._frames[-5:]  # 只保留最后几帧
            return "silence"

    def get_audio(self) -> bytes:
        """获取当前累积的音频数据"""
        return b"".join(self._frames)

    def get_duration_seconds(self) -> float:
        """获取当前累积音频的时长"""
        return len(self.get_audio()) / (SAMPLE_RATE * 2)  # 16-bit = 2 bytes/sample


class WebRTCVAD:
    """WebRTC VAD 适配器"""
    def __init__(self, aggressiveness: int = 2):
        import webrtcvad
        self._vad = webrtcvad.Vad(aggressiveness)

    def is_speech(self, frame: bytes) -> bool:
        return self._vad.is_speech(frame, SAMPLE_RATE)
