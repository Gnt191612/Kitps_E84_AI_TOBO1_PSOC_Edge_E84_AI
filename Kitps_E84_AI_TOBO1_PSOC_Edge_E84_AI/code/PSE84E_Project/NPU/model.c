#include "model.h"
#include "npu.h"
#include "model/model_weights.h"

void Model_Load(void)
{
    NPU_LoadWeights(model_weights);
    NPU_SetInputSize(MODEL_INPUT_W, MODEL_INPUT_H);
}