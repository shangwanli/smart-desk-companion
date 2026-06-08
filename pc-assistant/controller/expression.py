"""
表情/情绪映射 —— 将 LLM 回复的情绪映射到机器人指令
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Optional


@dataclass
class ExpressionCommand:
    """机器人表情指令"""
    mood: Optional[str] = None       # 基础情绪（normal/happy/angry/tired）
    animation: Optional[str] = None  # 特殊动画（laugh/confused/blink）
    motion: Optional[dict] = None    # 伴随动作

    def to_message(self) -> dict | None:
        """转为最合适的 WebSocket 消息"""
        if self.animation:
            return {"type": "animation", "cmd": self.animation}
        if self.mood:
            return {"type": "expression", "mood": self.mood}
        return None


def detect_emotion(text: str) -> ExpressionCommand:
    """
    从 LLM 回复文本中检测情绪，返回对应的表情指令。
    先用关键词匹配，未来可以接 sentiment analysis 模型。
    """
    text_lower = text.lower()

    # ---- 关键词匹配 ----
    happy_kw = ["开心", "哈哈", "高兴", "太棒", "恭喜", "快乐", "喜欢",
                "happy", "great", "wonderful", "love", "excellent"]
    sad_kw = ["难过", "伤心", "遗憾", "可惜", "对不起", "抱歉",
              "sorry", "unfortunately", "sad"]
    angry_kw = ["生气", "愤怒", "讨厌", "不行", "滚",
                "angry", "hate"]
    tired_kw = ["累了", "困了", "休息", "睡觉",
                "tired", "sleep", "rest"]
    laugh_kw = ["笑", "滑稽", "逗", "lol", "haha",
                "funny", "laugh", "joke"]
    confused_kw = ["困惑", "不明白", "奇怪", "搞不懂", "怎么回事",
                   "confused", "puzzled", "what", "why"]

    # 优先检测特殊动画
    for kw in laugh_kw:
        if kw in text_lower:
            return ExpressionCommand(mood="happy", animation="laugh")
    for kw in confused_kw:
        if kw in text_lower:
            return ExpressionCommand(animation="confused")

    # 检测情绪
    for kw in happy_kw:
        if kw in text_lower:
            return ExpressionCommand(mood="happy")
    for kw in angry_kw:
        if kw in text_lower:
            return ExpressionCommand(mood="angry")
    for kw in tired_kw:
        if kw in text_lower:
            return ExpressionCommand(mood="tired")
    for kw in sad_kw:
        if kw in text_lower:
            return ExpressionCommand(mood="tired")

    # 默认
    return ExpressionCommand(mood="normal")


def parse_move_command(text: str) -> Optional[dict]:
    """
    从语音识别的文本中解析移动指令。
    例如 "前进" "后退" "左转" "右转" "探索" "停"
    """
    text = text.strip().lower()

    moves = {
        "前进": ("forward", 200, 2000),
        "往前": ("forward", 200, 2000),
        "向前": ("forward", 200, 2000),
        "后退": ("backward", 180, 1500),
        "往后": ("backward", 180, 1500),
        "向后": ("backward", 180, 1500),
        "左转": ("left", 180, 1200),
        "向左转": ("left", 180, 1200),
        "右转": ("right", 180, 1200),
        "向右转": ("right", 180, 1200),
        "停": ("stop", 0, 0),
        "停止": ("stop", 0, 0),
        "探索": ("explore", 120, 0),
        "自由": ("explore", 120, 0),
    }

    for keyword, (cmd, speed, dur) in moves.items():
        if keyword in text:
            return {"type": "motion", "cmd": cmd, "speed": speed, "duration_ms": dur}

    return None
