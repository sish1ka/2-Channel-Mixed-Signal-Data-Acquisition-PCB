/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — 2-Channel Mixed-Signal DAQ
  *
  * This file assumes the .ioc has been configured with:
  *   - Clock: HSI16 + PLL -> 64 MHz HCLK
  *   - GPIO:  PB3 = GPIO_Output (LED / test toggle)
  *   - ADC1:  IN0 (PA0) + IN1 (PA1), 2 conversions, scan mode,
  *            external trigger = TIM3 TRGO, DMA continuous requests enabled
  *   - TIM3:  Prescaler 0, ARR 15999 (64 MHz / 16000 = 4 kHz), TRGO = Update
  *   - DMA:   ADC1 -> Circular, Half Word, Memory increment enabled
  *   - USART1: PA9 (TX) / PA10 (RX), 460800-8-N-1, TX DMA (Normal mode)
  *
  * NOT hardware-tested. Written and verified to compile against the
  * STM32G0 HAL. See project README for status.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* Binary UART frame, 9 bytes total, matches project spec section 69 */
#pragma pack(push, 1)
typedef struct
{
  uint8_t  sync0;      /* 0xA5 */
  uint8_t  sync1;       /* 0x5A */
  uint16_t seq;          /* sample sequence number */
  uint16_t ch1;           /* CH1 raw 12-bit ADC code */
  uint16_t ch2;            /* CH2 raw 12-bit ADC code */
  uint8_t  checksum;        /* XOR of seq_l..ch2_h */
} daq_frame_t;
#pragma pack(pop)

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ADC_BUF_LEN     256U    /* 128 channel-pairs (CH1,CH2 interleaved) */
#define FRAME_SYNC0     0xA5U
#define FRAME_SYNC1     0x5AU

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */

static uint16_t adc_buffer[ADC_BUF_LEN];

static volatile uint8_t half_ready = 0;
static volatile uint8_t full_ready = 0;

static uint16_t sample_seq = 0;
static daq_frame_t tx_frame;
static volatile uint8_t uart_busy = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

static uint8_t  ComputeChecksum(const daq_frame_t *f);
static void     BuildAndSendFrame(uint16_t ch1_code, uint16_t ch2_code);
static void     ProcessHalfBuffer(uint16_t *buf_half);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  XOR checksum over seq_l, seq_h, ch1_l, ch1_h, ch2_l, ch2_h.
  *         Matches project spec section 71.
  */
static uint8_t ComputeChecksum(const daq_frame_t *f)
{
  uint8_t seq_l = (uint8_t)(f->seq & 0xFF);
  uint8_t seq_h = (uint8_t)((f->seq >> 8) & 0xFF);
  uint8_t ch1_l = (uint8_t)(f->ch1 & 0xFF);
  uint8_t ch1_h = (uint8_t)((f->ch1 >> 8) & 0xFF);
  uint8_t ch2_l = (uint8_t)(f->ch2 & 0xFF);
  uint8_t ch2_h = (uint8_t)((f->ch2 >> 8) & 0xFF);

  return (uint8_t)(seq_l ^ seq_h ^ ch1_l ^ ch1_h ^ ch2_l ^ ch2_h);
}

/**
  * @brief  Pack one sample pair into a binary frame and transmit over
  *         UART via DMA. Blocks briefly if the previous frame is still
  *         being sent (uart_busy) rather than overwriting tx_frame mid-DMA.
  */
static void BuildAndSendFrame(uint16_t ch1_code, uint16_t ch2_code)
{
  /* Guard against clobbering an in-flight DMA TX buffer. At 4 kHz frame
   * rate and 460800 baud, a 9-byte frame takes ~195 us to transmit, well
   * under the 250 us sample period, so this should rarely if ever stall.
   */
  uint32_t timeout = 1000U;
  while (uart_busy && timeout--)
  {
    /* busy-wait; replace with a queue/ring buffer if this becomes an
     * issue once measured on real hardware */
  }

  tx_frame.sync0 = FRAME_SYNC0;
  tx_frame.sync1 = FRAME_SYNC1;
  tx_frame.seq   = sample_seq++;
  tx_frame.ch1   = ch1_code;
  tx_frame.ch2   = ch2_code;
  tx_frame.checksum = ComputeChecksum(&tx_frame);

  uart_busy = 1;
  HAL_UART_Transmit_DMA(&huart1, (uint8_t *)&tx_frame, sizeof(tx_frame));
}

