/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "adc.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "stdbool.h"
#include <stdlib.h>   // <-- add this
#include "sw_i2c.h"
#include "mpu6050_sw.h"
#include "sw_i2c_eep.h"    // EEPROM SW I2
#include "vl53l0x/vl53l0x_api.h"	//TOF API

#include "usbd_cdc_if.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define UART_RX_BUFFER_SIZE  32
#define BT_RX_BUFFER_SIZE  32

#define ADC_MAX_COUNTS     4095UL
#define ADC_REF_MV         3300UL

#define R1_KOHM            47UL
#define R2_KOHM            22UL

/* Divider scale factor = (R1 + R2) / R2 */
#define DIV_NUMERATOR      (R1_KOHM + R2_KOHM)   // 69

// Battery limits (adjust for your chemistry)
#define BAT_FULL_MV        8400
#define BAT_LOW_MV         7000
#define BAT_CRITICAL_MV    6600
#define BAT_EMPTY_MV       6600

uint16_t bt_speed = 800;   // default speed (0–1000)
#define TURN_NUM 10
#define TURN_DEN 12

/* BUS1: MPU6050 on PC10/PC12 (your IMU pins)  */
SWI2C_Bus_t bus_imu = {
    .SCL_Port = GPIOC, .SCL_Pin = GPIO_PIN_10,
    .SDA_Port = GPIOC, .SDA_Pin = GPIO_PIN_12,
    .DelayTicks = 40
};

/* BUS2: EEPROM on PB13/PB15 (your EEPROM pins) [2](https://bosch-my.sharepoint.com/personal/tkc1cob_bosch_com/Documents/Microsoft%20Copilot%20Chat%20Files/SW_I2C_EEP.h) */
SWI2C_Bus_t bus_eep = {
    .SCL_Port = GPIOB, .SCL_Pin = GPIO_PIN_13,
    .SDA_Port = GPIOB, .SDA_Pin = GPIO_PIN_15,
    .DelayTicks = 40
};
MPU6050_Data_t imu;
//****************************TOF****************************TOF****************************TOF****************************TOF
extern I2C_HandleTypeDef hi2c1;
VL53L0X_Dev_t Dev0 = {.I2cHandle = &hi2c1, .I2cDevAddr = (uint8_t) 0x52 };
VL53L0X_Dev_t Dev1 = {.I2cHandle = &hi2c1, .I2cDevAddr = (uint8_t) 0x62 };
VL53L0X_Dev_t Dev2 = {.I2cHandle = &hi2c1, .I2cDevAddr = (uint8_t) 0x72 };
VL53L0X_Dev_t Dev3 = {.I2cHandle = &hi2c1, .I2cDevAddr = (uint8_t) 0x82 };
VL53L0X_Dev_t Dev4 = {.I2cHandle = &hi2c1, .I2cDevAddr = (uint8_t) 0x92 };
VL53L0X_Dev_t Dev5 = {.I2cHandle = &hi2c1, .I2cDevAddr = (uint8_t) 0x42 };
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#ifdef __GNUC__
/* With GCC/RAISONANCE, small msg_info (option LD Linker->Libraries->Small msg_info
 set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#define GETCHAR_PROTOTYPE int __io_getchar(void)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#define GETCHAR_PROTOTYPE int fgetc(FILE *f)
#endif /* __GNUC__ */

uint16_t map_uint(uint16_t val, uint16_t I_Min, uint16_t I_Max, uint16_t O_Min, uint16_t O_Max);
float mapf(float val, float I_Min, float I_Max, float O_Min, float O_Max);

//MOTOR:
void forward(uint16_t Duty_L, uint16_t Duty_R);
void reverse(uint16_t Duty_L, uint16_t Duty_R);
void left(uint16_t Duty_L, uint16_t Duty_R);
void right(uint16_t Duty_L, uint16_t Duty_R);
void stoprobot();
// Sensor Board
typedef enum
{
    BOARD_UNKNOWN = 0,
    BOARD_1_3TOF,
    BOARD_1_5TOF,
    BOARD_1_2TOF_1US,
    BOARD_2_5IR,
	BOARD_2_3IR,
	BOARD_2_3US,
	BOARD_2_1TOF_2US,
	BOARD_2_1US_3IR,
	BOARD_2_1US_5IR,
	BOARD_2_3US_5IR,
	BOARD_2_1TOF_5IR
} SensorBoardType_t;

SensorBoardType_t ActiveBoard = BOARD_UNKNOWN;
typedef void (*SensorFn_t)(void);

typedef struct
{
    SensorBoardType_t Type;
    const char *Name;

    uint8_t TOF_Count;
    uint8_t US_Count;
    uint8_t IR_Count;

    bool TOF_Enable;
    bool US_Enable;
    bool IR_Enable;

    SensorFn_t Init;
    SensorFn_t ReadAll;
    SensorFn_t PrintAll;
    SensorFn_t DeInit;

} SensorBoardConfig_t;

SensorBoardConfig_t ActiveSensorBoard;

/* Runtime mount-board selection state */
static uint8_t board_index = 0;
static uint32_t last_boot2_press_tick = 0;
static GPIO_PinState boot2_prev_state = GPIO_PIN_SET;
static uint32_t last_sensor_print_tick = 0;

#define SENSOR_PRINT_PERIOD_MS   200U
#define BOOT2_DEBOUNCE_MS        250U

/* Auto-detect thresholds and probe settings */
#define TOF_DEFAULT_ADDR_8BIT    0x52U
#define TOF_READY_TRIALS         2U
#define TOF_READY_TIMEOUT_MS     20U

#define US_VOLTAGE_MIN_MV        700U
#define US_VOLTAGE_MAX_MV        2500U
#define US_ADC_MIN_COUNTS        ((US_VOLTAGE_MIN_MV * ADC_MAX_COUNTS) / ADC_REF_MV)
#define US_ADC_MAX_COUNTS        ((US_VOLTAGE_MAX_MV * ADC_MAX_COUNTS) / ADC_REF_MV)
#define US_ADC_MAX_DELTA         80U
#define US_ADC_SAMPLE_COUNT      6U
#define US_ADC_SAMPLE_DELAY_MS   2U

uint16_t IR_Value[5];
//****************************************TOF
// TOF distance readings:
uint16_t Front_distance = 0;
uint16_t Left_distance = 0;
uint16_t Right_distance = 0;
uint16_t Left_diag_distance = 0;
uint16_t Right_diag_distance = 0;

bool TOF_Enable = 1;
uint8_t active_tof_sensor_count = 3;   // change during runtime
bool TOF_Debug_Print = true;           // ON/OFF print

typedef struct
{
    GPIO_TypeDef *GPIO_Port;
    uint16_t GPIO_Pin;
    VL53L0X_DEV Dev;
    const char *Name;

    bool Online; //To Detect If Sensor Attached.
} TOF_SensorConfig_t;

static TOF_SensorConfig_t TOF_Front =
{
    TOF_enableFront_GPIO_Port,
    TOF_enableFront_Pin,
    &Dev1,
    "Front",
	false
};
static TOF_SensorConfig_t TOF_Left =
{
    TOF_enableLeft_GPIO_Port,
    TOF_enableLeft_Pin,
    &Dev2,
    "Left",
	false
};
static TOF_SensorConfig_t TOF_Right =
{
    TOF_enableRight_GPIO_Port,
    TOF_enableRight_Pin,
    &Dev3,
    "Right",
	false
};
static TOF_SensorConfig_t TOF_LeftDiag =
{
    TOF_enableDiagLeft_GPIO_Port,
    TOF_enableDiagLeft_Pin,
    &Dev4,
    "Left Diagonal",
	false
};
static TOF_SensorConfig_t TOF_RightDiag =
{
    TOF_enableDiagRight_GPIO_Port,
    TOF_enableDiagRight_Pin,
    &Dev5,
    "Right Diagonal",
	false
};

void TOF_Setup_Multi(uint8_t sensor_count);
void TOF_Setup_Front(void);
void TOF_Setup_LeftRight(void);
static void TOF_Disable_All(void);
static void TOF_Enable_And_Init(TOF_SensorConfig_t *sensor);
void TOF_Read_All_Sesnor_Data(void);
static bool TOF_Address_Change(VL53L0X_DEV Dev, VL53L0X_DEV Dev_);
static bool TOFInit(VL53L0X_DEV Dev);
static void StartTOF(VL53L0X_DEV Dev);
uint16_t GetDistance(VL53L0X_DEV Dev);
static uint16_t ADC_Read_Channel(uint32_t channel);

/* Runtime mount-board helpers */
static void Runtime_GPIO_DeInit_All_Sensors(void);
static void Runtime_GPIO_Config_TOF(uint8_t count);
static void Runtime_GPIO_Config_Ultrasonic(uint8_t count);
static void Runtime_GPIO_Config_Ultrasonic_LeftRight(void);
static void Runtime_GPIO_Config_IR(uint8_t count);
static void SensorBoard_Apply(SensorBoardType_t board);
static void SensorBoard_SelectNext(void);
static void BOOT2_Task(void);
void HardwareFault_Task(void);

/* IR functions are implemented in the IR module/user section. */
void IR_Init(void);
void IR_Read_All(void);

/* Mount-board init/read/print functions */
void Sesnor_Board_1_3TOF(void);
void Read_3TOF(void);
void Print_3TOF(void);
void Sesnor_Board_1_5TOF(void);
void Read_5TOF(void);
void Print_5TOF(void);
void Sesnor_Board_1_2TOF_1Ultrasonic(void);
void Read_2TOF_1US(void);
void Print_2TOF_1US(void);
void Sensor_Board_2_3Ultrasonic(void);
void Read_3US(void);
void Print_3US(void);
void Sesnor_Board_2_1TOF_2Ultrasonic(void);
void Read_1TOF_2US(void);
void Print_1TOF_2US(void);
void Sesnor_Board_2_1TOF_5IR(void);
void Read_1TOF_5IR(void);
void Print_1TOF_5IR(void);
void Sesnor_Board_2_5IR(void);
void Read_5IR(void);
void Print_5IR(void);
void Sesnor_Board_2_3IR(void);
void Read_3IR(void);
void Print_3IR(void);
void Sesnor_Board_2_1Ultrasonic_5IR(void);
void Read_1US_5IR(void);
void Print_1US_5IR(void);
void Sesnor_Board_2_1Ultrasonic_3IR(void);
void Read_1US_3IR(void);
void Print_1US_3IR(void);
void Sesnor_Board_2_3Ultrasonic_5IR(void);
void Read_3US_5IR(void);
void Print_3US_5IR(void);

/* Mount-board auto-detection helpers */
SensorBoardType_t AutoDetectBoard(void);
uint8_t Detect_TOF_Count(void);
bool Detect_5IR(void);
bool Detect_3IR(void);
bool Detect_TOF(TOF_SensorConfig_t *sensor);
uint8_t Detect_US_Count(void);
static bool TOF_Probe_By_XSHUT(TOF_SensorConfig_t *sensor);
static bool ADC_Channel_Is_Stable_In_Range(uint32_t channel,
                                           uint16_t min_count,
                                           uint16_t max_count,
                                           uint16_t max_delta,
                                           uint8_t sample_count,
                                           uint32_t sample_delay_ms);

