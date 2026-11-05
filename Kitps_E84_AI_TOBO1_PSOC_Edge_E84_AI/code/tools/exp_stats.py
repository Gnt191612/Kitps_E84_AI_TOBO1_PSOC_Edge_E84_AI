#!/usr/bin/env python3
"""
实验日志统计：把 H7 调试串口（UART4）和 ESP32 控制台的日志转成开题报告第九节的指标。

用法:
  python exp_stats.py 运行1.log [运行2.log ...]

每个文件可以是 H7、ESP32、84E 日志或它们的拼接；脚本只认下面几种行：
  H7   : $E,<tick_ms>,<事件>,<target_id>,<v1>,<v2>,<v3>      （见 STM32H7/Datalogger/exp_log.h）
  ESP32: ... $T,mode=roi3,frames=29,avg_us=4123,max_us=6011 （见 ESP32_ov2640/Core/system.c）
  ESP32: ... $C,frames=120,xor=0,bad_len=0                 （累计值）
  84E  : $I,frames=30,avg_ms=12.80,max_ms=13.10,prep_avg_ms=3.20（见 proj_cm55/source/inference_task.c）
  84E  : $C84,frames=120,err=0                             （累计值，见 h7_link.c）

对比实验：每组各录一份日志（只改一个编译开关），分别传入，按文件逐列对比。
只用标准库，Python 3.8+。
"""

import re
import statistics
import sys
from collections import defaultdict

E_RE = re.compile(r"\$E,(\d+),([A-Z0-9]+),(\d+),(-?\d+),(-?\d+),(-?\d+)")
T_RE = re.compile(r"\$T,mode=(\w+),frames=(\d+),avg_us=(\d+),max_us=(\d+)")
I_RE = re.compile(r"\$I,frames=(\d+),avg_ms=([\d.]+),max_ms=([\d.]+),prep_avg_ms=([\d.]+)")
C_RE = re.compile(r"\$C,frames=(\d+),xor=(\d+),bad_len=(\d+)")
C84_RE = re.compile(r"\$C84,frames=(\d+),err=(\d+)")
HUMAN_CONF = 0.5   # 与 H7 RECOG_HUMAN_CONF、84E HUMAN_CONF_THRESHOLD 相同


def summarize(values):
    if not values:
        return "—"
    v = sorted(values)
    p95 = v[min(len(v) - 1, int(round(0.95 * (len(v) - 1))))]
    return f"n={len(v)} 均值={statistics.mean(v):.0f} 中位={statistics.median(v):.0f} P95={p95:.0f} 最大={v[-1]:.0f}"


def ratio(ok, total):
    return f"{ok}/{total} = {100.0 * ok / total:.1f}%" if total else "—"


def accumulate(values):
    """把累计计数序列还原为总量；计数变小说明中途复位，从新值接着累加。"""
    total = prev = 0
    for v in values:
        total += v - prev if v >= prev else v
        prev = v
    return total


def error_rate(frames, errors):
    total = frames + errors
    return f"错误 {errors}/{total} = {100.0 * errors / total:.3f}%" if total else "—"


