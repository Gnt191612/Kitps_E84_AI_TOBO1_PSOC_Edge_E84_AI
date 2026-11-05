#!/usr/bin/env python3
"""
KIT_PSE84_AI OV7675 数据集采集（PC 端）
========================================
接收 CM55 固件在 DATASET_CAPTURE_MODE=1 下通过调试串口（KitProg3 USB-UART）发出的
160x120 灰度帧，即模型实际看到的输入，并保存为 PNG：

    <out>/<label>/<scene>_<distance>_<light>_<n>.png

依赖：pyserial、Pillow（pip install pyserial pillow）。

帧格式（小端，见 proj_cm55/source/dataset_capture.h）：
    "DSF1" | frame_counter u32 | timestamp_ms u32 | width u16 | height u16 |
    payload_len u32 | conf_milli u16 | reserved u16 | payload | crc32 u32
    crc32 = zlib.crc32(bytes[4 : 24 + payload_len])

用法示例：
    python capture_dataset.py --port COM7 --label human --scene lab --distance 2m --light day
    python capture_dataset.py --port COM7 --label background --scene corridor --distance 4m --light dim --count 60
    python capture_dataset.py --selftest          # 不接板子，自检帧解析
"""

import argparse
import os
import re
import struct
import sys
import time
import zlib

MAGIC = b"DSF1"
HEADER = struct.Struct("<4sIIHHIHH")      # 24 B
HEADER_LEN = HEADER.size
MAX_PAYLOAD = 320 * 240                   # sanity limit
LABELS = ("human", "background")


def build_frame(counter, width, height, pixels, conf_milli=0, ts_ms=0):
    """Same layout as the firmware (used by --selftest)."""
    hdr = HEADER.pack(MAGIC, counter, ts_ms, width, height, len(pixels), conf_milli, 0)
    crc = zlib.crc32(hdr[4:] + pixels) & 0xFFFFFFFF
    return hdr + pixels + struct.pack("<I", crc)


class FrameParser:
    """Finds DSF1 frames in a byte stream that also carries printf() text."""

    def __init__(self):
        self.buf = bytearray()
        self.crc_errors = 0
        self.text = bytearray()

    def feed(self, data):
        self.buf += data
        frames = []
        while True:
            idx = self.buf.find(MAGIC)
            if idx < 0:
                # keep a possible partial magic at the end
                keep = len(MAGIC) - 1
                self.text += self.buf[:-keep] if len(self.buf) > keep else b""
                del self.buf[:-keep or None]
                break
            if idx > 0:
                self.text += self.buf[:idx]
                del self.buf[:idx]
            if len(self.buf) < HEADER_LEN:
                break
            _, counter, ts_ms, w, h, plen, conf, _ = HEADER.unpack_from(self.buf, 0)
            if plen != w * h or plen == 0 or plen > MAX_PAYLOAD:
                del self.buf[:1]                  # false magic in text, resync
                continue
            total = HEADER_LEN + plen + 4
            if len(self.buf) < total:
                break
            body = bytes(self.buf[4:HEADER_LEN + plen])
            (crc_rx,) = struct.unpack_from("<I", self.buf, HEADER_LEN + plen)
            if (zlib.crc32(body) & 0xFFFFFFFF) != crc_rx:
                self.crc_errors += 1
                del self.buf[:1]
                continue
            frames.append({
                "counter": counter, "ts_ms": ts_ms, "width": w, "height": h,
                "conf": conf / 1000.0, "pixels": bytes(self.buf[HEADER_LEN:HEADER_LEN + plen]),
            })
            del self.buf[:total]
        return frames

    def pop_text_lines(self):
        lines = []
        while b"\n" in self.text:
            line, _, rest = self.text.partition(b"\n")
            self.text = bytearray(rest)
            s = line.decode("ascii", errors="replace").strip()
            if s:
                lines.append(s)
        if len(self.text) > 4096:                 # binary noise without newlines
            self.text.clear()
        return lines


def safe_token(s, name):
    if not re.fullmatch(r"[A-Za-z0-9.\-]+", s):
        sys.exit(f"--{name} '{s}' 只能包含字母、数字、'.'、'-'（'_' 用作文件名分隔符）")
    return s


def next_index(folder, prefix):
    pat = re.compile(re.escape(prefix) + r"_(\d+)\.png$")
    n = 0
    if os.path.isdir(folder):
        for f in os.listdir(folder):
            m = pat.match(f)
            if m:
                n = max(n, int(m.group(1)) + 1)
    return n