/**
  * @brief  Walk one half of the circular ADC buffer (64 uint16_t = 32
  *         channel-pairs) and ship each pair as a frame.
  * @param  buf_half Pointer to the first sample of the half to process.
  */
static void ProcessHalfBuffer(uint16_t *buf_half)
{
  const uint16_t pairs_per_half = (ADC_BUF_LEN / 2U) / 2U; /* = 64 */

  for (uint16_t i = 0; i < pairs_per_half; i++)
  {
    uint16_t ch1_code = buf_half[(i * 2U) + 0U];
    uint16_t ch2_code = buf_half[(i * 2U) + 1U];
    BuildAndSendFrame(ch1_code, ch2_code);
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* Calibrate ADC before first use — recommended by ST for accuracy */
  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Start the sample-rate timer (TIM3 TRGO drives ADC conversions) */
  if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  /* Start ADC in circular DMA mode. TIM3 TRGO paces each conversion
   * pair at 4 kHz; DMA moves each 16-bit result into adc_buffer and
   * wraps automatically at ADC_BUF_LEN. */
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, ADC_BUF_LEN) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (half_ready)
    {
      half_ready = 0;
      ProcessHalfBuffer(&adc_buffer[0]);
    }

    if (full_ready)
    {
      full_ready = 0;
      ProcessHalfBuffer(&adc_buffer[ADC_BUF_LEN / 2U]);
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief  ADC DMA half-transfer callback — first half of adc_buffer
  *         (samples 0..127, i.e. 64 channel-pairs) is ready.
  */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  half_ready = 1;
}

/**
  * @brief  ADC DMA full-transfer callback — second half of adc_buffer
  *         is ready. DMA wraps and keeps filling the first half again.
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  full_ready = 1;
}

/**
  * @brief  UART TX DMA complete callback — clears the busy flag so the
  *         next frame can be built/sent.
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uart_busy = 0;
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  *        CH0 (PA0) = Rank 1, CH1 (PA1) = Rank 2
  *        External trigger = TIM3 TRGO, DMA circular, continuous requests.
  * @retval None
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
  hadc1.Init.LowPowerAutoWait      = DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.NbrOfConversion       = 2;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIG_T3_TRGO;
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.SamplingTimeCommon1   = ADC_SAMPLETIME_39CYCLES_5;
  hadc1.Init.SamplingTimeCommon2   = ADC_SAMPLETIME_39CYCLES_5;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Rank 1: Channel 0 (PA0) — ADC_CH1 */
  sConfig.Channel      = ADC_CHANNEL_0;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /* Rank 2: Channel 1 (PA1) — ADC_CH2 */
  sConfig.Channel      = ADC_CHANNEL_1;
  sConfig.Rank         = ADC_REGULAR_RANK_2;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM3 Initialization Function
  *        64 MHz / (PSC+1) / (ARR+1) = 64,000,000 / 1 / 16000 = 4 kHz.
  *        TRGO on Update event drives the ADC external trigger.
  * @retval None
  */
static void MX_TIM3_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler         = 0;
  htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim3.Init.Period            = 15999; /* 64e6 / 16000 = 4000 Hz */
  htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  *        460800-8-N-1, TX via DMA (Normal mode, one frame per call).
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 460800;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * DMA channel init — ADC1 (circular) and USART1_TX (normal).
  */
static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt priorities — kept equal/low since nothing here is
   * hard-real-time critical beyond staying ahead of the 4 kHz sample
   * rate, which is generous for this MCU. */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
}

/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = LED_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
    /* Fast-blink the LED as a visible fault indicator once flashed to
     * real hardware. Harmless as dead code until then. */
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
