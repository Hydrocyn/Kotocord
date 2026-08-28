# tts_sidecar.py — Edge-TTS 合成侧车 (决策 D9: QProcess 驱动)
# 用法: python tts_sidecar.py <文本> <输出mp3路径>
# 由 PythonEdgeTTS (C++) 启动; 成功=退出码 0 + 产物文件; 失败=非零 + stderr
import asyncio
import sys

import edge_tts


async def main() -> None:
    if len(sys.argv) < 3:
        print("usage: tts_sidecar.py <text> <out.mp3>", file=sys.stderr)
        sys.exit(2)
    try:
        await edge_tts.Communicate(sys.argv[1]).save(sys.argv[2])
    except Exception as e:  # noqa: BLE001 — 任何失败都要让父进程感知
        print(f"tts_sidecar error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