static SensorBoardConfig_t SensorBoards[] =
{
    { BOARD_1_3TOF,       "BOARD_1_3TOF",       3, 0, 0, true,  false, false, Sesnor_Board_1_3TOF,              Read_3TOF,       Print_3TOF,       Runtime_GPIO_DeInit_All_Sensors },
    { BOARD_1_5TOF,       "BOARD_1_5TOF",       5, 0, 0, true,  false, false, Sesnor_Board_1_5TOF,              Read_5TOF,       Print_5TOF,       Runtime_GPIO_DeInit_All_Sensors },
    { BOARD_1_2TOF_1US,   "BOARD_1_2TOF_1US",   2, 1, 0, true,  true,  false, Sesnor_Board_1_2TOF_1Ultrasonic,  Read_2TOF_1US,   Print_2TOF_1US,   Runtime_GPIO_DeInit_All_Sensors },
    { BOARD_2_3US,        "BOARD_2_3US",        0, 3, 0, false, true,  false, Sensor_Board_2_3Ultrasonic,         Read_3US,        Print_3US,        Runtime_GPIO_DeInit_All_Sensors },
    { BOARD_2_1TOF_2US,   "BOARD_2_1TOF_2US",   1, 2, 0, true,  true,  false, Sesnor_Board_2_1TOF_2Ultrasonic,  Read_1TOF_2US,   Print_1TOF_2US,   Runtime_GPIO_DeInit_All_Sensors },
    { BOARD_2_1TOF_5IR,   "BOARD_2_1TOF_5IR",   1, 0, 5, true,  false, true,  Sesnor_Board_2_1TOF_5IR,          Read_1TOF_5IR,   Print_1TOF_5IR,   Runtime_GPIO_DeInit_All_Sensors },
    { BOARD_2_5IR,        "BOARD_2_5IR",        0, 0, 5, false, false, true,  Sesnor_Board_2_5IR,               Read_5IR,        Print_5IR,        Runtime_GPIO_DeInit_All_Sensors },
    { BOARD_2_3IR,        "BOARD_2_3IR",        0, 0, 3, false, false, true,  Sesnor_Board_2_3IR,               Read_3IR,        Print_3IR,        Runtime_GPIO_DeInit_All_Sensors },
    { BOARD_2_1US_3IR,    "BOARD_2_1US_3IR",    0, 1, 3, false, true,  true,  Sesnor_Board_2_1Ultrasonic_3IR,   Read_1US_3IR,    Print_1US_3IR,    Runtime_GPIO_DeInit_All_Sensors },
	{ BOARD_2_1US_5IR,    "BOARD_2_1US_5IR",    0, 1, 5, false, true,  true,  Sesnor_Board_2_1Ultrasonic_5IR,   Read_1US_5IR,    Print_1US_5IR,    Runtime_GPIO_DeInit_All_Sensors },
	{ BOARD_2_3US_5IR,    "BOARD_2_3US_5IR",    0, 3, 5, false, true,  true,  Sesnor_Board_2_3Ultrasonic_5IR,   Read_3US_5IR,    Print_3US_5IR,    Runtime_GPIO_DeInit_All_Sensors }
};

#define SENSOR_BOARD_COUNT  (sizeof(SensorBoards) / sizeof(SensorBoards[0]))

//*****************************************Ultrasonic
uint8_t active_ultrasonic_sensor_count = 3;   // 1, 2, or 3
bool Ultrasonic_Debug_Print = true;           // true = print, false = no print

typedef struct
{
    GPIO_TypeDef *TRIG_PORT;
    uint16_t TRIG_PIN;

    GPIO_TypeDef *ECHO_PORT;
    uint16_t ECHO_PIN;

    TIM_HandleTypeDef *htim;

} Ultrasonic_t;
Ultrasonic_t US_Front =
{
	Ultrasonic_Front_TRIG_PORT,
	Ultrasonic_Front_TRIG_PIN,
	Ultrasonic_Front_ECHO_PORT,
	Ultrasonic_Front_ECHO_PIN,
    &htim1
};
Ultrasonic_t US_Left =
{
	Ultrasonic_Left_TRIG_PORT,
	Ultrasonic_Left_TRIG_PIN,
	Ultrasonic_Left_ECHO_PORT,
	Ultrasonic_Left_ECHO_PIN,
    &htim1
};
Ultrasonic_t US_Right =
{
	Ultrasonic_Right_TRIG_PORT,
	Ultrasonic_Right_TRIG_PIN,
	Ultrasonic_Right_ECHO_PORT,
	Ultrasonic_Right_ECHO_PIN,
    &htim1
};
float Front_distance_cm = 0.0f;
float Left_distance_cm = 0.0f;
float Right_distance_cm = 0.0f;

void Ultrasonic_Read_All_Sensor_Data(void);
float Ultrasonic_Read(GPIO_TypeDef *TRIG_PORT,
                      uint16_t TRIG_PIN,
                      GPIO_TypeDef *ECHO_PORT,
                      uint16_t ECHO_PIN,
                      TIM_HandleTypeDef *htim);

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Runtime state variables are grouped below by peripheral/feature. */

// UART / Bluetooth command buffers:
int current_option = -1;   // -1 = nothing selected yet

uint8_t uart_rx_byte;                          // single byte RX
char uart_rx_buffer[UART_RX_BUFFER_SIZE];     // command buffer
uint8_t uart_rx_index = 0;
uint8_t uart_cmd_ready = 0;

uint8_t bt_rx_byte;                          // single byte RX
char bt_rx_buffer[BT_RX_BUFFER_SIZE];     // command buffer
uint8_t bt_rx_index = 0;
uint8_t bt_cmd_ready = 0;

#define USB_RX_BUF_SIZE  64
#define USB_TX_BUF_SIZE  128

extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern volatile uint32_t USB_RxLenFS;
extern volatile uint8_t  USB_RxReadyFS;

extern USBD_HandleTypeDef hUsbDeviceFS;

static uint8_t rx_buf[64];
static uint8_t rx_ready = 0;
static uint32_t rx_counter = 0;


// Ultrasonic timing scratch variables:
uint32_t pMillis;
uint32_t val1 = 0;
uint32_t val2 = 0;
uint16_t distance  = 0;

//Error_Handler
bool hardware_fault_active = false;

uint32_t hardware_fault_start_tick = 0;
uint32_t hardware_fault_toggle_tick = 0;

