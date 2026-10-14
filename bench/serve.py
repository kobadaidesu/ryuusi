#!/usr/bin/env python3
"""ブラウザ版の計測用ローカルサーバー。

bin/ (make wasm の出力) を配信し、計測モードのページが POST してきた結果の JSON を
bench/results/<label>-wasm.json に保存する。

    python3 bench/serve.py [--port 8126] [--root bin] [--out bench/results]

ブラウザで次を開く (タブを前面に出しておくこと。非表示だと rAF が止まる):

    http://127.0.0.1:8126/?bench=1&label=baseline&report=/report

report に別オリジンの URL を書いた場合のために CORS も許可している。
"""

import argparse
import json
import os
import re
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer


class Handler(SimpleHTTPRequestHandler):
    out_dir = "bench/results"

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        # 計測のたびに新しい wasm を読ませる
        self.send_header("Cache-Control", "no-store")
        # cross-origin isolated にして performance.now() の分解能を上げる (100 µs → 5 µs)
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def do_POST(self):
        if self.path.split("?")[0] != "/report":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        try:
            data = json.loads(body)
        except ValueError:
            self.send_error(400, "invalid JSON")
            return
        label = re.sub(r"[^A-Za-z0-9._-]", "_", str(data.get("label", "bench")))
        platform = re.sub(r"[^A-Za-z0-9._-]", "_", str(data.get("platform", "wasm")))
        os.makedirs(self.out_dir, exist_ok=True)
        path = os.path.join(self.out_dir, f"{label}-{platform}.json")
        with open(path, "w") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
            f.write("\n")
        print(f"saved {path}", flush=True)
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(f"saved {path}\n".encode())


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", type=int, default=8126)
    ap.add_argument("--root", default="bin")
    ap.add_argument("--out", default="bench/results")
    args = ap.parse_args()

    Handler.out_dir = os.path.abspath(args.out)
    handler = partial(Handler, directory=os.path.abspath(args.root))
    server = ThreadingHTTPServer(("127.0.0.1", args.port), handler)
    print(f"serving {args.root} on http://127.0.0.1:{args.port}/  (results -> {args.out})", flush=True)
    print(f"open http://127.0.0.1:{args.port}/?bench=1&label=bench&report=/report", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
