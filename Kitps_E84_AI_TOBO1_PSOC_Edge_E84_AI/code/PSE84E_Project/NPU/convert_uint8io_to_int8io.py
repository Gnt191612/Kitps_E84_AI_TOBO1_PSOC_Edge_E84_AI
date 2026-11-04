#!/usr/bin/env python3
"""
把 train_npu_model.py 导出的 uint8 输入/输出 TFLite 无损改成 int8 输入/输出。

原因: ml-coretools 3.1 (PSE84_M55_U55, int8x8) 只接受 int8 I/O，
      对 uint8 I/O 模型报 "Unsupported data types: input=3, output=3"。

做法: uint8 模型首尾各有一个 QUANTIZE 算子 (uint8<->int8，scale 不变，zero_point 差 128)。
      直接删掉这两个算子，把子图的输入/输出改成内部的 int8 张量，权重和量化参数不动。

用法:
  python convert_uint8io_to_int8io.py output/model_quantized.tflite output/model_quantized_int8io.tflite

转换后 (本模型):
  输入  int8 [1,120,160,1], scale=1.0,        zero_point=-128  ->  q = pixel - 128
  输出  int8 [1,2],         scale=1/256,      zero_point=-128  ->  p = (q + 128) / 256
"""

import sys
from tensorflow.lite.tools import flatbuffer_utils as fu
from tensorflow.lite.python import schema_py_generated as s

INT8 = s.TensorType.INT8
UINT8 = s.TensorType.UINT8


def op_name(model, op):
    code = model.operatorCodes[op.opcodeIndex]
    return max(code.builtinCode, code.deprecatedBuiltinCode)


def main(src, dst):
    model = fu.read_model(src)
    g = model.subgraphs[0]
    remap = {}

    first = g.operators[0]
    if op_name(model, first) == s.BuiltinOperator.QUANTIZE and \
            g.tensors[first.inputs[0]].type == UINT8 and g.tensors[first.outputs[0]].type == INT8:
        remap[int(first.inputs[0])] = int(first.outputs[0])
        g.inputs = [int(first.outputs[0]) if int(i) == int(first.inputs[0]) else int(i) for i in g.inputs]
        g.operators = g.operators[1:]

    last = g.operators[-1]
    if op_name(model, last) == s.BuiltinOperator.QUANTIZE and \
            g.tensors[last.inputs[0]].type == INT8 and g.tensors[last.outputs[0]].type == UINT8:
        remap[int(last.outputs[0])] = int(last.inputs[0])
        g.outputs = [int(last.inputs[0]) if int(o) == int(last.outputs[0]) else int(o) for o in g.outputs]
        g.operators = g.operators[:-1]

    if not remap:
        sys.exit('未找到 uint8<->int8 边界 QUANTIZE 算子，模型可能已是 int8 I/O')

    # SignatureDef 引用的是张量下标，同步更新
    for sig in (model.signatureDefs or []):
        for tm in list(sig.inputs or []) + list(sig.outputs or []):
            tm.tensorIndex = remap.get(int(tm.tensorIndex), int(tm.tensorIndex))

    fu.write_model(model, dst)

    for name, idx in (('input', g.inputs[0]), ('output', g.outputs[0])):
        t = g.tensors[idx]
        print(f'{name}: type={t.type} shape={list(t.shape)} '
              f'scale={list(t.quantization.scale)} zero_point={list(t.quantization.zeroPoint)}')
    print(f'written: {dst}')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
