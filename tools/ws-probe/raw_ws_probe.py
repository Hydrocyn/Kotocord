# raw_ws_probe.py — 捕获客户端 HTTP 升级请求的原始字节 (L2 诊断探针)
# 用法: python raw_ws_probe.py [port=10096]
# 配合: tts_client_cli.exe <文本> out.mp3 ws://127.0.0.1:<port>  (捕获 Qt 的请求)
#       python aiohttp_probe.py ws://127.0.0.1:<port>            (捕获 aiohttp 的请求)
# 对比两份输出 = 字节级差异定位
import socket
import sys


def main(port: int = 10096) -> None:
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(5)
    print(f"[probe] listening on 127.0.0.1:{port}", flush=True)

    while True:
        conn, addr = srv.accept()
        conn.settimeout(2)
        data = b""
        try:
            while b"\r\n\r\n" not in data and len(data) < 65536:
                data += conn.recv(4096)
        except Exception:
            pass
        print("===== REQUEST START =====", flush=True)
        print(data.decode("utf-8", "replace"), flush=True)
        print("===== REQUEST END =====", flush=True)
        conn.close()


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 10096)