def analyze(path):
    ev = defaultdict(list)
    t_lines = defaultdict(list)
    i_lines = []
    c_lines = []
    c84_lines = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = E_RE.search(line)
            if m:
                tick, name, tid, v1, v2, v3 = m.groups()
                ev[name].append((int(tick), int(tid), int(v1), int(v2), int(v3)))
                continue
            m = T_RE.search(line)
            if m:
                mode, frames, avg_us, max_us = m.groups()
                t_lines[mode].append((int(frames), int(avg_us), int(max_us)))
                continue
            m = I_RE.search(line)
            if m:
                i_lines.append((int(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4))))
                continue
            m = C84_RE.search(line)
            if m:
                c84_lines.append((int(m.group(1)), int(m.group(2))))
                continue
            m = C_RE.search(line)
            if m:
                c_lines.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))

    r = {}

    # 84E 识别时延：按请求是否经过预判拆分（REQ84E v3=1 表示预判）
    last_req_pred = {}
    lat_all, lat_pred, lat_plain = [], [], []
    merged = sorted([(e[0], "REQ", e) for e in ev["REQ84E"]] +
                    [(e[0], "RES", e) for e in ev["RES84E"]])
    for _, kind, e in merged:
        tid = e[1]
        if kind == "REQ":
            last_req_pred[tid] = e[4]
        else:
            lat_all.append(e[3])
            (lat_pred if last_req_pred.get(tid) == 1 else lat_plain).append(e[3])
    r["84E 命令→结果时延 ms"] = summarize(lat_all)
    r["  其中预判请求"] = summarize(lat_pred)
    r["  其中非预判请求"] = summarize(lat_plain)
    r["84E 超时次数"] = str(len(ev["TO84E"]))
    confs = [e[2] / 1000.0 for e in ev["RES84E"]]
    r[f"84E 判为人体(conf>={HUMAN_CONF})"] = ratio(sum(c >= HUMAN_CONF for c in confs), len(confs))

    # NPU 纯推理耗时（目标 ≤50 ms）
    if i_lines:
        frames = sum(x[0] for x in i_lines)
        avg = sum(x[0] * x[1] for x in i_lines) / frames
        prep = sum(x[0] * x[3] for x in i_lines) / frames
        r["NPU 纯推理耗时 ms"] = f"帧={frames} 均值={avg:.2f} 最大={max(x[2] for x in i_lines):.2f}"
        r["84E 预处理耗时 ms"] = f"均值={prep:.2f}"

    # 板间丢帧/校验错误率（目标 ≤0.1%）
    for ch, name in enumerate(("84E→H7", "ESP32-A→H7", "ESP32-B→H7")):
        rows = [e for e in ev["COMM"] if e[1] == ch]
        if rows:
            frames = accumulate([e[2] for e in rows])
            errors = accumulate([e[3] for e in rows]) + accumulate([e[4] for e in rows])
            r[f"{name} 通信"] = error_rate(frames, errors)
    if c_lines:
        frames = accumulate([x[0] for x in c_lines])
        errors = accumulate([x[1] for x in c_lines]) + accumulate([x[2] for x in c_lines])
        r["H7→ESP32 通信"] = error_rate(frames, errors)
    if c84_lines:
        r["H7→84E 通信"] = error_rate(accumulate([x[0] for x in c84_lines]),
                                      accumulate([x[1] for x in c84_lines]))

    # 发现→稳定锁定（目标 ≤500 ms）
    lock = [e[3] for e in ev["LOCK"] if e[3] >= 0]
    r["发现→锁定 ms"] = summarize(lock)
    r["发现→锁定 ≤500 ms"] = ratio(sum(v <= 500 for v in lock), len(lock))

    # 跟丢后 3 s 内复锁成功率（目标 ≥80%）
    lost_n = len(ev["LOST"])
    relock_ok = [e[3] for e in ev["RELOCK"] if e[3] <= 3000]
    r["跟丢后3s内复锁"] = ratio(len(relock_ok), lost_n)
    r["复锁耗时 ms"] = summarize([e[3] for e in ev["RELOCK"]])
    r["复锁失败释放"] = str(sum(1 for e in ev["RELEASE"] if e[2] == 1))

    # 边界接力成功率（目标 ≥90%）
    r["边界接力成功"] = ratio(len(ev["HOOK"]), len(ev["HOSTART"]))
    r["接力耗时 ms"] = summarize([e[3] for e in ev["HOOK"]])

    # 视觉跟踪帧率（目标 ≥25 fps），按 ESP32 板分别统计
    for esp in (0, 1):
        fps = [e[3] for e in ev["FPS"] if e[2] == esp]
        lostf = [e[4] for e in ev["FPS"] if e[2] == esp]
        name = "A" if esp == 0 else "B"
        r[f"ESP32-{name} 回传帧率 fps"] = summarize(fps)
        if fps:
            r[f"ESP32-{name} 丢失帧占比"] = ratio(sum(lostf), sum(fps))

    # 三角定位距离与雷达距离之差（对照③参考）
    tri = [abs(e[4]) for e in ev["TRI"]]
    r["三角定位与雷达距离差(绝对值) cm"] = summarize(tri)

    # ESP32 单帧跟踪耗时（对照①）
    for mode, rows in sorted(t_lines.items()):
        frames = sum(x[0] for x in rows)
        if frames:
            avg = sum(x[0] * x[1] for x in rows) / frames
            mx = max(x[2] for x in rows)
            r[f"ESP32 单帧耗时[{mode}] us"] = f"帧={frames} 均值={avg:.0f} 最大={mx}"

    r["目标发现次数"] = str(len(ev["CAND"]))
    return r


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    results = [(p, analyze(p)) for p in sys.argv[1:]]
    keys = []
    for _, r in results:
        for k in r:
            if k not in keys:
                keys.append(k)
    print("| 指标 | " + " | ".join(p for p, _ in results) + " |")
    print("|---|" + "---|" * len(results))
    for k in keys:
        print(f"| {k} | " + " | ".join(r.get(k, "—") for _, r in results) + " |")


if __name__ == "__main__":
    main()
