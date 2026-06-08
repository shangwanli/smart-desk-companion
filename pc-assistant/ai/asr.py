"""
语音识别 (ASR) —— 使用 faster-whisper 本地模型

用法:
  asr = ASREngine()
  text = asr.transcribe(audio_bytes)
"""

from __future__ import annotations

import io
import logging
import wave

import numpy as np

logger = logging.getLogger(__name__)

SAMPLE_RATE = 16000
DEFAULT_MODEL = "base"  # tiny / base / small / medium


class ASREngine:
    """faster-whisper 语音识别引擎"""

    def __init__(self, model_size: str = DEFAULT_MODEL, device: str = "auto"):
        """
        device: "auto" (自动选 GPU) / "cpu" / "cuda"
        """
        self.model_size = model_size
        self._model = None
        self._device = device

    def load(self):
        """加载模型（首次调用耗时较长）"""
        from faster_whisper import WhisperModel

        device = self._device
        if device == "auto":
            try:
                import torch
                device = "cuda" if torch.cuda.is_available() else "cpu"
            except ImportError:
                device = "cpu"

        logger.info(f"加载 Whisper 模型: {self.model_size} ({device})...")
        self._model = WhisperModel(self.model_size, device=device, compute_type="int8")
        logger.info("Whisper 模型加载完成")

    def transcribe(self, audio_bytes: bytes) -> str:
        """
        将 PCM 音频转为文本。
        audio_bytes: 16kHz 16-bit mono PCM
        """
        if self._model is None:
            self.load()

        # 包装成 WAV
        wav_bytes = self._pcm_to_wav(audio_bytes)
        audio_array = self._bytes_to_float32(audio_bytes)

        segments, _ = self._model.transcribe(audio_array, language="zh")
        text = " ".join(seg.text.strip() for seg in segments)

        if not text:
            logger.warning("ASR 未识别到内容")
            return ""
        logger.info(f"ASR: {text}")
        return text

    # ---- helpers ----

    @staticmethod
    def _pcm_to_wav(pcm: bytes) -> bytes:
        buf = io.BytesIO()
        with wave.open(buf, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(SAMPLE_RATE)
            wf.writeframes(pcm)
        return buf.getvalue()

    @staticmethod
    def _bytes_to_float32(pcm: bytes) -> np.ndarray:
        samples = np.frombuffer(pcm, dtype=np.int16)
        return samples.astype(np.float32) / 32768.0
