//推理接口
#ifndef INFER_H
#define INFER_H

#include <stdint.h>
#include "utils/postprocess.h"

NPU_Result_t Infer_Run(uint8_t *image_data);

#endif