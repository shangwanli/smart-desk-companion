"""
语音合成 (TTS) —— 使用 edge-tts（免费，中文效果好）

用法:
  tts = TTSEngine()
  await tts.speak("你好，我是小伴")
"""

from __future__ import annotations

import asyncio
import io
import logging
import tempfile
import subprocess
import os
import platform

logger = logging.getLogger(__name__)

# 中文语音: zh-CN-XiaoxiaoNeural (女声, 活泼)
DEFAULT_VOICE = "zh-CN-XiaoxiaoNeural"


class TTSEngine:
    """Edge-TTS 语音合成引擎"""

    def __init__(self, voice: str = DEFAULT_VOICE):
        self.voice = voice

    async def speak(self, text: str) -> bool:
        """合成并播放语音"""
        if not text.strip():
            return False

        try:
            import edge_tts

            communicate = edge_tts.Communicate(text, self.voice)
            audio_chunks = []
            async for chunk in communicate.stream():
                if chunk["type"] == "audio":
                    audio_chunks.append(chunk["data"])

            if not audio_chunks:
                logger.warning("TTS 未生成音频")
                return False

            audio_data = b"".join(audio_chunks)
            await self._play_audio(audio_data)
            return True

        except ImportError:
            logger.warning("edge-tts 未安装，尝试系统 TTS...")
            return await self._fallback_tts(text)
        except Exception as e:
            logger.error(f"TTS 错误: {e}")
            return False

    async def synthesize(self, text: str) -> bytes | None:
        """仅合成音频，返回 MP3 bytes（不播放）"""
        try:
            import edge_tts

            communicate = edge_tts.Communicate(text, self.voice)
            audio_chunks = []
            async for chunk in communicate.stream():
                if chunk["type"] == "audio":
                    audio_chunks.append(chunk["data"])
            return b"".join(audio_chunks) if audio_chunks else None
        except Exception as e:
            logger.error(f"TTS 合成错误: {e}")
            return None

    async def synthesize_pcm(self, text: str) -> bytes | None:
        """合成并转为 16kHz mono PCM（用于发给机器人）"""
        mp3_bytes = await self.synthesize(text)
        if not mp3_bytes:
            return None

        # 用 ffmpeg 转码 mp3 → 16kHz mono PCM
        return await self._mp3_to_pcm(mp3_bytes)

    @staticmethod
    async def _play_audio(audio_data: bytes):
        """播放 MP3 音频"""
        # 使用 pygame 播放
        try:
            import pygame
            pygame.mixer.init(frequency=24000)
            buf = io.BytesIO(audio_data)
            sound = pygame.mixer.Sound(buf)
            channel = sound.play()
            while channel.get_busy():
                await asyncio.sleep(0.05)
        except ImportError:
            # 回退：写入临时文件并调用系统播放器
            logger.debug("pygame 未安装，使用临时文件播放")
            with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as f:
                f.write(audio_data)
                tmp = f.name
            try:
                if platform.system() == "Windows":
                    subprocess.run(["powershell", "-c",
                        f"(New-Object Media.SoundPlayer '{tmp}').PlaySync()"],
                        capture_output=True)
                elif platform.system() == "Darwin":
                    subprocess.run(["afplay", tmp])
                else:
                    subprocess.run(["mpg123", tmp], capture_output=True)
            finally:
                try: os.unlink(tmp)
                except: pass

    @staticmethod
    async def _mp3_to_pcm(mp3_bytes: bytes) -> bytes:
        """ffmpeg 转码 MP3 → 16kHz 16-bit mono PCM"""
        proc = await asyncio.create_subprocess_exec(
            "ffmpeg", "-i", "pipe:0", "-f", "s16le",
            "-acodec", "pcm_s16le", "-ar", "16000", "-ac", "1",
            "pipe:1",
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL,
        )
        stdout, _ = await proc.communicate(mp3_bytes)
        return stdout

    async def _fallback_tts(self, text: str) -> bool:
        """无 edge-tts 时的回退方案"""
        if platform.system() == "Windows":
            try:
                import win32com.client
                speaker = win32com.client.Dispatch("SAPI.SpVoice")
                speaker.Speak(text)
                return True
            except ImportError:
                pass
        logger.error("无可用的 TTS 方案")
        return False