bool hardware_fault_led_state = false;
typedef enum
{
    HW_FAULT_NONE = 0,
    HW_FAULT_TOF_NOT_DETECTED,
    HW_FAULT_TOF_INIT_FAILED,
    HW_FAULT_ULTRASONIC_NOT_DETECTED,
    HW_FAULT_IR_NOT_DETECTED,
    HW_FAULT_MOTOR_DRIVER,
    HW_FAULT_IMU,
    HW_FAULT_EEPROM,
    HW_FAULT_BATTERY
} HardwareFault_t;
HardwareFault_t CurrentHardwareFault = HW_FAULT_NONE;
void HardwareFault_Report(HardwareFault_t fault);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
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

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART6_UART_Init();
  MX_USB_DEVICE_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);
  HAL_UART_Receive_IT(&huart6, &bt_rx_byte, 1);


  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);



  SWI2C_Init(&bus_imu);
  SWI2C_Init(&bus_eep);

  HAL_TIM_Base_Start(&htim1);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  Print_Main_Menu();
  TIM3->ARR = 1010; //Timer Clock 40Mhz, PSC 3 , Approx 10Khz(9990)
  TIM2->CNT = 0;
  TIM4->CNT = 0;
  //forward(800,800);
  HAL_Delay(1000);

  printf("\r\nRuntime Sensor Mount Board Demo\r\n");
  printf("AUTO-DETECTION MODE: Detecting sensor board...\r\n");
  printf("Press BOOT2 to manually switch sensor board.\r\n");
  
  /* Automatic sensor board detection */
  SensorBoardType_t detected_board = AutoDetectBoard();
  printf("\r\n[AutoDetect] Detected board type: ");
  
  if (detected_board != BOARD_UNKNOWN)
  {
      for (uint8_t i = 0; i < SENSOR_BOARD_COUNT; i++)
      {
          if (SensorBoards[i].Type == detected_board)
          {
              board_index = i;
              printf("%s\r\n", SensorBoards[i].Name);
              break;
          }
      }
  }
  else
  {
      printf("UNKNOWN - Using default board (3TOF)\r\n");
      board_index = 0;  /* Default to first board */
  }
  
  SensorBoard_Apply(SensorBoards[board_index].Type);

  while (1)
  {

	   // Process_UART_Command();
	   // Process_BT_Command();
	   // Run_Selected_Option(current_option);
        BOOT2_Task();
        HardwareFault_Task();

        if (ActiveSensorBoard.ReadAll != NULL)
        {
            ActiveSensorBoard.ReadAll();
        }

        if ((ActiveSensorBoard.PrintAll != NULL) &&
            ((HAL_GetTick() - last_sensor_print_tick) >= SENSOR_PRINT_PERIOD_MS))
        {
            last_sensor_print_tick = HAL_GetTick();
            ActiveSensorBoard.PrintAll();
        }

        HAL_Delay(5);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 144;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV6;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* -------------------------------------------------------------------------- */
/* USER FUNCTION DEFINITIONS - REARRANGED BY FEATURE AREA                      */
/* -------------------------------------------------------------------------- */


/* ========================= Menu and Command Handling ========================= */

void Print_Main_Menu(void)
{
	printf("\r\n==============================\r\n");
	printf("*****www.kavwv.in*****\r\n");
	printf("==============================\r\n");
	printf("\r\n==============================\r\n");
	printf(" MicroMouse V1 Demo Program\r\n");
	printf("==============================\r\n");
	printf("0. Exit\r\n");
	printf("1. Full Demo\r\n");
	printf("2. BaseBoard -> RGB Demo + Button Test + Battery Voltage \r\n");
	printf("3. BaseBoard -> Communication Test \r\n");
	printf("4. Motor Control \r\n");
	printf("5. Encoder Check \r\n");
	printf("6. IMU Check \r\n");
	printf("7. Interface Board(TOF 3CH + 5CH + HYBRID(TOF + Ultrasonic)) \r\n");
	printf("8. Interface Board(Ultrasonic 3CH + IR 3+5 CH + HYBRID(IR + Ultrasonic)) \r\n");
	printf("9. Memory Chip \r\n");
}
void Process_BT_Command(void)
{
    if (bt_cmd_ready)
    {
        bt_cmd_ready = 0;
        Process_Command(bt_rx_buffer, "BT");
    }
}
void Process_UART_Command(void)
{
    if (uart_cmd_ready)
    {
        uart_cmd_ready = 0;
        Process_Command(uart_rx_buffer, "UART");
    }
}
void Process_Command(const char *cmd, const char *source)
	{

    int new_option = atoi(cmd);

    // ✅ Ignore invalid input
    if (new_option < 0 || new_option > 9)
        return;

    // ✅ If same command → do nothing
    if (new_option == current_option)
        return;

    // ✅ New command → update state
    current_option = new_option;

    printf("\r\n[%s] New Option Selected: %d\r\n", source, current_option);
}
void Run_Selected_Option(int option)
{
    switch (option)
    {
        case 0:
            printf("Exiting Demo Program...\r\n");
            printf("\r\n\r\n\r\n");
            Print_Main_Menu();
            break;

        case 1:
            printf("BaseBoard Full Demo Running...\r\n");
            // BaseBoard_Full_Demo();
            break;

        case 2:
            printf("RGB+Button+Battery Voltage Demo Running...\r\n");
            RGB_Demo();
            Button_Test();
            Battery_Voltage_Check();
            break;

        case 3:
            printf("Communication Test Running...\r\n");
            printf("Bluetooth & ESP share Same UART...\r\n");
            USB_Send("USB CDC Ready\r\n");
            USB_CDC_Test();
            break;

        case 4:
            printf("Motor Control Demo Running...\r\n");
            // Motor_Control_GPIO_Only();
            Motor_Control_PWM();
            break;

        case 5:
            printf("Encoder Demo Running...\r\n");
            Print_Encoder_Right();
            Print_Encoder_Left();
            break;

        case 6:
            printf("IMU Check...\r\n");
            printf("Running I2C Bus Scanner on I2C_3...\r\n");

            SWI2C_Scan(&bus_imu, "IMU_BUS (PC10/PC12)");

            MPU6050_t imu;
            MPU6050_Data_t d;

            if (MPU6050_Init(&imu, &bus_imu, MPU6050_ADDR_DEFAULT))
            {
                if (MPU6050_ReadAll(&imu, &d))
                {
                    printf("AX=%d AY=%d AZ=%d | GX=%d GY=%d GZ=%d\r\n",
                           d.Accel_X, d.Accel_Y, d.Accel_Z,
                           d.Gyro_X, d.Gyro_Y, d.Gyro_Z);
                }
            }
        case 7:
        case 8:
        case 9:
            printf("External Memeory Device Check...\r\n");
            printf("Running I2C Bus Scanner on I2C_2...\r\n");
            SWI2C_Scan(&bus_eep, "EEP_BUS (PB13/PB15)");

            uint8_t val = 0;
            EEPROM24C04_WriteByte(&bus_eep, 0x0010, 0x5A);
            EEPROM24C04_ReadByte(&bus_eep,  0x0010, &val);
            printf("EEP[0x10]=0x%02X\r\n", val);

            break;

        default:
            // do nothing
            break;
    }
}
void Handle_BT_Char(char cmd)
{
    switch (cmd)
    {
        /* ---- Motion Commands ---- */
        case 'F': forward(bt_speed, bt_speed); break;
        case 'B': reverse(bt_speed, bt_speed); break;
        case 'L': left(bt_speed, bt_speed); break;
        case 'R': right(bt_speed, bt_speed); break;

        case 'G': forward((bt_speed * TURN_NUM) / TURN_DEN, bt_speed); break;
        case 'I': forward(bt_speed, (bt_speed * TURN_NUM) / TURN_DEN); break;
        case 'H': reverse((bt_speed * TURN_NUM) / TURN_DEN, bt_speed); break;
        case 'J': reverse(bt_speed, (bt_speed * TURN_NUM) / TURN_DEN); break;

        case 'S': stop_Slow(); break;   // released button → STOP

        /* ---- Speed Control (0–9, q) ---- */
        case '0'...'9':
            bt_speed = map_uint(cmd - '0', 0, 9, 200, 1000);
            break;

        case 'q':  // full speed
            bt_speed = 1000;
            break;

        /* ---- Optional Features (future ready) ---- */
        case 'V': /* buzzer ON */ break;
        case 'v': /* buzzer OFF */ break;
        case 'W': /* front light ON */ break;
        case 'w': /* front light OFF */ break;

        default:
            break;
    }
}

/* ========================= Base Board Demos ========================= */
void RGB_Demo(void){
	  HAL_GPIO_TogglePin(GPIOC, RGB_Red_Pin);
	  HAL_Delay(300);
	  HAL_GPIO_TogglePin(GPIOC, RGB_Green_Pin);
	  HAL_Delay(300);
	  HAL_GPIO_TogglePin(GPIOB, RGB_Blue_Pin);
	  HAL_Delay(500);
}
void RGB_Red_On(void)
{
    HAL_GPIO_WritePin(GPIOC,
    		RGB_Red_Pin,
                      GPIO_PIN_SET);
}

void RGB_Off(void)
{
    HAL_GPIO_WritePin(GPIOC,
    		RGB_Red_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOC,
    		RGB_Green_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOB,
    		RGB_Blue_Pin,
                      GPIO_PIN_RESET);
}
void Button_Test(void)
{
    static GPIO_PinState Select_Switch_1;
    static GPIO_PinState Select_Switch_2;
    static GPIO_PinState Select_Switch_3;
    static GPIO_PinState Select_Switch_4;
    static GPIO_PinState BOOT_2;

    Select_Switch_1 = HAL_GPIO_ReadPin(GPIOC, Select_Switch_1_Pin);
    Select_Switch_2 = HAL_GPIO_ReadPin(GPIOC, Select_Switch_2_Pin);
    Select_Switch_3 = HAL_GPIO_ReadPin(GPIOC, Select_Switch_3_Pin);
    Select_Switch_4 = HAL_GPIO_ReadPin(GPIOC, Select_Switch_4_Pin);
    BOOT_2          = HAL_GPIO_ReadPin(GPIOB, BOOT_2_Pin);



    printf("SW1:%s | SW2:%s | SW3:%s | SW4:%s | BOOT2:%s\r\n",
           (Select_Switch_1 == GPIO_PIN_SET) ? "ON" : "OFF",
           (Select_Switch_2 == GPIO_PIN_SET) ? "ON" : "OFF",
           (Select_Switch_3 == GPIO_PIN_SET) ? "ON" : "OFF",
           (Select_Switch_4 == GPIO_PIN_SET) ? "ON" : "OFF",
           (BOOT_2          == GPIO_PIN_SET) ? "ON" : "OFF");
}
void Battery_Voltage_Check(void)
{
    uint16_t adc_raw;
    uint32_t vadc_mV, vbat_mV;
    uint8_t bat_percent;

    adc_raw = ADC_Read_Channel(ADC_CHANNEL_0);

    /* ADC pin voltage */
    vadc_mV = (adc_raw * 3300UL) / 4095;

    // Battery voltage (47k / 22k divider)
    vbat_mV = (vadc_mV * 69) / 22;

    // Battery percentage (linear approximation)
    if (vbat_mV >= BAT_FULL_MV)
        bat_percent = 100;
    else if (vbat_mV <= BAT_EMPTY_MV)
        bat_percent = 0;
    else
        bat_percent = (uint8_t)((vbat_mV - BAT_EMPTY_MV) * 100 /
                                 (BAT_FULL_MV - BAT_EMPTY_MV));

    printf("Raw_ADC      : %u\r\n", adc_raw);
    printf("ADC Voltage  : %lu mV\r\n", vadc_mV);
    printf("Battery Volt : %lu mV\r\n", vbat_mV);
    printf("Battery %%    : %u %%\r\n", bat_percent);

    if (vbat_mV < BAT_CRITICAL_MV)
        printf("Battery Status: CRITICAL ⚠ ?\r\n");
    else if (vbat_mV < BAT_LOW_MV)
        printf("Battery Status: LOW ⚠ ?\r\n");
    else
        printf("Battery Status: OK ✅\r\n");
}
/* ========================= Motor Control - GPIO Only ========================= */
void forward_gpio_only(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
}
void reverse_gpio_only(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

}
void stop_gpio_only(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
}
void Motor_Control_GPIO_Only(){
	printf("Forward......\r\n");
	forward_gpio_only();
	HAL_Delay(2000);
	stop_gpio_only();
	HAL_Delay(100);
	printf("Reverse......\r\n");
	reverse_gpio_only();
	HAL_Delay(2000);
	stop_gpio_only();
	HAL_Delay(100);
}
/* ========================= Motor Control - PWM ========================= */
void Motor_Control_PWM(){
	printf("Forward......\r\n");
	forward(800,800);
	HAL_Delay(2000);
	stop_Slow();
	HAL_Delay(100);
	printf("Reverse......\r\n");
	reverse(800,800);
	HAL_Delay(2000);
	stop_Slow();
	HAL_Delay(100);
	printf("Left......\r\n");
	left(800,800);
	HAL_Delay(2000);
	stop_Slow();
	HAL_Delay(100);
	printf("Right......\r\n");
	right(800,800);
	HAL_Delay(2000);
	stop_Slow();
	HAL_Delay(100);
}
void forward(uint16_t duty_L, uint16_t duty_R) {
	TIM3->CCR1 = duty_R;    //Right Motor
	TIM3->CCR2 = 0;			//Right Motor
	TIM3->CCR3 = duty_L;	//Left Motor
	TIM3->CCR4 = 0;			//Left Motor
}
void reverse(uint16_t duty_L, uint16_t duty_R) {
	TIM3->CCR1 = 0;
	TIM3->CCR2 = duty_R;
	TIM3->CCR3 = 0;
	TIM3->CCR4 = duty_L;
}
void left(uint16_t duty_L, uint16_t duty_R) {
    TIM3->CCR1 = duty_R;
    TIM3->CCR2 = 0;
    TIM3->CCR3 = 0;
    TIM3->CCR4 = duty_L;
}
void right(uint16_t duty_L, uint16_t duty_R) {
    TIM3->CCR1 = 0;
    TIM3->CCR2 = duty_R;
    TIM3->CCR3 = duty_L;
    TIM3->CCR4 = 0;
}
void stop_Slow() {
	TIM3->CCR1 = 0;
	TIM3->CCR2 = 0;
	TIM3->CCR3 = 0;
	TIM3->CCR4 = 0;
}
void stop_Fast() {
	TIM3->CCR1 = 800;
	TIM3->CCR2 = 800;
	TIM3->CCR3 = 800;
	TIM3->CCR4 = 800;
}
//Sensor_Board

/* ========================= Runtime Mount-Board GPIO/Selection ========================= */
static void Runtime_GPIO_DeInit_All_Sensors(void)
{
    TOF_Enable = false;
    active_tof_sensor_count = 0;
    active_ultrasonic_sensor_count = 0;
    memset(IR_Value, 0, sizeof(IR_Value));

    TOF_Front.Online = false;
    TOF_Left.Online = false;
    TOF_Right.Online = false;
    TOF_LeftDiag.Online = false;
    TOF_RightDiag.Online = false;

    TOF_Disable_All();

    /* Release shared sensor pins A0-A4 / PA1-PA5 before selecting a new board. */
    HAL_GPIO_DeInit(GPIOA,
                    GPIO_PIN_1 |
                    GPIO_PIN_2 |
                    GPIO_PIN_3 |
                    GPIO_PIN_4 |
                    GPIO_PIN_5);

    HAL_GPIO_DeInit(Ultrasonic_Front_TRIG_PORT, Ultrasonic_Front_TRIG_PIN);
    HAL_GPIO_DeInit(Ultrasonic_Left_TRIG_PORT,  Ultrasonic_Left_TRIG_PIN);
    HAL_GPIO_DeInit(Ultrasonic_Right_TRIG_PORT, Ultrasonic_Right_TRIG_PIN);
    HAL_GPIO_DeInit(Ultrasonic_Front_ECHO_PORT, Ultrasonic_Front_ECHO_PIN);
    HAL_GPIO_DeInit(Ultrasonic_Left_ECHO_PORT,  Ultrasonic_Left_ECHO_PIN);
    HAL_GPIO_DeInit(Ultrasonic_Right_ECHO_PORT, Ultrasonic_Right_ECHO_PIN);
}

static void Runtime_GPIO_Config_TOF(uint8_t count)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    if (count >= 1)
    {
        GPIO_InitStruct.Pin = TOF_enableFront_Pin;
        HAL_GPIO_Init(TOF_enableFront_GPIO_Port, &GPIO_InitStruct);
    }

    if (count >= 2)
    {
        GPIO_InitStruct.Pin = TOF_enableLeft_Pin;
        HAL_GPIO_Init(TOF_enableLeft_GPIO_Port, &GPIO_InitStruct);
    }

    if (count >= 3)
    {
        GPIO_InitStruct.Pin = TOF_enableRight_Pin;
        HAL_GPIO_Init(TOF_enableRight_GPIO_Port, &GPIO_InitStruct);
    }

    if (count >= 4)
    {
        GPIO_InitStruct.Pin = TOF_enableDiagLeft_Pin;
        HAL_GPIO_Init(TOF_enableDiagLeft_GPIO_Port, &GPIO_InitStruct);
    }

    if (count >= 5)
    {
        GPIO_InitStruct.Pin = TOF_enableDiagRight_Pin;
        HAL_GPIO_Init(TOF_enableDiagRight_GPIO_Port, &GPIO_InitStruct);
    }

    TOF_Disable_All();
    HAL_Delay(10);
}

