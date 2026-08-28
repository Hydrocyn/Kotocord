# aiohttp_probe.py — 用 aiohttp 连接本地探针, 让 raw_ws_probe.py 捕获其请求字节
# 用法: python aiohttp_probe.py ws://127.0.0.1:<port>
import asyncio
import sys

import aiohttp


async def main(url: str) -> None:
    async with aiohttp.ClientSession() as session:
        try:
            async with session.ws_connect(url, timeout=3) as _ws:
                print("[aiohttp] connected", flush=True)
        except Exception as e:  # noqa: BLE001 — 探针不回 101 是预期的
            print(f"[aiohttp] error: {type(e).__name__}: {e}", flush=True)


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1]))
