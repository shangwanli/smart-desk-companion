"""
麦克风录音 —— PyAudio 封装

用法:
  rec = AudioRecorder()
  rec.start()
  while True:
      frame = rec.read()  # 阻塞读取一帧
"""

from __future__ import annotations

import logging
import queue
import threading
from typing import Optional

import numpy as np

logger = logging.getLogger(__name__)

SAMPLE_RATE = 16000
CHANNELS = 1
CHUNK_MS = 30               # 每次读取的帧长
CHUNK_SIZE = SAMPLE_RATE * CHUNK_MS // 1000
FORMAT = "int16"            # PyAudio paInt16
SAMPLE_WIDTH = 2            # 16-bit = 2 bytes


class AudioRecorder:
    """非阻塞麦克风录音器"""

    def __init__(self, sample_rate: int = SAMPLE_RATE, chunk_ms: int = CHUNK_MS):
        self.sample_rate = sample_rate
        self.chunk_size = sample_rate * chunk_ms // 1000
        self._pyaudio = None
        self._stream = None
        self._thread: Optional[threading.Thread] = None
        self._running = False
        self._queue = queue.Queue(maxsize=500)

    # ---- 启动 / 停止 ----

    def start(self):
        """启动录音线程"""
        import pyaudio

        self._pyaudio = pyaudio.PyAudio()
        self._stream = self._pyaudio.open(
            format=pyaudio.paInt16,
            channels=CHANNELS,
            rate=self.sample_rate,
            input=True,
            frames_per_buffer=self.chunk_size,
        )
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        logger.info(f"录音已启动: {self.sample_rate}Hz, {self.chunk_size} samples/frame")

    def stop(self):
        """停止录音"""
        self._running = False
        if self._thread:
            self._thread.join(timeout=2.0)
        if self._stream:
            self._stream.stop_stream()
            self._stream.close()
        if self._pyaudio:
            self._pyaudio.terminate()
        logger.info("录音已停止")

    # ---- 读取 ----

    def read(self, timeout: float = 0.1) -> Optional[bytes]:
        """读取一帧音频（非阻塞）"""
        try:
            return self._queue.get(timeout=timeout)
        except queue.Empty:
            return None

    def read_all(self) -> list[bytes]:
        """读取当前队列中所有帧"""
        frames = []
        while True:
            frame = self.read(timeout=0)
            if frame is None:
                break
            frames.append(frame)
        return frames

    # ---- 内部 ----

    def _run(self):
        """录音线程：持续读取麦克风并放入队列"""
        while self._running:
            try:
                data = self._stream.read(self.chunk_size, exception_on_overflow=False)
                self._queue.put(data)
            except Exception as e:
                logger.error(f"录音错误: {e}")
                break

    @property
    def queue_size(self) -> int:
        return self._queue.qsize()