static void Runtime_GPIO_Config_Ultrasonic(uint8_t count)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    if (count >= 1)
    {
        GPIO_InitStruct.Pin = Ultrasonic_Front_TRIG_PIN;
        HAL_GPIO_Init(Ultrasonic_Front_TRIG_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(Ultrasonic_Front_TRIG_PORT, Ultrasonic_Front_TRIG_PIN, GPIO_PIN_RESET);
    }

    if (count >= 2)
    {
        GPIO_InitStruct.Pin = Ultrasonic_Left_TRIG_PIN;
        HAL_GPIO_Init(Ultrasonic_Left_TRIG_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(Ultrasonic_Left_TRIG_PORT, Ultrasonic_Left_TRIG_PIN, GPIO_PIN_RESET);
    }

    if (count >= 3)
    {
        GPIO_InitStruct.Pin = Ultrasonic_Right_TRIG_PIN;
        HAL_GPIO_Init(Ultrasonic_Right_TRIG_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(Ultrasonic_Right_TRIG_PORT, Ultrasonic_Right_TRIG_PIN, GPIO_PIN_RESET);
    }

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    if (count >= 1)
    {
        GPIO_InitStruct.Pin = Ultrasonic_Front_ECHO_PIN;
        HAL_GPIO_Init(Ultrasonic_Front_ECHO_PORT, &GPIO_InitStruct);
    }

    if (count >= 2)
    {
        GPIO_InitStruct.Pin = Ultrasonic_Left_ECHO_PIN;
        HAL_GPIO_Init(Ultrasonic_Left_ECHO_PORT, &GPIO_InitStruct);
    }

    if (count >= 3)
    {
        GPIO_InitStruct.Pin = Ultrasonic_Right_ECHO_PIN;
        HAL_GPIO_Init(Ultrasonic_Right_ECHO_PORT, &GPIO_InitStruct);
    }

    active_ultrasonic_sensor_count = count;
    HAL_TIM_Base_Start(&htim1);
}


static void Runtime_GPIO_Config_Ultrasonic_LeftRight(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = Ultrasonic_Left_TRIG_PIN;
    HAL_GPIO_Init(Ultrasonic_Left_TRIG_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(Ultrasonic_Left_TRIG_PORT, Ultrasonic_Left_TRIG_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = Ultrasonic_Right_TRIG_PIN;
    HAL_GPIO_Init(Ultrasonic_Right_TRIG_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(Ultrasonic_Right_TRIG_PORT, Ultrasonic_Right_TRIG_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    GPIO_InitStruct.Pin = Ultrasonic_Left_ECHO_PIN;
    HAL_GPIO_Init(Ultrasonic_Left_ECHO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = Ultrasonic_Right_ECHO_PIN;
    HAL_GPIO_Init(Ultrasonic_Right_ECHO_PORT, &GPIO_InitStruct);

    active_ultrasonic_sensor_count = 2;
    HAL_TIM_Base_Start(&htim1);
}

static void Runtime_GPIO_Config_IR(uint8_t count)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (count > 5U)
    {
        count = 5U;
    }

    /* A0-A4 are PA1-PA5. These pins are shared with TOF XSHUT and ultrasonic.
     * For IR mode, force the selected pins into analog mode.
     */
    HAL_GPIO_DeInit(GPIOA,
                    GPIO_PIN_1 |
                    GPIO_PIN_2 |
                    GPIO_PIN_3 |
                    GPIO_PIN_4 |
                    GPIO_PIN_5);

    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    if (count >= 1U)
    {
        GPIO_InitStruct.Pin = A0_Pin;   /* PA1 -> ADC_CHANNEL_1 */
        HAL_GPIO_Init(A0_GPIO_Port, &GPIO_InitStruct);
    }

    if (count >= 2U)
    {
        GPIO_InitStruct.Pin = A1_Pin;   /* PA2 -> ADC_CHANNEL_2 */
        HAL_GPIO_Init(A1_GPIO_Port, &GPIO_InitStruct);
    }

    if (count >= 3U)
    {
        GPIO_InitStruct.Pin = A2_Pin;   /* PA3 -> ADC_CHANNEL_3 */
        HAL_GPIO_Init(A2_GPIO_Port, &GPIO_InitStruct);
    }

    if (count >= 4U)
    {
        GPIO_InitStruct.Pin = A3_Pin;   /* PA4 -> ADC_CHANNEL_4 */
        HAL_GPIO_Init(A3_GPIO_Port, &GPIO_InitStruct);
    }

    if (count >= 5U)
    {
        GPIO_InitStruct.Pin = A4_Pin;   /* PA5 -> ADC_CHANNEL_5 */
        HAL_GPIO_Init(A4_GPIO_Port, &GPIO_InitStruct);
    }

    printf("IR analog GPIO configured: %d channel(s)\r\n", count);
}

static void SensorBoard_Apply(SensorBoardType_t board)
{
    for (uint8_t i = 0; i < SENSOR_BOARD_COUNT; i++)
    {
        if (SensorBoards[i].Type == board)
        {
            if (ActiveSensorBoard.DeInit != NULL)
            {
                ActiveSensorBoard.DeInit();
            }

            ActiveSensorBoard = SensorBoards[i];
            ActiveBoard = board;

            printf("\r\n================================\r\n");
            printf("Selected sensor board: %s\r\n", ActiveSensorBoard.Name);
            printf("TOF:%d  US:%d  IR:%d\r\n",
                   ActiveSensorBoard.TOF_Count,
                   ActiveSensorBoard.US_Count,
                   ActiveSensorBoard.IR_Count);
            printf("Initializing...\r\n");

            if (ActiveSensorBoard.Init != NULL)
            {
                ActiveSensorBoard.Init();
            }

            printf("Initialization done.\r\n");
            printf("================================\r\n");
            return;
        }
    }

    printf("Unknown sensor board selected.\r\n");
}

static void SensorBoard_SelectNext(void)
{
    board_index++;
    if (board_index >= SENSOR_BOARD_COUNT)
    {
        board_index = 0;
    }

    SensorBoard_Apply(SensorBoards[board_index].Type);
}

static void BOOT2_Task(void)
{
    GPIO_PinState now = HAL_GPIO_ReadPin(GPIOB, BOOT_2_Pin);

    if ((boot2_prev_state == GPIO_PIN_SET) &&
        (now == GPIO_PIN_RESET) &&
        ((HAL_GetTick() - last_boot2_press_tick) > BOOT2_DEBOUNCE_MS))
    {
        last_boot2_press_tick = HAL_GetTick();

        printf("\r\n[BOOT2] Re-running auto-detection...\r\n");

        SensorBoardType_t detected = AutoDetectBoard();

        printf("\r\n[AutoDetect] Detected board type: ");
        if (detected != BOARD_UNKNOWN)
        {
            for (uint8_t i = 0; i < SENSOR_BOARD_COUNT; i++)
            {
                if (SensorBoards[i].Type == detected)
                {
                    board_index = i;
                    printf("%s\r\n", SensorBoards[i].Name);
                    break;
                }
            }
        }
        else
        {
            printf("UNKNOWN - Keeping current board\r\n");
            return;  /* Don't reinitialize if detection failed */
        }

        SensorBoard_Apply(SensorBoards[board_index].Type);
    }

    boot2_prev_state = now;
}

static void Sensor_Board_DeInit_All(void)
{
    Runtime_GPIO_DeInit_All_Sensors();
    TOF_Disable_All();
    HAL_Delay(100);
}
uint16_t ADC_Read_Channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);

    HAL_ADC_PollForConversion(&hadc1, 10);

    uint16_t value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}

void IR_Read_All(void)
{
    IR_Value[0] = ADC_Read_Channel(ADC_CHANNEL_1);  // PA1
    IR_Value[1] = ADC_Read_Channel(ADC_CHANNEL_2);  // PA2
    IR_Value[2] = ADC_Read_Channel(ADC_CHANNEL_3);  // PA3
    IR_Value[3] = ADC_Read_Channel(ADC_CHANNEL_4);  // PA4
    IR_Value[4] = ADC_Read_Channel(ADC_CHANNEL_5);  // PA5

    //IR_Value[3] = IR_Value[1];
    //IR_Value[4] = IR_Value[2];
}
/* ========================= TOF Sensor Functions ========================= */
static void TOF_Disable_All(void)
{
    HAL_GPIO_WritePin(TOF_enableFront_GPIO_Port,
                      TOF_enableFront_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(TOF_enableLeft_GPIO_Port,
                      TOF_enableLeft_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(TOF_enableRight_GPIO_Port,
                      TOF_enableRight_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(TOF_enableDiagLeft_GPIO_Port,
                      TOF_enableDiagLeft_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(TOF_enableDiagRight_GPIO_Port,
                      TOF_enableDiagRight_Pin,
                      GPIO_PIN_RESET);
}
static void TOF_Enable_And_Init(TOF_SensorConfig_t *sensor)
{
    if (sensor == NULL)
    {
        return;
    }

    HAL_GPIO_WritePin(sensor->GPIO_Port, sensor->GPIO_Pin, GPIO_PIN_SET);
    HAL_Delay(30);

    if (TOFInit(sensor->Dev))
    {
        sensor->Online = true;
        printf("[OK] TOF %s initialized\r\n", sensor->Name);
    }
    else
    {
        sensor->Online = false;
        printf("[FAULT] TOF %s init failed\r\n", sensor->Name);
        HardwareFault_Report(HW_FAULT_TOF_INIT_FAILED);
    }
}

static uint8_t TOF_Setup_List(TOF_SensorConfig_t **sensor_list,
                              uint8_t sensor_count,
                              uint8_t gpio_config_count,
                              const char *layout_name)
{
    uint8_t online_count = 0;

    TOF_Front.Online = false;
    TOF_Left.Online = false;
    TOF_Right.Online = false;
    TOF_LeftDiag.Online = false;
    TOF_RightDiag.Online = false;

    Runtime_GPIO_Config_TOF(gpio_config_count);
    TOF_Disable_All();
    HAL_Delay(100);

    printf("All TOF sensors reset for %s\r\n", layout_name);

    for (uint8_t i = 0; i < sensor_count; i++)
    {
        TOF_SensorConfig_t *sensor = sensor_list[i];

        printf("Initializing TOF %s...\r\n", sensor->Name);

        Dev0.I2cDevAddr = 0x52;

        HAL_GPIO_WritePin(sensor->GPIO_Port, sensor->GPIO_Pin, GPIO_PIN_SET);
        HAL_Delay(30);

        if (HAL_I2C_IsDeviceReady(&hi2c1, 0x52, 2, 50) != HAL_OK)
        {
            printf("[FAULT] TOF %s not detected at default address 0x52\r\n", sensor->Name);
            sensor->Online = false;
            HardwareFault_Report(HW_FAULT_TOF_NOT_DETECTED);
            continue;
        }

        if (!TOFInit(&Dev0))
        {
            printf("[FAULT] TOF %s initialization failed\r\n", sensor->Name);
            sensor->Online = false;
            HardwareFault_Report(HW_FAULT_TOF_INIT_FAILED);
            continue;
        }

        if (!TOF_Address_Change(&Dev0, sensor->Dev))
        {
            printf("[FAULT] TOF %s address change failed\r\n", sensor->Name);
            sensor->Online = false;
            HardwareFault_Report(HW_FAULT_TOF_INIT_FAILED);
            continue;
        }

        if (!TOFInit(sensor->Dev))
        {
            printf("[FAULT] TOF %s final init failed\r\n", sensor->Name);
            sensor->Online = false;
            HardwareFault_Report(HW_FAULT_TOF_INIT_FAILED);
            continue;
        }

        sensor->Online = true;
        online_count++;

        printf("[OK] TOF %s ready at 0x%02X\r\n", sensor->Name, sensor->Dev->I2cDevAddr);
    }

    TOF_Enable = (online_count > 0U);

    if (online_count == 0U)
    {
        printf("\r\n");
        printf("NO TOF SENSOR DETECTED\r\n");
        printf("CHECK POWER / SDA / SCL / XSHUT\r\n");
        printf("\r\n");
        HardwareFault_Report(HW_FAULT_TOF_NOT_DETECTED);
    }
    else
    {
        printf("TOF online count: %d/%d (%s)\r\n", online_count, sensor_count, layout_name);
    }

    return online_count;
}

void TOF_Setup_Front(void)
{
    TOF_SensorConfig_t *sensors[1] = { &TOF_Front };

    active_tof_sensor_count = 1;
    (void)TOF_Setup_List(sensors, 1, 1, "Front only");
}

void TOF_Setup_LeftRight(void)
{
    TOF_SensorConfig_t *sensors[2] = { &TOF_Left, &TOF_Right };

    /* count is 3 so ReadAll can refresh Right distance too. Front remains offline. */
    active_tof_sensor_count = 3;
    (void)TOF_Setup_List(sensors, 2, 3, "Left/Right only");
}

void TOF_Setup_Multi(uint8_t sensor_count)
{
    TOF_SensorConfig_t *active_sensors[5];
    uint8_t active_count = 0;
    uint8_t online_count = 0;

    if (sensor_count > 5)
    {
        sensor_count = 5;
    }

    TOF_Front.Online = false;
    TOF_Left.Online = false;
    TOF_Right.Online = false;
    TOF_LeftDiag.Online = false;
    TOF_RightDiag.Online = false;

    switch (sensor_count)
    {
        case 1:
            active_sensors[0] = &TOF_Front;
            active_count = 1;
            break;

        case 2:
            active_sensors[0] = &TOF_Front;
            active_sensors[1] = &TOF_Left;
            active_count = 2;
            break;

        case 3:
            active_sensors[0] = &TOF_Front;
            active_sensors[1] = &TOF_Left;
            active_sensors[2] = &TOF_Right;
            active_count = 3;
            break;

        case 5:
            active_sensors[0] = &TOF_Front;
            active_sensors[1] = &TOF_Left;
            active_sensors[2] = &TOF_Right;
            active_sensors[3] = &TOF_LeftDiag;
            active_sensors[4] = &TOF_RightDiag;
            active_count = 5;
            break;

        default:
            printf("Invalid TOF sensor count: %d\r\n", sensor_count);
            TOF_Enable = false;
            return;
    }

    Runtime_GPIO_Config_TOF(active_count);
    TOF_Disable_All();
    HAL_Delay(100);
    printf("All TOF sensors reset\r\n");

    for (uint8_t i = 0; i < active_count; i++)
    {
        TOF_SensorConfig_t *sensor = active_sensors[i];

        printf("Initializing TOF %s...\r\n", sensor->Name);

        Dev0.I2cDevAddr = 0x52;

        HAL_GPIO_WritePin(sensor->GPIO_Port, sensor->GPIO_Pin, GPIO_PIN_SET);
        HAL_Delay(30);

        if (HAL_I2C_IsDeviceReady(&hi2c1, 0x52, 2, 50) != HAL_OK)
        {
            printf("[FAULT] TOF %s not detected at default address 0x52\r\n", sensor->Name);
            sensor->Online = false;
            HardwareFault_Report(HW_FAULT_TOF_NOT_DETECTED);
            continue;
        }

        if (!TOFInit(&Dev0))
        {
            printf("[FAULT] TOF %s initialization failed\r\n", sensor->Name);
            sensor->Online = false;
            HardwareFault_Report(HW_FAULT_TOF_INIT_FAILED);
            continue;
        }

        if (!TOF_Address_Change(&Dev0, sensor->Dev))
        {
            printf("[FAULT] TOF %s address change failed\r\n", sensor->Name);
            sensor->Online = false;
            HardwareFault_Report(HW_FAULT_TOF_INIT_FAILED);
            continue;
        }

        if (!TOFInit(sensor->Dev))
        {
            printf("[FAULT] TOF %s final init failed\r\n", sensor->Name);
            sensor->Online = false;
            HardwareFault_Report(HW_FAULT_TOF_INIT_FAILED);
            continue;
        }

        sensor->Online = true;
        online_count++;

        printf("[OK] TOF %s ready at 0x%02X\r\n", sensor->Name, sensor->Dev->I2cDevAddr);
    }

    active_tof_sensor_count = active_count;
    TOF_Enable = (online_count > 0U);

    if (online_count == 0U)
    {
        printf("\r\n");
        printf("NO TOF SENSOR DETECTED\r\n");
        printf("CHECK POWER / SDA / SCL / XSHUT\r\n");
        printf("\r\n");
        HardwareFault_Report(HW_FAULT_TOF_NOT_DETECTED);
    }
    else
    {
        printf("TOF online count: %d/%d\r\n", online_count, active_count);
    }
}
static bool TOF_Address_Change(VL53L0X_DEV Dev, VL53L0X_DEV Dev_)
{
    VL53L0X_Error Status;

    Status = VL53L0X_SetDeviceAddress(Dev, Dev_->I2cDevAddr);
    if (Status != VL53L0X_ERROR_NONE)
    {
        printf("VL53L0X address change failed: %d\r\n", Status);
        return false;
    }

    HAL_Delay(5);
    Dev->I2cDevAddr = Dev_->I2cDevAddr;
    return true;
}
static bool TOFInit(VL53L0X_DEV Dev)
{
    VL53L0X_Error Status;
    uint8_t vhv_settings = 0;
    uint8_t phase_cal = 0;
    uint32_t ref_spad_count = 0;
    uint8_t is_aperture_spads = 0;

    Status = VL53L0X_DataInit(Dev);
    if (Status != VL53L0X_ERROR_NONE)
    {
        printf("VL53L0X_DataInit failed: %d\r\n", Status);
        return false;
    }

    Status = VL53L0X_StaticInit(Dev);
    if (Status != VL53L0X_ERROR_NONE)
    {
        printf("VL53L0X_StaticInit failed: %d\r\n", Status);
        return false;
    }

    Status = VL53L0X_PerformRefCalibration(Dev, &vhv_settings, &phase_cal);
    if (Status != VL53L0X_ERROR_NONE)
    {
        printf("VL53L0X RefCalibration failed: %d\r\n", Status);
        return false;
    }

    Status = VL53L0X_PerformRefSpadManagement(Dev, &ref_spad_count, &is_aperture_spads);
    if (Status != VL53L0X_ERROR_NONE)
    {
        printf("VL53L0X RefSpad failed: %d\r\n", Status);
        return false;
    }

    Status = VL53L0X_SetDeviceMode(Dev, VL53L0X_DEVICEMODE_SINGLE_RANGING);
    if (Status != VL53L0X_ERROR_NONE)
    {
        printf("VL53L0X SetDeviceMode failed: %d\r\n", Status);
        return false;
    }

    return true;
}
static void StartTOF(VL53L0X_DEV Dev)
{
    VL53L0X_Error Status = VL53L0X_StartMeasurement(Dev);
    if (Status != VL53L0X_ERROR_NONE)
    {
        printf("VL53L0X_StartMeasurement failed: %d\r\n", Status);
        HardwareFault_Report(HW_FAULT_TOF_INIT_FAILED);
    }
}
uint16_t GetDistance(VL53L0X_DEV Dev)
{
    VL53L0X_RangingMeasurementData_t rangeData;
    VL53L0X_Error Status;

    Status = VL53L0X_PerformSingleRangingMeasurement(Dev, &rangeData);
    if (Status != VL53L0X_ERROR_NONE)
    {
        return 0xFFFF;
    }

    return rangeData.RangeMilliMeter;
}
void TOF_Read_All_Sesnor_data(void)
{
    if (TOF_Enable != true)
        return;

    Front_distance = TOF_Front.Online ? GetDistance(TOF_Front.Dev) : 0xFFFF;
    Left_distance = TOF_Left.Online ? GetDistance(TOF_Left.Dev) : 0xFFFF;
    Right_distance = TOF_Right.Online ? GetDistance(TOF_Right.Dev) : 0xFFFF;
    Left_diag_distance = TOF_LeftDiag.Online ? GetDistance(TOF_LeftDiag.Dev) : 0xFFFF;
    Right_diag_distance = TOF_RightDiag.Online ? GetDistance(TOF_RightDiag.Dev) : 0xFFFF;
}
/* ========================= Ultrasonic Sensor Functions ========================= */
//Setup timmer 1 as time base
//count = 1 us
//Prescaler = 39 when clock is 40Mhz
float Ultrasonic_Read(GPIO_TypeDef *TRIG_PORT,
                      uint16_t TRIG_PIN,
                      GPIO_TypeDef *ECHO_PORT,
                      uint16_t ECHO_PIN,
                      TIM_HandleTypeDef *htim)
{
    uint32_t pMillis;
    uint32_t val1 = 0;
    uint32_t val2 = 0;
    float distance_cm = 0.0f;

    /*
     * Generate 10 us trigger pulse
     */
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);

    __HAL_TIM_SET_COUNTER(htim, 0);

    while (__HAL_TIM_GET_COUNTER(htim) < 10);

    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

    /*
     * Wait for echo pin to go HIGH
     * Timeout: 10 ms
     */
    pMillis = HAL_GetTick();

    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_RESET)
    {
        if ((HAL_GetTick() - pMillis) > 10)
        {
            return -1.0f;   // Echo not received
        }
    }

    val1 = __HAL_TIM_GET_COUNTER(htim);

    /*
     * Wait for echo pin to go LOW
     * Timeout: 50 ms
     */
    pMillis = HAL_GetTick();

    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET)
    {
        if ((HAL_GetTick() - pMillis) > 50)
        {
            return -1.0f;   // Echo timeout / object too far
        }
    }

    val2 = __HAL_TIM_GET_COUNTER(htim);

    /*
     * Distance calculation
     * Speed of sound = 0.034 cm/us
     * Divide by 2 because pulse travels forward and back
     */
    distance_cm = (val2 - val1) * 0.034f / 2.0f;

    return distance_cm;
}
void Ultrasonic_Read_All_Sensor_data(void)
{
    if (active_ultrasonic_sensor_count >= 1)
    {
        Front_distance_cm = Ultrasonic_Read(US_Front.TRIG_PORT, US_Front.TRIG_PIN,
                                            US_Front.ECHO_PORT, US_Front.ECHO_PIN,
                                            US_Front.htim);
    }
    if (active_ultrasonic_sensor_count >= 2)
    {
        Left_distance_cm = Ultrasonic_Read(US_Left.TRIG_PORT, US_Left.TRIG_PIN,
                                           US_Left.ECHO_PORT, US_Left.ECHO_PIN,
                                           US_Left.htim);
    }
    if (active_ultrasonic_sensor_count >= 3)
    {
        Right_distance_cm = Ultrasonic_Read(US_Right.TRIG_PORT, US_Right.TRIG_PIN,
                                            US_Right.ECHO_PORT, US_Right.ECHO_PIN,
                                            US_Right.htim);
    }
}
/* ========================= Encoder Utilities ========================= */
void Print_Encoder_Left(void)
{
    static int32_t prev_count = 0;
    static uint8_t prev_dir = 0xFF;   // invalid initial value

    int32_t count = (int32_t)TIM4->CNT;
    uint8_t dir = (TIM4->CR1 & TIM_CR1_DIR) ? 1 : 0;

    // Print only if count OR direction changed
    if ((count != prev_count) || (dir != prev_dir))
    {
        if (dir)
        {
            // DIR = 1 → counting down
            printf("LEFT  | Count: %ld | Direction: CCW\r\n", count);  // Micromouse
        }
        else
        {
            // DIR = 0 → counting up
            printf("LEFT  | Count: %ld | Direction: CW\r\n", count);   // Micromouse
        }

        prev_count = count;
        prev_dir   = dir;
    }
}
void Print_Encoder_Right(void)
{
    static int32_t prev_count = 0;
    static uint8_t prev_dir = 0xFF;   // invalid initial value

    int32_t count = (int32_t)TIM2->CNT;
    uint8_t dir = (TIM2->CR1 & TIM_CR1_DIR) ? 1 : 0;

    // Print only if count OR direction changed
    if ((count != prev_count) || (dir != prev_dir))
    {
        if (dir)
        {
            // DIR = 1 → counting down
            printf("RIGHT | Count: %ld | Direction: CW\r\n", count);   // Micromouse
        }
        else
        {
            // DIR = 0 → counting up
            printf("RIGHT | Count: %ld | Direction: CCW\r\n", count);  // Micromouse
        }

        prev_count = count;
        prev_dir   = dir;
    }
}
/*void Print_Encoder_Left(void)
{
    int32_t count = (int32_t)TIM4->CNT;

    if (TIM4->CR1 & TIM_CR1_DIR)
    {
        // DIR = 1 → counting down
        printf("LEFT  | Count: %ld | Direction: CCW\r\n", count);   // Micromouse
    }
    else
    {
        // DIR = 0 → counting up
        printf("LEFT  | Count: %ld | Direction: CW\r\n", count);  // Micromouse
    }
}
void Print_Encoder_Right(void)
{
    int32_t count = (int32_t)TIM2->CNT;

    if (TIM2->CR1 & TIM_CR1_DIR)
    {
        // DIR = 1 → counting down
        printf("RIGHT | Count: %ld | Direction: CW\r\n", count);   // Micromouse
    }
    else
    {
        // DIR = 0 → counting up
        printf("RIGHT | Count: %ld | Direction: CCW\r\n", count);  // Micromouse
    }
}
*/

/* ========================= Mount Board ========================= */

SensorBoardType_t AutoDetectBoard(void)
{
    printf("\r\n[AutoDetect] Start\r\n");

    /* Priority 1: TOF presence by active XSHUT + I2C probe */
    uint8_t tof_count = Detect_TOF_Count();
    printf("[AutoDetect] TOF count: %u\r\n", tof_count);

    /* CRITICAL: Leave all sensors DISABLED after detection so board init can initialize from clean state */
    printf("[AutoDetect] Disabling all TOF sensors (board init will enable and initialize properly)...\r\n");
    TOF_Disable_All();  /* Pull all XSHUT LOW - this is critical */
    HAL_Delay(100);     /* Wait for I2C bus to settle */
    printf("[AutoDetect] TOF detection phase complete - sensors disabled\r\n");

    if(tof_count == 5)
        return BOARD_1_5TOF;

    if(tof_count == 3)
        return BOARD_1_3TOF;

    if(tof_count == 2)
    {
        /* -----------------------------------------------------------------------
         * TEMPORARY PATCH (Board 1 - Module 1):
         *
         * When exactly 2 TOF sensors are detected (Left + Right), this is
         * ALWAYS Board 1 with a Front Ultrasonic sensor. No other board
         * combination uses exactly 2 TOF sensors, so the front slot is
         * definitively ultrasonic.
         *
         * ADC-based ultrasonic detection was skipped here because Module 1
         * has no pull-up resistor on the Front US echo pin (A0), causing
         * it to read ~0 mV even when the sensor is connected.
         *
         * TODO: Add pull-up on A0 in hardware to re-enable ADC detection.
         * ----------------------------------------------------------------------- */
        printf("[AutoDetect] 2 TOF (Left+Right) detected.\r\n");
        printf("[AutoDetect] PATCH: Front slot = Ultrasonic (forced, no ADC check).\r\n");
        printf("[AutoDetect] -> BOARD_1_2TOF_1US\r\n");
        return BOARD_1_2TOF_1US;
    }

    if(tof_count == 1)
    {
        /* Optional disambiguation for front TOF + 5IR variant */
        Runtime_GPIO_Config_IR(5);
        IR_Read_All();

        printf("[AutoDetect] IR values: %u %u %u %u %u\r\n",
               IR_Value[0], IR_Value[1], IR_Value[2], IR_Value[3], IR_Value[4]);

        if(Detect_5IR())
            return BOARD_2_1TOF_5IR;

        return BOARD_2_1TOF_2US;
    }

    /* Priority 2: IR boards */
    Runtime_GPIO_Config_IR(5);
    IR_Read_All();

    printf("[AutoDetect] IR values: %u %u %u %u %u\r\n",
           IR_Value[0], IR_Value[1], IR_Value[2], IR_Value[3], IR_Value[4]);

    if(Detect_5IR())
        return BOARD_2_5IR;

    if(Detect_3IR())
        return BOARD_2_3IR;

    /* Priority 3: Ultrasonic by stable analog level range */
    uint8_t us_count = Detect_US_Count();
    printf("[AutoDetect] US count: %u\r\n", us_count);

    if(us_count == 3)
        return BOARD_2_3US;

    /* -----------------------------------------------------------------------
     * TEMPORARY PATCH — Board 2, No Pull-up on Ultrasonic Echo Pins
     * -----------------------------------------------------------------------
     * WHY THIS EXISTS:
     *   Board 2 (Module 2) PCB shares PA1/PA2/PA3 (A0/A1/A2) between the
     *   ultrasonic echo lines AND the IR analog inputs. There are no external
     *   pull-up resistors on the echo pins.
     *
     *   Consequence: when an ultrasonic sensor is connected but no IR sensor
     *   is on the same channel, the echo pin floats near 0 V (~0–300 mV).
     *   The ADC-based detection window (700 mV – 2500 mV) therefore NEVER
     *   fires for ultrasonic, and the channel looks identical to an empty pin.
     *
     * WHAT WE DO INSTEAD (elimination logic):
     *   After the 5IR and 3IR checks fail, we count how many of ch0/ch1/ch2
     *   read as IR (> 4000 counts, ~3.3 V pulled high by active IR sensor).
     *   Any channel that is NOT IR is assumed to carry an ultrasonic sensor.
     *   This works only because on this hardware, a slot is always populated
     *   with exactly one of: IR sensor or Ultrasonic sensor.
     *
     *   Note: extended IR channels (ch3/ch4 = A3/A4) are intentionally
     *   IGNORED in this patch. The 5IR hybrid board type (BOARD_2_3US_5IR)
     *   is excluded because simultaneous 3US + 5IR is a test-only scenario
     *   not present in the final robot configuration.
     *
     * HOW TO REMOVE THIS PATCH:
     *   1. Add 10k pull-up resistors on all three US echo pins (PA1/PA2/PA3).
     *   2. Verify the echo idle voltage lands in 700 mV – 2500 mV range.
     *   3. Delete this block; Detect_US_Count() above will handle it cleanly.
     * ----------------------------------------------------------------------- */
    uint8_t ir_ch = 0;
    if (IR_Value[0] > 4000) ir_ch++;
    if (IR_Value[1] > 4000) ir_ch++;
    if (IR_Value[2] > 4000) ir_ch++;
    uint8_t us_ch = 3U - ir_ch;

    printf("[AutoDetect] PATCH (Board2): ch0-2  IR=%u  US(assumed)=%u\r\n",
           ir_ch, us_ch);

    if (us_ch == 3) { printf("[AutoDetect] -> BOARD_2_3US\r\n");      return BOARD_2_3US;     }
    if (us_ch == 1) { printf("[AutoDetect] -> BOARD_2_1US_3IR\r\n"); return BOARD_2_1US_3IR; }

    printf("[AutoDetect] PATCH: no match for IR=%u US=%u -> UNKNOWN\r\n",
           ir_ch, us_ch);
    return BOARD_UNKNOWN;
}
uint8_t Detect_TOF_Count(void)
{
    uint8_t count = 0;

    printf("[DiagTOF] Starting TOF detection..\r\n");
    Runtime_GPIO_Config_TOF(5);
    printf("[DiagTOF] GPIO configured for 5 TOF XSHUT pins\r\n");

    TOF_Disable_All();
    printf("[DiagTOF] All XSHUT pulled LOW (disabled)\r\n");

    HAL_Delay(100);
    printf("[DiagTOF] Waited 100ms for I2C bus settle\r\n");

    /* ------------------------------------------------------------------
     * I2C bus check BEFORE probing: only check address 0x29 (VL53L0X default)
     * Avoid full bus scan - scanning 112 addresses with 5ms timeout each
     * (~560ms of failed transactions) corrupts the STM32 I2C state machine.
     * ------------------------------------------------------------------ */
    printf("[DiagTOF] I2C bus check (all XSHUT=LOW, expect empty)...\r\n");
    {
        HAL_StatusTypeDef r = HAL_I2C_IsDeviceReady(&hi2c1, 0x52, 1, 10);
        if (r == HAL_OK)
            printf("[DiagTOF]   Bus check: device found at 0x52 -> UNEXPECTED (sensor on without XSHUT?)\r\n");
        else
            printf("[DiagTOF]   Bus check: 0x52 silent (expected baseline)\r\n");
    }
    /* Reset I2C peripheral to clear any error state from above */
    HAL_I2C_DeInit(&hi2c1);
    MX_I2C1_Init();

    /* Enable FRONT only and verify sensor appears at 0x52 */
    printf("[DiagTOF] Enabling FRONT XSHUT only - checking 0x52...\r\n");
    HAL_GPIO_WritePin(TOF_Front.GPIO_Port, TOF_Front.GPIO_Pin, GPIO_PIN_SET);
    HAL_Delay(40);
    {
        HAL_StatusTypeDef r = HAL_I2C_IsDeviceReady(&hi2c1, 0x52, 2, 20);
        if (r == HAL_OK)
            printf("[DiagTOF]   Front-check: sensor at 0x52 -> PRESENT\r\n");
        else
            printf("[DiagTOF]   Front-check: 0x52 NO RESPONSE -> bad pull-ups or wiring!\r\n");
    }
    HAL_GPIO_WritePin(TOF_Front.GPIO_Port, TOF_Front.GPIO_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    /* Reset I2C again before the actual per-sensor probing */
    HAL_I2C_DeInit(&hi2c1);
    MX_I2C1_Init();
    HAL_Delay(10);

    printf("[DiagTOF] Probing each TOF sensor...\r\n");

    if(Detect_TOF(&TOF_Front))
    {
        count++;
        printf("[DiagTOF]   Front    -> DETECTED\r\n");
    }
    else
        printf("[DiagTOF]   Front    -> NOT DETECTED\r\n");

    if(Detect_TOF(&TOF_Left))
    {
        count++;
        printf("[DiagTOF]   Left     -> DETECTED\r\n");
    }
    else
        printf("[DiagTOF]   Left     -> NOT DETECTED\r\n");

    if(Detect_TOF(&TOF_Right))
    {
        count++;
        printf("[DiagTOF]   Right    -> DETECTED\r\n");
    }
    else
        printf("[DiagTOF]   Right    -> NOT DETECTED\r\n");

    if(Detect_TOF(&TOF_LeftDiag))
    {
        count++;
        printf("[DiagTOF]   LeftDiag -> DETECTED\r\n");
    }
    else
        printf("[DiagTOF]   LeftDiag -> NOT DETECTED\r\n");

    if(Detect_TOF(&TOF_RightDiag))
    {
        count++;
        printf("[DiagTOF]   RightDiag-> DETECTED\r\n");
    }
    else
        printf("[DiagTOF]   RightDiag-> NOT DETECTED\r\n");

    printf("[DiagTOF] TOF detection complete. Total found: %u/5\r\n", count);
    return count;
}
bool Detect_5IR(void)
{
    uint8_t count = 0;

    for(uint8_t i=0; i<5; i++)
    {
        if(IR_Value[i] > 4000)
        {
            count++;
        }
    }

    return (count == 5);
}
bool Detect_3IR(void)
{
    if(IR_Value[0] > 4000 &&
       IR_Value[1] > 4000 &&
       IR_Value[2] > 4000)
    {
        return true;
    }

    return false;
}
bool Detect_TOF(TOF_SensorConfig_t *sensor)
{
    return TOF_Probe_By_XSHUT(sensor);
}
uint8_t Detect_US_Count(void)
{
    uint8_t count = 0;
    bool us_front = false;
    bool us_left = false;
    bool us_right = false;

    /* Use analog readback on shared A0/A1/A2 echo lines for attachment hint. */
    Runtime_GPIO_Config_IR(5);
    HAL_Delay(5);

    us_front = ADC_Channel_Is_Stable_In_Range(ADC_CHANNEL_1,
                                              US_ADC_MIN_COUNTS,
                                              US_ADC_MAX_COUNTS,
                                              US_ADC_MAX_DELTA,
                                              US_ADC_SAMPLE_COUNT,
                                              US_ADC_SAMPLE_DELAY_MS);
    if (us_front)
    {
        count++;
    }

    us_left = ADC_Channel_Is_Stable_In_Range(ADC_CHANNEL_2,
                                             US_ADC_MIN_COUNTS,
                                             US_ADC_MAX_COUNTS,
                                             US_ADC_MAX_DELTA,
                                             US_ADC_SAMPLE_COUNT,
                                             US_ADC_SAMPLE_DELAY_MS);
    if (us_left)
    {
        count++;
    }

    us_right = ADC_Channel_Is_Stable_In_Range(ADC_CHANNEL_3,
                                              US_ADC_MIN_COUNTS,
                                              US_ADC_MAX_COUNTS,
                                              US_ADC_MAX_DELTA,
                                              US_ADC_SAMPLE_COUNT,
                                              US_ADC_SAMPLE_DELAY_MS);
    if (us_right)
    {
        count++;
    }

    printf("[AutoDetect] US ADC window: %u..%u counts\r\n",
           (unsigned)US_ADC_MIN_COUNTS,
           (unsigned)US_ADC_MAX_COUNTS);
    printf("[AutoDetect] US channels F:%d L:%d R:%d\r\n",
           us_front ? 1 : 0,
           us_left ? 1 : 0,
           us_right ? 1 : 0);

    return count;
}

static bool TOF_Probe_By_XSHUT(TOF_SensorConfig_t *sensor)
{
    if (sensor == NULL)
    {
        return false;
    }

    printf("  [Probe] %s: ", sensor->Name);
    
    HAL_GPIO_WritePin(sensor->GPIO_Port, sensor->GPIO_Pin, GPIO_PIN_SET);
    printf("XSHUT=HIGH ");
    HAL_Delay(40);

    HAL_StatusTypeDef ready = HAL_I2C_IsDeviceReady(&hi2c1,
                                                     TOF_DEFAULT_ADDR_8BIT,
                                                     TOF_READY_TRIALS,
                                                     TOF_READY_TIMEOUT_MS);

    printf("I2C_Ready=%s ", (ready == HAL_OK) ? "YES" : "NO");

    HAL_GPIO_WritePin(sensor->GPIO_Port, sensor->GPIO_Pin, GPIO_PIN_RESET);
    printf("XSHUT=LOW\r\n");
    HAL_Delay(5);

    sensor->Online = (ready == HAL_OK);
    return sensor->Online;
}

static bool ADC_Channel_Is_Stable_In_Range(uint32_t channel,
                                           uint16_t min_count,
                                           uint16_t max_count,
                                           uint16_t max_delta,
                                           uint8_t sample_count,
                                           uint32_t sample_delay_ms)
{
    uint16_t sample = 0U;
    uint16_t observed_min = 0xFFFFU;
    uint16_t observed_max = 0U;

    if (sample_count == 0U)
    {
        return false;
    }

    for (uint8_t i = 0; i < sample_count; i++)
    {
        sample = ADC_Read_Channel(channel);

        if (sample < observed_min)
        {
            observed_min = sample;
        }

        if (sample > observed_max)
        {
            observed_max = sample;
        }

        if ((i + 1U) < sample_count)
        {
            HAL_Delay(sample_delay_ms);
        }
    }

    printf("[ADC] CH%lu: min=%u  max=%u  delta=%u  range=[%u..%u]\r\n",
           channel, observed_min, observed_max, (observed_max - observed_min),
           min_count, max_count);

    if (observed_min < min_count)
    {
        printf("[ADC] CH%lu FAIL: observed_min (%u) < min_count (%u)\r\n",
               channel, observed_min, min_count);
        return false;
    }

    if (observed_max > max_count)
    {
        printf("[ADC] CH%lu FAIL: observed_max (%u) > max_count (%u)\r\n",
               channel, observed_max, max_count);
        return false;
    }

    if ((observed_max - observed_min) > max_delta)
    {
        printf("[ADC] CH%lu FAIL: delta (%u) > max_delta (%u)\r\n",
               channel, (observed_max - observed_min), max_delta);
        return false;
    }

    printf("[ADC] CH%lu PASS\r\n", channel);
    return true;
}
void Sesnor_Board_1_3TOF(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_1_3TOF;

    TOF_Enable = 1;
    active_tof_sensor_count = 3;

    TOF_Setup_Multi(3);

    printf("Board1 : 3 TOF Loaded\r\n");
}
void Read_3TOF(void)
{
    TOF_Read_All_Sesnor_data();
}
void Print_3TOF(void)
{
    printf("F:%4d  L:%4d  R:%4d\r\n",
           Front_distance,
           Left_distance,
           Right_distance);
}
void Read_5TOF(void)
{
    TOF_Read_All_Sesnor_data();
}
void Print_5TOF(void)
{
    printf("F:%4d  L:%4d  R:%4d  LD:%4d  RD:%4d\r\n",
           Front_distance,
           Left_distance,
           Right_distance,
           Left_diag_distance,
           Right_diag_distance);
}
void Sesnor_Board_1_2TOF_1Ultrasonic(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_1_2TOF_1US;

    /* Physical hybrid layout: Front = Ultrasonic, Left/Right = TOF. */
    TOF_Enable = true;
    active_tof_sensor_count = 3;
    active_ultrasonic_sensor_count = 1;

    TOF_Setup_LeftRight();
    Runtime_GPIO_Config_Ultrasonic(1);

    printf("Board1 : 2TOF + 1US Loaded\r\n");
}
void Read_2TOF_1US(void){
	TOF_Read_All_Sesnor_data();
	Ultrasonic_Read_All_Sensor_data();
}
void Print_2TOF_1US(void)
{
    printf("TOF[L:%4d R:%4d]  US:%5.1fcm\r\n",
           Left_distance,
           Right_distance,
           Front_distance_cm);
}
void Sensor_Board_2_3Ultrasonic(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_2_3US;

    active_ultrasonic_sensor_count = 3;
    Runtime_GPIO_Config_Ultrasonic(3);

    printf("Board1 : 3 Ultrasonic Loaded\r\n");
}
void Read_3US(void)
{
    Ultrasonic_Read_All_Sensor_data();
}
void Print_3US(void)
{
    printf("F:%5.1f  L:%5.1f  R:%5.1f cm\r\n",
           Front_distance_cm,
           Left_distance_cm,
           Right_distance_cm);
}
void Sesnor_Board_2_1TOF_2Ultrasonic(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_2_1TOF_2US;

    /* Physical hybrid layout: Front = TOF, Left/Right = Ultrasonic. */
    TOF_Enable = true;
    active_tof_sensor_count = 1;
    active_ultrasonic_sensor_count = 2;

    TOF_Setup_Front();
    Runtime_GPIO_Config_Ultrasonic_LeftRight();

    printf("Board2 : 1TOF + 2US Loaded\r\n");
}
void Read_1TOF_2US(void)
{
    TOF_Read_All_Sesnor_data();
    Ultrasonic_Read_All_Sensor_data();
}
void Print_1TOF_2US(void)
{
    printf("TOF:%4d  US[L:%5.1f R:%5.1f]\r\n",
           Front_distance,
           Left_distance_cm,
           Right_distance_cm);
}
void Sesnor_Board_2_1TOF_5IR(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_2_1TOF_5IR;

    TOF_Enable = true;
    active_tof_sensor_count = 1;

    TOF_Setup_Front();
    Runtime_GPIO_Config_IR(5);

    printf("Board2 : 1TOF + 5IR Loaded\r\n");
}
void Read_1TOF_5IR(void)
{
    TOF_Read_All_Sesnor_data();
    IR_Read_All();
}
void Print_1TOF_5IR(void)
{
    printf("TOF:%4d | IR:%4d %4d %4d %4d %4d\r\n",
           Front_distance,
           IR_Value[0],
           IR_Value[1],
           IR_Value[2],
           IR_Value[3],
           IR_Value[4]);
}
void Sesnor_Board_2_5IR(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_2_5IR;

    Runtime_GPIO_Config_IR(5);

    printf("Board2 : 5IR Loaded\r\n");
}
void Read_5IR(void)
{
    IR_Read_All();
}
void Print_5IR(void)
{
    printf("IR : %4d %4d %4d %4d %4d\r\n",
            IR_Value[0],
            IR_Value[1],
            IR_Value[2],
            IR_Value[3],
            IR_Value[4]);
}
void Sesnor_Board_2_3IR(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_2_3IR;

    Runtime_GPIO_Config_IR(3);

    printf("Board2 : 3IR Loaded\r\n");
}
void Read_3IR(void)
{
    IR_Read_All();
}
void Print_3IR(void)
{
    printf("IR : %4d %4d %4d\r\n",
            IR_Value[0],
            IR_Value[1],
            IR_Value[2]);
}
void Sesnor_Board_2_1Ultrasonic_5IR(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_2_1US_5IR;

    /*
     * A0 shared with Echo0.
     * Validate hardware configuration.
     */

    active_ultrasonic_sensor_count = 1;
    Runtime_GPIO_Config_Ultrasonic(1);
    Runtime_GPIO_Config_IR(5);

    printf("Board2 : 1US + 5IR Loaded\r\n");
}
void Read_1US_5IR(void)
{
    Ultrasonic_Read_All_Sensor_data();
    IR_Read_All();
}
void Print_1US_5IR(void)
{
	// Placement issues will occur and A0 pins will be shared with Ech0 of Ultrasonic 1 + Placement issues.

    printf("US:%5.1f | IR:%4d %4d %4d %4d %4d\r\n",
           Front_distance_cm,
           IR_Value[0],
           IR_Value[1],
           IR_Value[2],
           IR_Value[3],
           IR_Value[4]);
}
void Sesnor_Board_2_1Ultrasonic_3IR(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_2_1US_3IR;

    active_ultrasonic_sensor_count = 1;
    Runtime_GPIO_Config_Ultrasonic(1);
    Runtime_GPIO_Config_IR(3);

    printf("Board2 : 1US + 3IR Loaded\r\n");
}
void Read_1US_3IR(void)
{
    Ultrasonic_Read_All_Sensor_data();
    IR_Read_All();
}
void Print_1US_3IR(void)
{
    printf("US:%5.1f | IR:%4d %4d %4d\r\n",
           Front_distance_cm,
           IR_Value[0],
           IR_Value[1],
           IR_Value[2]);
}
void Sesnor_Board_2_3Ultrasonic_5IR(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_2_3US_5IR;

    /* PA1-PA3 are shared between US echo and IR analog inputs.
     * Pins are switched dynamically on every Read_3US_5IR() call:
     *   digital input  → US measurement
     *   analog         → IR measurement
     * Start in US (digital) mode as the default. */
    active_ultrasonic_sensor_count = 3;
    Runtime_GPIO_Config_Ultrasonic(3);

    printf("Board2 : 3US + 5IR Loaded\r\n");
}
void Read_3US_5IR(void)
{
    /* Switch PA1-PA3 to digital input for ultrasonic echo capture */
    Runtime_GPIO_Config_Ultrasonic(3);
    Ultrasonic_Read_All_Sensor_data();

    /* Switch PA1-PA5 to analog for IR ADC read */
    Runtime_GPIO_Config_IR(5);
    IR_Read_All();

    /* Leave pins in digital (US) mode for next cycle */
    Runtime_GPIO_Config_Ultrasonic(3);
}
void Print_3US_5IR(void)
{
    printf("US[F:%5.1f L:%5.1f R:%5.1f] | "
           "IR:%4d %4d %4d %4d %4d\r\n",

           Front_distance_cm,
           Left_distance_cm,
           Right_distance_cm,

           IR_Value[0],
           IR_Value[1],
           IR_Value[2],
           IR_Value[3],
           IR_Value[4]);
}
void Sesnor_Board_1_5TOF(void)
{
    Sensor_Board_DeInit_All();

    ActiveBoard = BOARD_1_5TOF;
    TOF_Enable = true;
    active_tof_sensor_count = 5;

    TOF_Setup_Multi(5);

    printf("Board1 : 5 TOF Loaded\r\n");
}
void HardwareFault_Report(HardwareFault_t fault)
{
    CurrentHardwareFault = fault;

    hardware_fault_active = true;

    hardware_fault_start_tick = HAL_GetTick();
    hardware_fault_toggle_tick = HAL_GetTick();

    hardware_fault_led_state = false;
}
void HardwareFault_Task(void)
{
    if (!hardware_fault_active)
    {
        return;
    }

    if ((HAL_GetTick() - hardware_fault_toggle_tick) >= 100)
    {
        hardware_fault_toggle_tick = HAL_GetTick();

        hardware_fault_led_state =
            !hardware_fault_led_state;

        if (hardware_fault_led_state)
        {
            RGB_Red_On();
        }
        else
        {
            RGB_Off();
        }
    }

    if ((HAL_GetTick() - hardware_fault_start_tick) >= 2000)
    {
        RGB_Off();

        hardware_fault_active = false;
    }
}
void Sensor_Error_Start(void)
{
    hardware_fault_active = true;

    hardware_fault_start_tick = HAL_GetTick();
    hardware_fault_toggle_tick = HAL_GetTick();

    hardware_fault_led_state = false;
}
/* ========================= I2C Utilities ========================= */
void I2C_Scan(I2C_HandleTypeDef *hi2c, const char *bus_name)
{
    uint8_t addr;
    HAL_StatusTypeDef result;

    printf("\r\n[I2C SCAN] Bus: %s\r\n", bus_name);

    for (addr = 1; addr < 128; addr++)
    {
        result = HAL_I2C_IsDeviceReady(
                    hi2c,
                    (uint16_t)(addr << 1),
                    2,
                    5);

        if (result == HAL_OK)
        {
            printf("  Device found at 0x%02X\r\n", addr);
        }
    }

    printf("[I2C SCAN] %s Done\r\n", bus_name);
}
/* ========================= USB CDC Utilities ========================= */
void USB_CDC_Test(){
	if (USB_RxReadyFS)
	{
	    uint32_t len = USB_RxLenFS;

	    // Clear flag early (so next packet can arrive)
	    USB_RxReadyFS = 0;
	    USB_RxLenFS = 0;

	    if (len >= sizeof(rx_buf)) len = sizeof(rx_buf) - 1;

	    memcpy(rx_buf, UserRxBufferFS, len);
	    rx_buf[len] = '\0';

	    // Optional: strip CR/LF
	    while (len > 0 && (rx_buf[len-1] == '\r' || rx_buf[len-1] == '\n')) {
	    	rx_buf[len-1] = '\0';
	        len--;
	    }

	    USB_Send("RX: ");
	    USB_Send(rx_buf);
	    USB_Send("\r\n");
	}

}
void USB_Send(const char *s)
{
    while (CDC_Transmit_FS((uint8_t*)s, strlen(s)) == USBD_BUSY) {
        // tiny wait; prevents back-to-back TX overlap
    }
}
void USB_CheckReceive(void)
{
    USBD_CDC_HandleTypeDef *hcdc =
        (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;

    if (hcdc == NULL)
        return;

    if (hcdc->RxLength > 0)
    {
        uint32_t len = hcdc->RxLength;

        if (len > sizeof(rx_buf))
            len = sizeof(rx_buf);

        memcpy(rx_buf, UserRxBufferFS, len);
        rx_buf[len] = '\0';

        hcdc->RxLength = 0;   // 🔑 CLEAR after reading
        rx_ready = 1;
    }
}
void USB_StatusTask(void)
{
    static uint32_t last_tick = 0;

    if (HAL_GetTick() - last_tick > 3000)
    {
        last_tick = HAL_GetTick();
        USB_Send("STM32 USB CDC Running...\r\n");
    }
}
/* ========================= Math Helpers ========================= */
uint16_t map_uint(uint16_t x,
                  uint16_t in_min, uint16_t in_max,
                  uint16_t out_min, uint16_t out_max)
{
    return (uint32_t)(x - in_min) * (out_max - out_min)
           / (in_max - in_min)
           + out_min;
}
float mapf(float val, float I_Min, float I_Max, float O_Min, float O_Max)
{
	return(((val-I_Min)*((O_Max-O_Min)/(I_Max-I_Min)))+O_Min);
}
/* ========================= Retarget printf/scanf ========================= */
PUTCHAR_PROTOTYPE {
    /* UART1 */
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    /* UART6 */
    HAL_UART_Transmit(&huart6, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    /* USB CDC */
    CDC_Transmit_FS((uint8_t *)&ch, 1);

    return ch;
}
GETCHAR_PROTOTYPE {
	/* Place your implementation of fgetc here */
	/* e.g. readwrite a character to the USART2 and Loop until the end of transmission */
	uint8_t ch = 0;
	while (HAL_OK != HAL_UART_Receive(&huart1, (uint8_t*) &ch, 1, 10)) {
		;
	}
	return ch;
}
/* ========================= HAL Callbacks ========================= */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    /* ------- Bluetooth RX (USART6) ------- */
    if (huart->Instance == USART6)
    {
        Handle_BT_Char((char)bt_rx_byte);

        HAL_UART_Receive_IT(&huart6, &bt_rx_byte, 1); // re-arm
    }

    /* ------- UART1 (CLI / Menu) ------- */
    if (huart->Instance == USART1)
    {
        if (uart_rx_byte == '\n')
        {
            uart_rx_buffer[uart_rx_index] = '\0';
            uart_cmd_ready = 1;
            uart_rx_index = 0;
        }
        else if (uart_rx_index < UART_RX_BUFFER_SIZE - 1)
        {
            uart_rx_buffer[uart_rx_index++] = uart_rx_byte;
        }

        HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);
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

#ifdef  USE_FULL_ASSERT
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

----------------------------------------------------I2C------------------------
  /* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
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
#include "i2c.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

I2C_HandleTypeDef hi2c1;

/* I2C1 init function */
void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2cHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspInit 0 */

  /* USER CODE END I2C1_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PB8     ------> I2C1_SCL
    PB9     ------> I2C1_SDA
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  /* Internal ~40k pull-up: workaround for boards missing external 4.7k pull-ups */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* I2C1 clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
  /* USER CODE BEGIN I2C1_MspInit 1 */

  /* USER CODE END I2C1_MspInit 1 */
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{

  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspDeInit 0 */

  /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PB8     ------> I2C1_SCL
    PB9     ------> I2C1_SDA
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_9);

  /* USER CODE BEGIN I2C1_MspDeInit 1 */

  /* USER CODE END I2C1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
----------------------------------------main.h----------------------
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Motor_Error_Pin_Pin GPIO_PIN_13
#define Motor_Error_Pin_GPIO_Port GPIOC
#define Select_Switch_1_Pin GPIO_PIN_0
#define Select_Switch_1_GPIO_Port GPIOC
#define Select_Switch_2_Pin GPIO_PIN_1
#define Select_Switch_2_GPIO_Port GPIOC
#define Select_Switch_3_Pin GPIO_PIN_2
#define Select_Switch_3_GPIO_Port GPIOC
#define Select_Switch_4_Pin GPIO_PIN_3
#define Select_Switch_4_GPIO_Port GPIOC
#define Battery_Voltage_Pin GPIO_PIN_0
#define Battery_Voltage_GPIO_Port GPIOA
#define A0_Pin GPIO_PIN_1
#define A0_GPIO_Port GPIOA
#define A1_Pin GPIO_PIN_2
#define A1_GPIO_Port GPIOA
#define A2_Pin GPIO_PIN_3
#define A2_GPIO_Port GPIOA
#define A3_Pin GPIO_PIN_4
#define A3_GPIO_Port GPIOA
#define A4_Pin GPIO_PIN_5
#define A4_GPIO_Port GPIOA
#define Motor_1A_Pin GPIO_PIN_6
#define Motor_1A_GPIO_Port GPIOA
#define Motor_1B_Pin GPIO_PIN_7
#define Motor_1B_GPIO_Port GPIOA
#define INT_Open_Drain_IMU_Pin GPIO_PIN_4
#define INT_Open_Drain_IMU_GPIO_Port GPIOC
#define FSync_IMU_Pin GPIO_PIN_5
#define FSync_IMU_GPIO_Port GPIOC
#define Motor_2B_Pin GPIO_PIN_0
#define Motor_2B_GPIO_Port GPIOB
#define Motor_2A_Pin GPIO_PIN_1
#define Motor_2A_GPIO_Port GPIOB
#define BOOT_2_Pin GPIO_PIN_2
#define BOOT_2_GPIO_Port GPIOB
#define G1_Pin GPIO_PIN_10
#define G1_GPIO_Port GPIOB
#define SPI_2_CS_Pin GPIO_PIN_12
#define SPI_2_CS_GPIO_Port GPIOB
#define RGB_Red_Pin GPIO_PIN_8
#define RGB_Red_GPIO_Port GPIOC
#define RGB_Green_Pin GPIO_PIN_9
#define RGB_Green_GPIO_Port GPIOC
#define Motor1_Encoder_A_Pin GPIO_PIN_15
#define Motor1_Encoder_A_GPIO_Port GPIOA
#define SPI_3_CS_Pin GPIO_PIN_2
#define SPI_3_CS_GPIO_Port GPIOD
#define Motor1_Encoder_B_Pin GPIO_PIN_3
#define Motor1_Encoder_B_GPIO_Port GPIOB
#define RGB_Blue_Pin GPIO_PIN_5
#define RGB_Blue_GPIO_Port GPIOB
#define Motor2_Encoder_A_Pin GPIO_PIN_6
#define Motor2_Encoder_A_GPIO_Port GPIOB
#define Motor2_Encoder_B_Pin GPIO_PIN_7
#define Motor2_Encoder_B_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define TOF_enableFront_Pin GPIO_PIN_1
#define TOF_enableFront_GPIO_Port GPIOA
#define TOF_enableLeft_Pin GPIO_PIN_2
#define TOF_enableLeft_GPIO_Port GPIOA
#define TOF_enableRight_Pin GPIO_PIN_3
#define TOF_enableRight_GPIO_Port GPIOA

#define TOF_enableDiagLeft_Pin GPIO_PIN_4
#define TOF_enableDiagLeft_GPIO_Port GPIOA
#define TOF_enableDiagRight_Pin GPIO_PIN_5
#define TOF_enableDiagRight_GPIO_Port GPIOA

#define TOF_intFront_Pin GPIO_PIN_10
#define TOF_intFront_GPIO_Port GPIOB
#define TOF_intRight_Pin GPIO_PIN_4
#define TOF_intRight_GPIO_Port GPIOA
#define TOF_intLeft_Pin GPIO_PIN_5
#define TOF_intLeft_GPIO_Port GPIOA
////////////////////////////////////////////////
#define Ultrasonic_Front_TRIG_PORT GPIOB
#define Ultrasonic_Front_TRIG_PIN  GPIO_PIN_10
#define Ultrasonic_Front_ECHO_PORT GPIOA
#define Ultrasonic_Front_ECHO_PIN  GPIO_PIN_1
#define Ultrasonic_Left_TRIG_PORT GPIOA
#define Ultrasonic_Left_TRIG_PIN  GPIO_PIN_4
#define Ultrasonic_Left_ECHO_PORT GPIOA
#define Ultrasonic_Left_ECHO_PIN  GPIO_PIN_2
#define Ultrasonic_Right_TRIG_PORT GPIOA
#define Ultrasonic_Right_TRIG_PIN  GPIO_PIN_5
#define Ultrasonic_Right_ECHO_PORT GPIOA
#define Ultrasonic_Right_ECHO_PIN  GPIO_PIN_3


/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */


  
