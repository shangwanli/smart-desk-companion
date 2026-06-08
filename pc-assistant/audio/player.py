"""
音频播放与机器人音频桥接

RobotAudioBridge:
  - 接收机器人麦克风音频 → VAD → 累积语音段
  - PC TTS 音频 → 编码为二进制帧 → 发送给机器人扬声器
  - 本地 PCM 播放（调试/回退用）
"""

from __future__ import annotations

import asyncio
import logging
from typing import Callable, Optional

logger = logging.getLogger(__name__)

SAMPLE_RATE = 16000
CHANNELS = 1
SAMPLE_WIDTH = 2  # 16-bit


class PCMPlayer:
    """本地 PCM 音频播放器 (PyAudio)"""

    def __init__(self):
        self._pyaudio = None
        self._stream = None

    def open(self):
        import pyaudio
        self._pyaudio = pyaudio.PyAudio()
        self._stream = self._pyaudio.open(
            format=pyaudio.paInt16,
            channels=CHANNELS,
            rate=SAMPLE_RATE,
            output=True,
        )

    def play(self, pcm_data: bytes):
        """播放 16kHz 16-bit mono PCM"""
        if self._stream is None:
            self.open()
        try:
            self._stream.write(pcm_data)
        except Exception as e:
            logger.error(f"PCM 播放失败: {e}")

    def close(self):
        if self._stream:
            self._stream.stop_stream()
            self._stream.close()
            self._stream = None
        if self._pyaudio:
            self._pyaudio.terminate()
            self._pyaudio = None


class RobotAudioBridge:
    """
    机器人音频桥接器 —— 连接机器人音频 I/O 与 PC AI 管线

    用法:
      bridge = RobotAudioBridge(robot)
      bridge.on_speech(lambda text: print(f"用户说: {text}"))

      # 在 WebSocket 二进制回调中:
      robot.on_binary(lambda ftype, data: bridge.feed_mic(data))

      # 发送 TTS 给机器人:
      await bridge.send_tts(pcm_data)
    """

    def __init__(self, robot, vad_mode: str = "energy"):
        from server.ws_client import RobotConnection
        from utils.vad import VADPipeline

        self.robot: RobotConnection = robot
        self._vad = VADPipeline(mode=vad_mode)
        self._player: Optional[PCMPlayer] = None
        self._speech_handlers: list[Callable] = []

    def on_speech(self, handler: Callable[[bytes], None]):
        """注册回调: 当检测到完整语音段时调用 handler(pcm_bytes)"""
        self._speech_handlers.append(handler)

    # ---- 接收机器人麦克风 ----

    def feed_mic(self, frame_type: int, pcm_data: bytes):
        """
        处理从机器人收到的二进制音频帧。
        由 ws_client 的 on_binary 回调调用。

        frame_type: 0x01 = 麦克风音频
        pcm_data: 16kHz 16-bit mono PCM
        """
        if frame_type != 0x01:
            return

        # 可选：本地监听（调试用）
        # self._ensure_player().play(pcm_data)

        state = self._vad.process(pcm_data)
        if state == "start":
            logger.debug("机器人: 检测到说话开始")
        elif state == "end":
            audio = self._vad.get_audio()
            dur = len(audio) / (SAMPLE_RATE * SAMPLE_WIDTH)
            logger.info(f"机器人: 语音段结束 ({dur:.1f}s)")
            for handler in self._speech_handlers:
                try:
                    handler(audio)
                except Exception:
                    logger.exception("语音处理回调异常")
            self._vad.reset()

    # ---- 发送 TTS 到机器人扬声器 ----

    async def send_tts(self, pcm_data: bytes):
        """
        将 PCM 音频发送到机器人扬声器。
        pcm_data: 16kHz 16-bit mono PCM
        """
        if not self.robot.connected:
            logger.warning("机器人未连接，无法发送音频")
            return
        await self.robot.send_audio(pcm_data)

    async def send_tts_file(self, pcm_path: str):
        """发送 PCM 文件到机器人"""
        with open(pcm_path, "rb") as f:
            pcm_data = f.read()
        await self.send_tts(pcm_data)

    # ---- 内部 ----

    def _ensure_player(self) -> PCMPlayer:
        if self._player is None:
            self._player = PCMPlayer()
            self._player.open()
        return self._player

    def close(self):
        """释放资源"""
        if self._player:
            self._player.close()
            self._player = None


async def pcm_to_robot_stream(tts_engine, text: str, robot, chunk_ms: int = 30):
    """
    将 edge-tts 合成 → 转 PCM → 分块发送给机器人。

    用法:
      await pcm_to_robot_stream(tts, "你好小伴", robot)
    """
    from server.ws_client import RobotConnection

    pcm = await tts_engine.synthesize_pcm(text)
    if not pcm:
        logger.warning("TTS 合成失败，无音频")
        return

    chunk_size = SAMPLE_RATE * SAMPLE_WIDTH * chunk_ms // 1000

    for i in range(0, len(pcm), chunk_size):
        chunk = pcm[i:i + chunk_size]
        if not chunk:
            break
        await robot.send_audio(chunk)
        # 模拟实时流：按 chunk 时长等待
        await asyncio.sleep(chunk_ms / 1000.0)
