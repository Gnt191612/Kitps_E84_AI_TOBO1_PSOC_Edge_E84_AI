#!/usr/bin/env python3
"""把 demo_sim.log 写成 demo_sim_data.js，供 sim_3d.html 的“内置仿真”按钮直接加载。

用法：python embed_sim_log.py
改了调度或雷达代码、更新 demo_sim.log 后运行一次。
"""

import json
import os

here = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(here, "demo_sim.log"), encoding="utf-8") as f:
    text = f.read()
with open(os.path.join(here, "demo_sim_data.js"), "w", encoding="utf-8", newline="\n") as f:
    f.write("// 由 embed_sim_log.py 生成，不要手工修改\n")
    f.write("window.DEMO_SIM_LOG = " + json.dumps(text, ensure_ascii=False) + ";\n")
print("demo_sim_data.js: %d bytes of log" % len(text))
