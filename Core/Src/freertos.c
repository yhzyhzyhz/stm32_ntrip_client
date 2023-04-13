/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "api.h"
#include "dhcp.h"
#include "netif.h"
#include "ntrip.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern struct netif gnetif;  // lwip.c
extern UART_HandleTypeDef huart3; // usart.c

/******************************Configurations**********************************/
uint8_t pui8IpAddr[4] = {192, 168, 10, 102};
uint16_t ui16Port = 2101;
uint16_t ui16LocalPort = 12345;
char sMountpoint[20] = "TEST";
char sUsername[20] = "username";
char sPassword[20] = "password";
/******************************Configurations*END******************************/
uint8_t bNtripRun = 1;
uint8_t pui8Data[1024] = {0};
uint8_t ui8DataLength = 0;

osThreadId ntripHandle;
osThreadId blinkHandle;

/* USER CODE END Variables */
osThreadId start_taskHandle;
uint32_t start_taskBuffer[ 128 ];
osStaticThreadDef_t start_taskControlBlock;
osSemaphoreId NtripChangeHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void ntrip_process(void const * argument);
void blink_process(void const * argument);

/* USER CODE END FunctionPrototypes */

void startup(void const * argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of NtripChange */
  osSemaphoreDef(NtripChange);
  NtripChangeHandle = osSemaphoreCreate(osSemaphore(NtripChange), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of start_task */
  osThreadStaticDef(start_task, startup, osPriorityAboveNormal, 0, 128, start_taskBuffer, &start_taskControlBlock);
  start_taskHandle = osThreadCreate(osThread(start_task), NULL);

  /* USER CODE BEGIN RTOS_THREADS */

  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_startup */
/**
  * @brief  Function implementing the start_task thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_startup */
void startup(void const * argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN startup */
  while (!dhcp_supplied_address(&gnetif))
  {
    HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_SET);
    osDelay(1000);
  }
  HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_RESET);

  int32_t i32SemaphoreStatus = osOK;

  /* Infinite loop */
  for(;;)
  {
    i32SemaphoreStatus = osSemaphoreWait(NtripChangeHandle, 10000);
    if (i32SemaphoreStatus == osOK)
    {
      if (bNtripRun == 0)
      {
	osThreadTerminate(blinkHandle);
	HAL_GPIO_WritePin(GPIOB, LD1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, LD3_Pin, GPIO_PIN_RESET);
      }
      else
      {
	osThreadDef(ntrip, ntrip_process, osPriorityHigh, 0, 2048);
	ntripHandle = osThreadCreate(osThread(ntrip), NULL);
	osThreadDef(blink, blink_process, osPriorityNormal, 0, 128);
	blinkHandle = osThreadCreate(osThread(blink), NULL);
	HAL_GPIO_WritePin(GPIOB, LD1_Pin, GPIO_PIN_SET);
      }
    }
  }
  /* USER CODE END startup */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void ntrip_process(void const * argument)
{
  struct netconn* pxConn;
  struct netbuf* pxRxBuf;
  err_t xErr;

  ip_addr_t xLocalIp = gnetif.ip_addr;
  char* sLocalIp = ipaddr_ntoa(&xLocalIp);
  ip_addr_t xServerIp;
  IP4_ADDR(&xServerIp, pui8IpAddr[0], pui8IpAddr[1], pui8IpAddr[2], pui8IpAddr[3]);

  pxConn = netconn_new(NETCONN_TCP);
  if (pxConn != NULL)
  {
    xErr = netconn_bind(pxConn, &xLocalIp, ui16LocalPort);
    if (xErr != ERR_OK)
    {
      printf("TCP bind err\n");
    }
    xErr = netconn_connect(pxConn, &xServerIp, ui16Port);
    if (xErr != ERR_OK)
    {
      printf("TCP Connect err\n");
    }

    ui8DataLength = sprintf((char*)pui8Data,
			    "GET /%s HTTP/1.1\r\n"
			    "Host: %s\r\n"
			    "NTRIP-Version: Ntrip/2.0\r\n"
			    "User-Agent: NTRIPstm32/TEST\r\n"
			    "Connection: close\r\n"
			    "Authorization: Basic ",
			    sMountpoint,
			    sLocalIp);
    ui8DataLength += encode((char*)pui8Data+ui8DataLength, 512-ui8DataLength-4, sUsername, sPassword);
    ui8DataLength += sprintf((char*)pui8Data+ui8DataLength, "\r\n\r\n");

    xErr = netconn_write(pxConn, (void*)pui8Data, ui8DataLength, NETCONN_COPY);
    if (xErr != ERR_OK)
    {
      printf("TCP write err\n");
    }
  }
  while(bNtripRun)
  {
    xErr = netconn_recv(pxConn, &pxRxBuf);
    if (xErr == ERR_OK)
    {
      do
      {
	HAL_UART_Transmit(&huart3, pxRxBuf->p->payload, pxRxBuf->p->len, 1000);
      }
      while (netbuf_next(pxRxBuf) >0);
      netbuf_delete(pxRxBuf);
      HAL_GPIO_TogglePin(GPIOB, LD2_Pin);
    }
    else
    {
      printf("receive err: %X", (int)xErr);
    }
  }
  netconn_close(pxConn);
  netconn_delete(pxConn);
  HAL_GPIO_WritePin(GPIOB, LD2_Pin, GPIO_PIN_RESET);
  osThreadTerminate(NULL);
}

void blink_process(void const * argument)
{
  while(1)
  {
    HAL_GPIO_TogglePin(GPIOB, LD3_Pin);
    osDelay(1000);
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == USER_Btn_Pin)
  {
    if (bNtripRun)
      bNtripRun = 0;
    else
      bNtripRun = 1;
    osSemaphoreRelease(NtripChangeHandle);
  }
}
/* USER CODE END Application */

