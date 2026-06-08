"""
大模型对话 (LLM) —— 支持 OpenAI API 和 Ollama 本地模型

用法:
  llm = LLMEngine(provider="openai", model="gpt-4o-mini")
  reply = await llm.chat("你好")
"""

from __future__ import annotations

import logging
from typing import Optional

logger = logging.getLogger(__name__)

SYSTEM_PROMPT = """你是一个名叫"小伴"的桌面机器人助手。你住在用户的电脑桌上，是一台小型两轮机器人，有可爱的OLED眼睛可以表达情绪。

你的性格特点:
- 活泼可爱，说话语气像朋友一样轻松自然
- 喜欢简短的回答（1-3句话），像聊天一样
- 偶尔用拟声词（哈哈、嗯、哇、咦）
- 你能移动（前进、后退、左转、右转），也能表达开心、生气、困倦、困惑等表情

重要规则:
- 如果用户让你移动，回复中要包含"移动:"加上方向（如"移动:前进"）
- 每次回复尽量简短，像真人对话"""


class LLMEngine:
    """LLM 对话引擎"""

    def __init__(self, provider: str = "openai", model: str = "gpt-4o-mini",
                 api_key: Optional[str] = None, base_url: Optional[str] = None):
        self.provider = provider  # "openai" / "ollama"
        self.model = model
        self.api_key = api_key
        self.base_url = base_url
        self._client = None
        self._history: list[dict] = []

    def reset_history(self):
        """清空对话历史"""
        self._history = []

    async def chat(self, user_text: str) -> str:
        """发送消息并获取回复"""
        if not user_text.strip():
            return ""

        if self.provider == "ollama":
            return await self._chat_ollama(user_text)
        else:
            return await self._chat_openai(user_text)

    async def _chat_openai(self, user_text: str) -> str:
        from openai import AsyncOpenAI

        if self._client is None:
            self._client = AsyncOpenAI(api_key=self.api_key, base_url=self.base_url)

        # 首次对话加入 system prompt
        if not self._history:
            self._history.append({"role": "system", "content": SYSTEM_PROMPT})

        self._history.append({"role": "user", "content": user_text})

        try:
            resp = await self._client.chat.completions.create(
                model=self.model,
                messages=self._history,
                max_tokens=200,
                temperature=0.8,
            )
            reply = resp.choices[0].message.content.strip()
            self._history.append({"role": "assistant", "content": reply})

            # 控制历史长度
            if len(self._history) > 20:
                self._history = self._history[:1] + self._history[-10:]

            logger.info(f"LLM: {reply}")
            return reply
        except Exception as e:
            logger.error(f"OpenAI 调用失败: {e}")
            return "嗯...我好像没听清，再说一次？"

    async def _chat_ollama(self, user_text: str) -> str:
        import aiohttp

        if not self._history:
            self._history.append({"role": "system", "content": SYSTEM_PROMPT})

        self._history.append({"role": "user", "content": user_text})

        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    f"{self.base_url or 'http://localhost:11434'}/api/chat",
                    json={
                        "model": self.model,
                        "messages": self._history,
                        "stream": False,
                    },
                ) as resp:
                    data = await resp.json()
                    reply = data["message"]["content"].strip()
                    self._history.append({"role": "assistant", "content": reply})

                    if len(self._history) > 20:
                        self._history = self._history[:1] + self._history[-10:]

                    logger.info(f"LLM: {reply}")
                    return reply
        except Exception as e:
            logger.error(f"Ollama 调用失败: {e}")
            return "嗯...脑袋有点卡，再说一次？"