def save_png(path, frame):
    from PIL import Image
    img = Image.frombytes("L", (frame["width"], frame["height"]), frame["pixels"])
    img.save(path)


def selftest():
    import random
    import tempfile
    rnd = random.Random(0)
    frames = [bytes(rnd.randrange(256) for _ in range(160 * 120)) for _ in range(3)]
    stream = b"boot text\r\nDSF1 fake magic in text\r\n"
    stream += build_frame(0, 160, 120, frames[0], 873)
    stream += b"[HUMAN_DETECT] printf between frames\r\n"
    bad = bytearray(build_frame(1, 160, 120, frames[1]))
    bad[100] ^= 0xFF                              # corrupt payload -> CRC error
    stream += bytes(bad)
    stream += build_frame(2, 160, 120, frames[2], 12)

    p = FrameParser()
    got = []
    for i in range(0, len(stream), 997):          # arbitrary chunking
        got += p.feed(stream[i:i + 997])
    assert [f["counter"] for f in got] == [0, 2], [f["counter"] for f in got]
    assert got[0]["pixels"] == frames[0] and got[1]["pixels"] == frames[2]
    assert abs(got[0]["conf"] - 0.873) < 1e-9
    assert p.crc_errors >= 1
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "t.png")
        save_png(path, got[0])
        from PIL import Image
        assert Image.open(path).tobytes() == frames[0]
    print(f"selftest OK: 2 frames parsed, {p.crc_errors} CRC error(s) detected, PNG round-trip exact")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="KitProg3 串口，例如 COM7 或 /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=921600, help="与固件 DATASET_CAPTURE_BAUD 一致（默认 921600）")
    ap.add_argument("--label", choices=LABELS)
    ap.add_argument("--scene", help="场景名，例如 lab、corridor；建议带日期，如 lab-1104")
    ap.add_argument("--distance", help="距离档，例如 0.5m、2m、8-10m")
    ap.add_argument("--light", help="光照，例如 day、fluor、dim、backlit、night")
    ap.add_argument("--out", default="dataset", help="输出根目录（默认 ./dataset）")
    ap.add_argument("--count", type=int, default=0, help="保存这么多张后退出（0 = 一直采，Ctrl+C 结束）")
    ap.add_argument("--quiet", action="store_true", help="不回显固件的文本日志")
    ap.add_argument("--selftest", action="store_true", help="不接硬件，自检帧解析和 PNG 保存")
    a = ap.parse_args()

    if a.selftest:
        selftest()
        return

    missing = [n for n in ("port", "label", "scene", "distance", "light") if getattr(a, n) is None]
    if missing:
        ap.error("缺少参数：" + ", ".join("--" + n for n in missing))
    prefix = "_".join(safe_token(getattr(a, n), n) for n in ("scene", "distance", "light"))
    folder = os.path.join(a.out, a.label)
    os.makedirs(folder, exist_ok=True)
    n = next_index(folder, prefix)

    import serial
    ser = serial.Serial(a.port, a.baud, timeout=0.1)
    parser = FrameParser()
    saved = 0
    last_counter = None
    lost = 0
    t0 = time.time()
    print(f"[capture] {a.port} @ {a.baud} -> {folder}{os.sep}{prefix}_NNNN.png (start index {n})")
    print("[capture] 板上 SW1 短按 = 采 1 帧，长按 >= 1 s = 开/关连续采集。Ctrl+C 结束。")
    try:
        while a.count == 0 or saved < a.count:
            data = ser.read(8192)
            if not data:
                continue
            for f in parser.feed(data):
                if last_counter is not None and f["counter"] != last_counter + 1:
                    lost += (f["counter"] - last_counter - 1) & 0xFFFFFFFF
                last_counter = f["counter"]
                path = os.path.join(folder, f"{prefix}_{n:04d}.png")
                save_png(path, f)
                n += 1
                saved += 1
                print(f"[capture] {saved:5d} saved  {os.path.basename(path)}  "
                      f"model p(human)={f['conf']:.2f}  frame#{f['counter']}  "
                      f"crc_err={parser.crc_errors} lost={lost}  {saved / (time.time() - t0):.2f} fps")
                if a.count and saved >= a.count:
                    break
            if not a.quiet:
                for line in parser.pop_text_lines():
                    print(f"  | {line}")
            else:
                parser.pop_text_lines()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
    print(f"[capture] done: {saved} saved to {folder}, CRC errors {parser.crc_errors}, frames lost {lost}")


if __name__ == "__main__":
    main()
