#!/usr/bin/env python3
"""检查 3D 工具的连线表（sim_3d/nets.js）是否与 WIRING.md 一致。

用法：python check_sim_wiring.py
逐对检查 H7 引脚和对端，发现不一致时返回非零。
"""

import os
import re
import sys

here = os.path.dirname(os.path.abspath(__file__))
nets = open(os.path.join(here, "sim_3d", "nets.js"), encoding="utf-8").read()
wiring = open(os.path.join(here, "..", "WIRING.md"), encoding="utf-8").read()

# WIRING.md 中写明的连接：H7 引脚 → (nets.js 里的对端, WIRING.md 里应出现的关键字)
EXPECT = {
    "PB6": ("LS.HV1", "PB6 (USART1_TX)"),
    "PB7": ("LS.HV2", "PB7 (USART1_RX)"),
    "PD5": ("A.G2", "PD5 (USART2 TX)"),
    "PD6": ("A.G1", "PD6 (USART2 RX)"),
    "PD8": ("B.G2", "PD8 (USART3 TX)"),
    "PD9": ("B.G1", "PD9 (USART3 RX)"),
    "PC0": ("R.TRIG", "PC0"),
    "PC6": ("BB.DIV_OUT", "PC6"),
    "PC10": ("TTL.RXD", "PC10 (UART4_TX)"),
    "PC11": ("TTL.TXD", "PC11 (UART4_RX)"),
    "PA0": ("SR.S", "PA0 (TIM2_CH1"),
    "PA1": ("SAP.S", "PA1 (TIM2_CH2"),
    "PA2": ("SAT.S", "PA2 (TIM2_CH3"),
    "PA3": ("SBP.S", "PA3 (TIM2_CH4"),
    "PC7": ("SBT.S", "PC7 (TIM3_CH2"),
    "PB0": ("SEP.S", "PB0 (TIM3_CH3"),
    "PB1": ("SET.S", "PB1 (TIM3_CH4"),
}

pairs = re.findall(r"add\('\w+', '[^']*', '([\w.]+)', '([\w.]+)'", nets)
servo = re.findall(r"\['(S\w+)', '[^']*', '(H7\.\w+)', '", nets)
links = {frozenset(p) for p in pairs} | {frozenset((s + ".S", h)) for s, h in servo}

errors = []
for pin, (other, key) in EXPECT.items():
    if frozenset(("H7." + pin, other)) not in links:
        errors.append("nets.js 缺少连接 H7.%s - %s" % (pin, other))
    if key not in wiring:
        errors.append("WIRING.md 里找不到 “%s”" % key)
for key, why in (("P9_3", "J5 接收"), ("P9_2", "J5 发送"), ("VDD_1V8", "J5 第 1 针")):
    if key not in wiring:
        errors.append("WIRING.md 没有写明 %s（%s）" % (key, why))

if errors:
    print("\n".join(errors))
    sys.exit(1)
print("sim_3d/nets.js 与 WIRING.md 一致：%d 项连接" % len(EXPECT))
