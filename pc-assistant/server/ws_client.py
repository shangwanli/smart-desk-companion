"""
WebSocket 客户端 —— 连接 ESP32 机器人，收发 JSON 指令

协议: ws://<robot_ip>:81
"""

from __future__ import annotations

import asyncio
import json
import logging
from typing import Callable, Optional

import websockets
from websockets.asyncio.client import ClientConnection

logger = logging.getLogger(__name__)


class RobotConnection:
    """管理与机器人 WebSocket 的连接、自动重连、指令发送"""

    def __init__(self, host: str, port: int = 81):
        self.url = f"ws://{host}:{port}"
        self._ws: Optional[ClientConnection] = None
        self._connected = False
        self._callbacks: list[Callable] = []
        self._binary_callbacks: list[Callable] = []
        self._rx_task: Optional[asyncio.Task] = None

    # ---- properties ----

    @property
    def connected(self) -> bool:
        return self._connected and self._ws is not None

    # ---- connect / disconnect ----

    async def connect(self) -> bool:
        """连接机器人，失败返回 False"""
        try:
            self._ws = await websockets.connect(self.url, ping_interval=10)
            self._connected = True
            logger.info(f"已连接到机器人: {self.url}")
            self._rx_task = asyncio.create_task(self._listen())
            return True
        except Exception as e:
            logger.error(f"连接失败: {e}")
            self._connected = False
            return False

    async def disconnect(self):
        """断开连接"""
        self._connected = False
        if self._rx_task:
            self._rx_task.cancel()
            self._rx_task = None
        if self._ws:
            await self._ws.close()
            self._ws = None

    # ---- 发送指令 ----

    async def send(self, msg: dict) -> bool:
        """发送 JSON 指令到机器人"""
        if not self.connected:
            logger.warning("未连接，无法发送")
            return False
        try:
            data = json.dumps(msg, ensure_ascii=False)
            await self._ws.send(data)
            logger.debug(f">>> {data}")
            return True
        except Exception as e:
            logger.error(f"发送失败: {e}")
            self._connected = False
            return False

    async def send_motion(self, cmd: str, speed: int = 200, duration_ms: int = 2000):
        """发送运动指令"""
        return await self.send({
            "type": "motion",
            "cmd": cmd,
            "speed": speed,
            "duration_ms": duration_ms,
        })

    async def send_expression(self, mood: str):
        """发送表情指令"""
        return await self.send({"type": "expression", "mood": mood})

    async def send_animation(self, cmd: str):
        """发送动画指令 (laugh / confused / blink)"""
        return await self.send({"type": "animation", "cmd": cmd})

    async def send_screen(self, cmd: str):
        """屏幕开关 (on / off)"""
        return await self.send({"type": "screen", "cmd": cmd})

    async def send_audio(self, pcm_data: bytes) -> bool:
        """发送 PCM 音频到机器人扬声器 (16kHz 16-bit mono)

        二进制帧格式: [type=0x02:1][len:2 big-endian][PCM data...]
        """
        if not self.connected:
            return False
        try:
            import struct
            header = struct.pack(">BH", 0x02, len(pcm_data))
            await self._ws.send(header + pcm_data)
            return True
        except Exception as e:
            logger.error(f"音频发送失败: {e}")
            self._connected = False
            return False

    # ---- 接收消息 ----

    def on_message(self, callback: Callable):
        """注册 JSON 消息回调 callback(dict)"""
        self._callbacks.append(callback)

    def on_binary(self, callback: Callable):
        """注册二进制帧回调 callback(bytes)
        回调接收原始 PCM 音频数据（不含帧头）。
        帧格式: [type:1][len:2 big-endian][PCM data...]
        """
        self._binary_callbacks.append(callback)

    async def _listen(self):
        """后台接收消息"""
        try:
            async for raw in self._ws:
                if isinstance(raw, bytes):
                    # 二进制音频帧: [type:1][len:2 big-endian][PCM data...]
                    if len(raw) >= 4:
                        frame_type = raw[0]
                        data_len = (raw[1] << 8) | raw[2]
                        pcm_data = raw[3:]
                        for cb in self._binary_callbacks:
                            try:
                                cb(frame_type, pcm_data[:data_len])
                            except Exception:
                                logger.exception("二进制回调异常")
                else:
                    try:
                        msg = json.loads(raw)
                        logger.debug(f"<<< {msg}")
                        for cb in self._callbacks:
                            try:
                                cb(msg)
                            except Exception:
                                logger.exception("回调异常")
                    except json.JSONDecodeError:
                        logger.warning(f"收到非 JSON 消息: {raw[:100]}")
        except Exception:
            if self._connected:
                logger.warning("连接断开")
            self._connected = False
