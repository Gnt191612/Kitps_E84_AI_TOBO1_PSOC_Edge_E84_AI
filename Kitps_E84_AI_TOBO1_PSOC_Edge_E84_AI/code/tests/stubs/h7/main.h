/* 电脑端测试用的 STM32 HAL 替身：只提供被测代码用到的类型、宏和函数 */
#ifndef TEST_STUB_MAIN_H
#define TEST_STUB_MAIN_H

#include <stdint.h>
#include <stddef.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

typedef struct { int id; } TIM_TypeDef;
typedef struct { int id; } GPIO_TypeDef;
typedef struct { TIM_TypeDef *Instance; int Channel; } TIM_HandleTypeDef;
typedef struct { int id; } UART_HandleTypeDef;

extern TIM_TypeDef stub_TIM2, stub_TIM3;
extern GPIO_TypeDef stub_GPIOA, stub_GPIOB, stub_GPIOC, stub_GPIOD;
#define TIM2   (&stub_TIM2)
#define TIM3   (&stub_TIM3)
#define GPIOA  (&stub_GPIOA)
#define GPIOB  (&stub_GPIOB)
#define GPIOC  (&stub_GPIOC)
#define GPIOD  (&stub_GPIOD)

#define TIM_CHANNEL_1  0x00U
#define TIM_CHANNEL_2  0x04U
#define TIM_CHANNEL_3  0x08U
#define TIM_CHANNEL_4  0x0CU
#define HAL_TIM_ACTIVE_CHANNEL_1  0x01
#define TIM_INPUTCHANNELPOLARITY_RISING   0
#define TIM_INPUTCHANNELPOLARITY_FALLING  1

#define GPIO_PIN_0   0x0001U
#define GPIO_PIN_1   0x0002U
#define GPIO_PIN_2   0x0004U
#define GPIO_PIN_3   0x0008U
#define GPIO_PIN_6   0x0040U
#define GPIO_PIN_7   0x0080U
#define GPIO_PIN_8   0x0100U
#define GPIO_PIN_9   0x0200U
#define GPIO_PIN_12  0x1000U
#define GPIO_PIN_13  0x2000U

extern TIM_HandleTypeDef htim2, htim3, htim6;
extern UART_HandleTypeDef huart1, huart2, huart3, huart4;

/* 时基：由测试推进 */
extern uint32_t stub_tick;
static inline uint32_t HAL_GetTick(void) { return stub_tick; }
static inline void HAL_Delay(uint32_t ms) { stub_tick += ms; }

/* PWM 比较值：记录到替身，供测试检查 */
void stub_set_compare(TIM_HandleTypeDef *h, uint32_t ch, uint32_t v);
#define __HAL_TIM_SET_COMPARE(h, ch, v)  stub_set_compare((h), (ch), (v))
static inline HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, uint32_t ch)
{ (void)h; (void)ch; return HAL_OK; }
static inline void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s)
{ (void)p; (void)pin; (void)s; }

/* 输入捕获（雷达驱动测试用） */
extern uint32_t stub_capture_value;
#define __HAL_TIM_SET_CAPTUREPOLARITY(h, ch, p)  ((void)(h), (void)(ch), (void)(p))
static inline HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *h, uint32_t ch)
{ (void)h; (void)ch; return HAL_OK; }
static inline uint32_t HAL_TIM_ReadCapturedValue(TIM_HandleTypeDef *h, uint32_t ch)
{ (void)h; (void)ch; return stub_capture_value; }
#define __NOP()  ((void)0)

void Error_Handler(void);

#endif
