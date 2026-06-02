/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "can.h"
#include "stm32h7xx_hal_fdcan.h"
#include "stm32h7xx_hal_rcc_ex.h"
#include "ValveController.h"
#include "ValveStatusPacket.h"
#include "ValveCmdPacket.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define FDCAN_RX_BUFFER_SIZE 32

typedef struct {
    FDCAN_RxHeaderTypeDef header;
    uint8_t data[8];
} FDCAN_RxMessageTypeDef;

typedef struct {
    FDCAN_RxMessageTypeDef messages[FDCAN_RX_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} FDCAN_RingBufferTypeDef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
/* To comment when developping/debugging on a single core                     */
#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

// Target id is at [4:1]
#define TARGET_ID_SHIFT 4
#define TARGET_ID_MASK (0xF << TARGET_ID_SHIFT)

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

FDCAN_HandleTypeDef hfdcan1;

IWDG_HandleTypeDef hiwdg2;

TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */
FDCAN_RingBufferTypeDef fdcanRxBuffer = { .head = 0, .tail = 0 };

Valve valves[] = {
    {{&htim1, TIM_CHANNEL_1, 1200, 1800}, VALVE_STATE_UNKNOWN, 0, 100},
    {{&htim1, TIM_CHANNEL_2, 1200, 1800}, VALVE_STATE_UNKNOWN, 0, 100}
  };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_IWDG2_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /* Activate HSEM notification for Cortex-M4*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
  /*
  Domain D2 goes to STOP mode (Cortex-M4 in deep-sleep) waiting for Cortex-M7 to
  perform system initialization (system clock config, external memory configuration.. )
  */
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
  /* Clear HSEM flag */
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_IWDG2_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  valveInit(&valves[0], 100.0f);

  FDCAN_FilterTypeDef sFilterConfig;

  sFilterConfig.IdType = FDCAN_EXTENDED_ID;
  sFilterConfig.FilterIndex = 0;
  sFilterConfig.FilterType = FDCAN_FILTER_MASK;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = (CAN_NODE_ENGINE_H747 << TARGET_ID_SHIFT);
  sFilterConfig.FilterID2 = TARGET_ID_MASK;

  if(HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK){
    Error_Handler();
  }

  if(HAL_FDCAN_ConfigGlobalFilter(
    &hfdcan1,
    FDCAN_REJECT,
    FDCAN_REJECT,
    FDCAN_REJECT_REMOTE,
    FDCAN_REJECT_REMOTE
  ) != HAL_OK){
    Error_Handler();
  }

  if(HAL_FDCAN_Start(&hfdcan1) != HAL_OK){
    Error_Handler();
  }

  if(HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0)){
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (fdcanRxBuffer.tail != fdcanRxBuffer.head) 
    {
        // Pointeur sur le message actuel
        FDCAN_RxHeaderTypeDef *pHeader = &fdcanRxBuffer.messages[fdcanRxBuffer.tail].header;
        uint8_t *pData = fdcanRxBuffer.messages[fdcanRxBuffer.tail].data;

        CANHeader header;
        header.code = pHeader->Identifier;

        if (header.frame.targetID == CAN_NODE_ENGINE_H747) 
        {
          switch (header.frame.messageID)
          {
          case CAN_ID_CMD_VALVE:
          {
            CanValveIndex valve;
            CanValveCmd cmd;
            uint32_t ts;
            
            // Parse
            valveCmdPacketParse(pHeader->Identifier, pData, &valve, &cmd, &ts);

            // Ouvre/Ferme la valve
            if (valve == CAN_VALVE_1) {
              if (cmd == CAN_CMD_OPEN)  valveOpen(&(valves[0]));
              if (cmd == CAN_CMD_CLOSE) valveClose(&(valves[0]));
            }
            else if (valve == CAN_VALVE_2) {
              if (cmd == CAN_CMD_OPEN)  valveOpen(&(valves[1]));
              if (cmd == CAN_CMD_CLOSE) valveClose(&(valves[1]));
            }

            // Envoie la réponse
            ValveStatusPacket packet;
            CanValveStatus status;
            
            if(valve == CAN_VALVE_1) {
              status = (valves[0].state == VALVE_STATE_OPEN) ? CAN_STATUS_OPEN : CAN_STATUS_CLOSED;
            } else {
              status = (valves[1].state == VALVE_STATE_OPEN) ? CAN_STATUS_OPEN : CAN_STATUS_CLOSED;
            }

            valveStatusPacketMake(valve, status, &packet);

            FDCAN_TxHeaderTypeDef txHeader = {0};
            txHeader.Identifier = packet.header.code;
            txHeader.IdType = FDCAN_EXTENDED_ID;
            txHeader.TxFrameType = FDCAN_DATA_FRAME;
            txHeader.DataLength = FDCAN_DLC_BYTES_8;
            txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
            txHeader.BitRateSwitch = FDCAN_BRS_OFF;
            txHeader.FDFormat = FDCAN_CLASSIC_CAN;
            txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
            txHeader.MessageMarker = 0;

            if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) > 0)
            {
              if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, packet.payload.data) != HAL_OK)
              {
                Error_Handler();
              }
            }
            break;
          }
          default:
            break;
        }
      }

      fdcanRxBuffer.tail = (fdcanRxBuffer.tail + 1) % FDCAN_RX_BUFFER_SIZE;
    }

    for (int i = 0; i < sizeof(valves) / sizeof(valves[0]); i++) {
      valveUpdate(&valves[i]);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = ENABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 25;
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 13;
  hfdcan1.Init.NominalTimeSeg2 = 2;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 1;
  hfdcan1.Init.DataTimeSeg2 = 1;
  hfdcan1.Init.MessageRAMOffset = 0;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 1;
  hfdcan1.Init.RxFifo0ElmtsNbr = 2;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxFifo1ElmtsNbr = 0;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr = 0;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 2;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief IWDG2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG2_Init(void)
{

  /* USER CODE BEGIN IWDG2_Init 0 */

  /* USER CODE END IWDG2_Init 0 */

  /* USER CODE BEGIN IWDG2_Init 1 */

  /* USER CODE END IWDG2_Init 1 */
  hiwdg2.Instance = IWDG2;
  hiwdg2.Init.Prescaler = IWDG_PRESCALER_4;
  hiwdg2.Init.Window = 4095;
  hiwdg2.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG2_Init 2 */

  /* USER CODE END IWDG2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 99;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 3003;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
	if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0)
	{
		uint16_t next_head = (fdcanRxBuffer.head + 1) % FDCAN_RX_BUFFER_SIZE;

		if (next_head != fdcanRxBuffer.tail) 
		{
			if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &fdcanRxBuffer.messages[fdcanRxBuffer.head].header, fdcanRxBuffer.messages[fdcanRxBuffer.head].data) == HAL_OK)
			{
				fdcanRxBuffer.head = next_head;
			}
		}
		else
		{
			// ERROR Buffer full
			FDCAN_RxHeaderTypeDef dummyHeader;
			uint8_t dummyData[8];
			HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &dummyHeader, dummyData);
		}
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
