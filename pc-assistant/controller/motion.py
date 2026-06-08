"""
运动指令生成 —— 提供预定义的运动模式
"""

from __future__ import annotations

# 预定义动作集
PRESETS = {
    "forward":   {"type": "motion", "cmd": "forward",  "speed": 200, "duration_ms": 2000},
    "backward":  {"type": "motion", "cmd": "backward", "speed": 180, "duration_ms": 1500},
    "left":      {"type": "motion", "cmd": "left",     "speed": 180, "duration_ms": 1200},
    "right":     {"type": "motion", "cmd": "right",    "speed": 180, "duration_ms": 1200},
    "stop":      {"type": "motion", "cmd": "stop",     "speed": 0,   "duration_ms": 0},
    "explore":   {"type": "move",   "cmd": "explore",  "speed": 120, "duration_ms": 0},

    # 表情联动动作
    "happy_wiggle":  {"type": "expression", "mood": "happy"},
    "angry_rush":    {"type": "expression", "mood": "angry"},
    "tired_retreat": {"type": "expression", "mood": "tired"},
    "confused_shake":{"type": "animation", "cmd": "confused"},
    "laugh_dance":   {"type": "animation", "cmd": "laugh"},
}


def get_preset(name: str) -> dict | None:
    """获取预定义动作"""
    return PRESETS.get(name)


def custom_motion(cmd: str, speed: int = 200, duration_ms: int = 2000) -> dict:
    """生成自定义运动指令"""
    return {"type": "motion", "cmd": cmd, "speed": speed, "duration_ms": duration_ms}
