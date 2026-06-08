"""
智能桌面伴侣 —— PC 端主入口

用法:
  python main.py --host 192.168.1.100              # 交互命令模式
  python main.py --host 192.168.1.100 --demo       # 运行演示序列
  python main.py --host 192.168.1.100 --voice      # AI 语音对话 (PC 麦克风)
  python main.py --host 192.168.1.100 --robot-audio # AI 语音对话 (机器人麦)
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import os
import sys

from server.ws_client import RobotConnection
from controller.expression import detect_emotion, parse_move_command
from controller.motion import get_preset

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("companion")


# ============================
# 演示序列
# ============================
DEMO_SEQUENCE = [
    ("表情: 默认",         {"type": "expression", "mood": "normal"}),
    ("运动: 前进 2 秒",    {"type": "motion", "cmd": "forward", "speed": 200, "duration_ms": 2000}),
    ("等待 2s",           None),
    ("表情: 开心",         {"type": "expression", "mood": "happy"}),
    ("运动: 左转",         {"type": "motion", "cmd": "left", "speed": 180, "duration_ms": 1200}),
    ("等待 1.5s",         None),
    ("动画: 困惑抖动",     {"type": "animation", "cmd": "confused"}),
    ("运动: 后退",         {"type": "motion", "cmd": "backward", "speed": 180, "duration_ms": 1500}),
    ("等待 2s",           None),
    ("表情: 愤怒",         {"type": "expression", "mood": "angry"}),
    ("运动: 右转",         {"type": "motion", "cmd": "right", "speed": 180, "duration_ms": 1200}),
    ("等待 1.5s",         None),
    ("动画: 大笑",         {"type": "animation", "cmd": "laugh"}),
    ("表情: 困倦",         {"type": "expression", "mood": "tired"}),
    ("运动: 停止",         {"type": "motion", "cmd": "stop"}),
    ("屏幕: 休眠测试(关)",  {"type": "screen", "cmd": "off"}),
    ("等待 1s",           None),
    ("屏幕: 唤醒测试(开)",  {"type": "screen", "cmd": "on"}),
]


async def run_demo(robot: RobotConnection):
    """运行预定义演示序列"""
    logger.info("=== 开始演示序列 ===")
    for i, (desc, msg) in enumerate(DEMO_SEQUENCE):
        logger.info(f"[{i+1}/{len(DEMO_SEQUENCE)}] {desc}")
        if msg is None:
            await asyncio.sleep(2.0)
        elif isinstance(msg, dict) and msg.get("type") == "animation":
            # 动画后等待一会儿让动画播放完
            await robot.send(msg)
            await asyncio.sleep(1.5)
        elif isinstance(msg, dict) and msg.get("cmd") == "forward":
            await robot.send(msg)
            await asyncio.sleep(2.2)
        elif isinstance(msg, dict) and msg.get("cmd") == "backward":
            await robot.send(msg)
            await asyncio.sleep(1.8)
        elif isinstance(msg, dict) and msg.get("cmd") in ("left", "right"):
            await robot.send(msg)
            await asyncio.sleep(1.5)
        else:
            await robot.send(msg)
            await asyncio.sleep(0.5)

    logger.info("=== 演示序列完成 ===")


# ============================
# 交互命令行
# ============================
async def run_interactive(robot: RobotConnection):
    """交互式命令模式"""
    print("\n" + "=" * 50)
    print("  智能桌面伴侣 - 交互模式")
    print("=" * 50)
    print("  命令:")
    print("    f / forward   前进       b / back      后退")
    print("    l / left      左转       r / right     右转")
    print("    s / stop      停止       e / explore   探索")
    print("    happy         开心表情    angry         愤怒")
    print("    tired         困倦        laugh         大笑")
    print("    confused      困惑        normal        默认")
    print("    screen_off    关屏        screen_on     开屏")
    print("    demo          运行演示")
    print("    q / quit      退出")
    print("=" * 50 + "\n")

    quick = {
        "f": ("motion", "forward"),
        "forward": ("motion", "forward"),
        "b": ("motion", "backward"),
        "back": ("motion", "backward"),
        "l": ("motion", "left"),
        "left": ("motion", "left"),
        "r": ("motion", "right"),
        "right": ("motion", "right"),
        "s": ("motion", "stop"),
        "stop": ("motion", "stop"),
        "e": ("move", "explore"),
        "explore": ("move", "explore"),
        "happy": ("expression", "happy"),
        "angry": ("expression", "angry"),
        "tired": ("expression", "tired"),
        "normal": ("expression", "normal"),
        "laugh": ("animation", "laugh"),
        "confused": ("animation", "confused"),
        "blink": ("animation", "blink"),
        "screen_off": ("screen", "off"),
        "screen_on": ("screen", "on"),
    }

    while True:
        try:
            cmd = input("> ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            break

        if not cmd:
            continue
        if cmd in ("q", "quit", "exit"):
            break
        if cmd in ("demo",):
            await run_demo(robot)
            continue

        if cmd in quick:
            msg_type, value = quick[cmd]
            if msg_type == "motion":
                preset = get_preset(value)
                if preset:
                    await robot.send(preset)
            elif msg_type == "expression":
                await robot.send_expression(value)
            elif msg_type == "animation":
                await robot.send_animation(value)
            elif msg_type == "screen":
                await robot.send_screen(value)
            print(f"  -> {msg_type}: {value}")
        else:
            # 尝试自然语言解析
            motion = parse_move_command(cmd)
            if motion:
                await robot.send(motion)
                print(f"  -> motion: {motion['cmd']}")
            else:
                emotion = detect_emotion(cmd)
                msg = emotion.to_message()
                if msg:
                    await robot.send(msg)
                    print(f"  -> {emotion}")
                else:
                    print(f"  无法识别: {cmd}")


# ============================
# AI 语音对话模式
# ============================
async def run_voice(robot: RobotConnection, llm_provider: str = "openai",
                    llm_model: str = "gpt-4o-mini", api_key: str = None,
                    base_url: str = None):
    """AI 语音交互主循环：VAD → ASR → LLM → TTS → 机器人表情/动作"""
    from audio.recorder import AudioRecorder
    from utils.vad import VADPipeline
    from ai.asr import ASREngine
    from ai.llm import LLMEngine
    from ai.tts import TTSEngine

    print("\n" + "=" * 50)
    print("  智能桌面伴侣 - AI 语音对话模式")
    print("=" * 50)
    print("  初始化中...")

    # 初始化各模块
    recorder = AudioRecorder()
    vad = VADPipeline(mode="energy")
    asr_engine = ASREngine(model_size="base")
    llm = LLMEngine(provider=llm_provider, model=llm_model,
                    api_key=api_key, base_url=base_url)
    tts = TTSEngine()

    # 加载 ASR 模型（首次较慢）
    print("  加载语音识别模型...")
    asr_engine.load()
    print("  准备就绪！")
    print()
    print("  按 Enter 开始说话，说完后自动识别")
    print("  说 '退出' 或 '再见' 结束对话")
    print("  Ctrl+C 退出")
    print("=" * 50 + "\n")

    recorder.start()

    try:
        while True:
            # 等待用户按 Enter 开始
            try:
                input("按 Enter 开始说话...")
            except (EOFError, KeyboardInterrupt):
                break

            print("  正在听...(说话中)", end="", flush=True)
            vad.reset()

            # 录音 + VAD 循环
            audio_data = b""
            silent_reads = 0
            while not audio_data:
                frame = recorder.read(timeout=0.1)
                if frame is None:
                    if vad.is_speaking:
                        silent_reads += 1
                        if silent_reads > 15:  # ~1.5s 无数据，强制结束
                            audio_data = vad.get_audio()
                    continue

                silent_reads = 0
                state = vad.process(frame)
                if state == "start":
                    print("\r  正在听...", end="", flush=True)
                elif state == "end":
                    audio_data = vad.get_audio()

            if not audio_data:
                print("\r  未检测到语音，请重试")
                continue

            duration = vad.get_duration_seconds()
            print(f"\r  录音完成 ({duration:.1f}s)，识别中...")

            # ASR 识别
            user_text = asr_engine.transcribe(audio_data)
            if not user_text:
                print("  没听清，请再说一次")
                continue

            print(f"  你: {user_text}")

            # 检查退出
            if any(kw in user_text for kw in ["退出", "再见", "拜拜", "bye"]):
                print("  小伴: 再见啦！下次聊~")
                await robot.send_expression("happy")
                await tts.speak("再见啦！下次聊！")
                break

            # 检查移动指令（直接发送给机器人，不经过 LLM）
            motion_cmd = parse_move_command(user_text)
            if motion_cmd:
                await robot.send(motion_cmd)
                print(f"  -> 运动指令: {motion_cmd['cmd']}")

            # LLM 对话
            print("  小伴思考中...")
            reply = await llm.chat(user_text)
            print(f"  小伴: {reply}")

            # 情绪 → 机器人表情
            emotion = detect_emotion(reply)
            emsg = emotion.to_message()
            if emsg:
                await robot.send(emsg)
                print(f"  -> 表情: {emotion}")

            # TTS 播放
            await tts.speak(reply)

    except KeyboardInterrupt:
        print("\n")
    finally:
        recorder.stop()
        print("  语音对话结束")


# ============================
# AI 语音对话模式 (机器人端音频)
# ============================
async def run_voice_robot(robot: RobotConnection, llm_provider: str = "openai",
                          llm_model: str = "gpt-4o-mini", api_key: str = None,
                          base_url: str = None):
    """使用机器人的麦克风和扬声器进行 AI 语音对话"""
    from audio.player import RobotAudioBridge
    from ai.asr import ASREngine
    from ai.llm import LLMEngine
    from ai.tts import TTSEngine

    print("\n" + "=" * 50)
    print("  智能桌面伴侣 - 机器人音频模式")
    print("  使用机器人麦克风 & 扬声器")
    print("=" * 50)
    print("  初始化中...")

    # 初始化 AI 模块
    asr_engine = ASREngine(model_size="base")
    llm = LLMEngine(provider=llm_provider, model=llm_model,
                    api_key=api_key, base_url=base_url)
    tts = TTSEngine()

    print("  加载语音识别模型...")
    asr_engine.load()

    # 音频桥接器 (机器人 mic → PC, PC TTS → 机器人 speaker)
    bridge = RobotAudioBridge(robot)
    speech_event = asyncio.Event()
    pending_audio: list[bytes] = []

    def on_mic_audio(frame_type: int, pcm_data: bytes):
        """机器人麦克风音频回调"""
        if frame_type == 0x01:
            bridge.feed_mic(frame_type, pcm_data)

    def on_speech_segment(pcm_data: bytes):
        """检测到完整语音段"""
        pending_audio.append(pcm_data)
        speech_event.set()

    bridge.on_speech(on_speech_segment)
    robot.on_binary(on_mic_audio)

    print("  准备就绪！")
    print()
    print("  对机器人说话即可，会自动识别回复")
    print("  说 '退出' 或 '再见' 结束对话")
    print("  Ctrl+C 退出")
    print("=" * 50 + "\n")

    try:
        while True:
            print("  正在听...", end="", flush=True)
            speech_event.clear()

            # 等待语音段
            try:
                await asyncio.wait_for(speech_event.wait(), timeout=60.0)
            except asyncio.TimeoutError:
                print("\r  超时未检测到语音，继续监听...")
                continue

            # 取最早的语音段
            audio_data = pending_audio.pop(0)
            duration = len(audio_data) / (16000 * 2)
            print(f"\r  录音完成 ({duration:.1f}s)，识别中...")

            # ASR 识别
            user_text = asr_engine.transcribe(audio_data)
            if not user_text:
                print("  没听清，请再说一次")
                continue

            print(f"  你: {user_text}")

            # 检查退出
            if any(kw in user_text for kw in ["退出", "再见", "拜拜", "bye"]):
                print("  小伴: 再见啦！下次聊~")
                await robot.send_expression("happy")
                # TTS → 机器人扬声器
                pcm = await tts.synthesize_pcm("再见啦！下次聊!")
                if pcm:
                    await bridge.send_tts(pcm)
                break

            # 移动指令
            motion_cmd = parse_move_command(user_text)
            if motion_cmd:
                await robot.send(motion_cmd)
                print(f"  -> 运动指令: {motion_cmd['cmd']}")

            # LLM 对话
            print("  小伴思考中...")
            reply = await llm.chat(user_text)
            print(f"  小伴: {reply}")

            # 情绪 → 机器人表情/动作
            emotion = detect_emotion(reply)
            emsg = emotion.to_message()
            if emsg:
                await robot.send(emsg)
                print(f"  -> 表情: {emotion}")

            # TTS → 机器人扬声器
            pcm = await tts.synthesize_pcm(reply)
            if pcm:
                print("  -> 音频发送到机器人...")
                await bridge.send_tts(pcm)
            else:
                # 回退到 PC 本地播放
                await tts.speak(reply)

    except KeyboardInterrupt:
        print("\n")
    finally:
        bridge.close()
        print("  机器人音频对话结束")


# ============================
# 入口
# ============================
async def main():
    parser = argparse.ArgumentParser(description="智能桌面伴侣 PC 端")
    parser.add_argument("--host", required=True, help="机器人 IP 地址")
    parser.add_argument("--port", type=int, default=81, help="WebSocket 端口 (默认 81)")
    parser.add_argument("--demo", action="store_true", help="运行演示序列")
    parser.add_argument("--voice", action="store_true", help="AI 语音对话模式 (PC 麦克风)")
    parser.add_argument("--robot-audio", action="store_true",
                        help="AI 语音对话模式 (机器人麦克风 + 扬声器)")
    parser.add_argument("--provider", default="openai", choices=["openai", "ollama"],
                        help="LLM 提供商 (默认 openai)")
    parser.add_argument("--model", default="gpt-4o-mini", help="LLM 模型名")
    parser.add_argument("--api-key", default=None, help="OpenAI API Key (也可用 OPENAI_API_KEY 环境变量)")
    parser.add_argument("--base-url", default=None, help="API Base URL (Ollama 默认 http://localhost:11434)")
    args = parser.parse_args()

    # API Key: 命令行 > 环境变量
    api_key = args.api_key or os.environ.get("OPENAI_API_KEY")

    robot = RobotConnection(host=args.host, port=args.port)

    # 注册消息回调
    def on_status(msg: dict):
        if msg.get("type") == "status":
            logger.info(f"机器人状态: battery={msg.get('battery_mv')}mV "
                        f"rssi={msg.get('rssi')} mode={msg.get('mode')}")

    robot.on_message(on_status)

    # 连接
    if not await robot.connect():
        logger.error("无法连接到机器人，请检查 IP 和网络")
        sys.exit(1)

    try:
        if args.robot_audio:
            await run_voice_robot(robot, llm_provider=args.provider, llm_model=args.model,
                                  api_key=api_key, base_url=args.base_url)
        elif args.voice:
            await run_voice(robot, llm_provider=args.provider, llm_model=args.model,
                            api_key=api_key, base_url=args.base_url)
        elif args.demo:
            await run_demo(robot)
        else:
            await run_interactive(robot)
    finally:
        await robot.disconnect()
        logger.info("已断开连接")


if __name__ == "__main__":
    asyncio.run(main())
