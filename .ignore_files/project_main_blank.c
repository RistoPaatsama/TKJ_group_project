/* C Standard library */
#include <stdio.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
#include "custom/DataHandling.h"
#include "custom/Music.h"
#include "custom/Songs.h"

/* Marcos */
#define SLEEP(ms)               Task_sleep((ms)*1000 / Clock_tickPeriod)
#define GET_TIME_DS             (Double)Clock_getTicks() * (Double)Clock_tickPeriod / 100000

#define MPU_DATA_SPAN           10
#define SLIDING_MEAN_WINDOW     3
#define BUFFER_SIZE             80
#define MESSAGE_COUNT           10

/* Task stacks */
#define STACKSIZE 2048
Char mpuSensorTask_Stack[STACKSIZE];
Char lightSensorTask_Stack[STACKSIZE];
Char gestureAnalysisTask_Stack[STACKSIZE];
Char uartWriteTask_Stack[STACKSIZE];
Char uartReadTask_Stack[STACKSIZE];
Char signalTask_Stack[STACKSIZE];
Char playBackgroundSongTask_Stack[STACKSIZE];

/* PIN VARIABLES */
// MPU
static PIN_Handle MPUPowerPinHandle;
static PIN_State MPUPowerPinState;

PIN_Config MPUPowerPinConfig[] = {
   Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

/* Enums for State and Gesture */
enum state { IDLE=0, READING_MPU_DATA, DETECTING_LIGHT_LEVEL, ANALYSING_DATA, SENDING_MESSAGE_UART, SIGNALLING_TO_USER, 
            LISTENING_UART, BEEP_RECIEVED, PLAYING_BACKGROUND_MUSIC };
enum state programState = READING_MPU_DATA;

enum gesture { NONE=0, PETTING, PLAYING, SLEEPING, EATING, WALKING };
enum gesture currentGesture = NONDE;


/* Global flags */
uint8_t uartInterruptFlag = 0;

/* Global variables */
uint8_t uartBuffer[BUFFER_SIZE];


/* DATA */
float MPU_data[MPU_DATA_SPAN][7];
float MPU_data_buffer[SLIDING_MEAN_WINDOW][7];


/* INTERRUPT HANDLERS */

static void uartFxn(UART_Handle handle, void *rxBuf, size_t len) 
{
    uartInterruptFlag = 1;
    UART_read(handle, rxBuf, BUFFER_SIZE);
}


/* TASK FUNCTIONS */

static void uartWriteTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization

    while (1) {

        if (programState == SENDING_MESSAGE_UART) {
            // execute state function

        }
        SLEEP(100);
    }
}


Void uartReadTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization

    while (1) {

        if (programState == LISTENING_UART) {
            // execute state function

            programState = DETECTING_LIGHT_LEVEL;
        }
        SLEEP(100);
    }
}


Void mpuSensorTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization
    float time, ax, ay, az, gx, gy, gz;
    float new_data[7], new_mean_data[7];

    I2C_Handle      i2cMPU;
    I2C_Params      i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    PIN_setOutputValue(MPUPowerPinHandle, Board_MPU_POWER, Board_MPU_POWER_ON);

    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // Open MPU I2C channel and setup MPU
    i2cMPU = I2C_open(Board_I2C0, &i2cMPUParams);
    if (i2cMPU == NULL) {
      System_abort("Error Initializing I2C\n");
    }

    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();

    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    I2C_close(i2cMPU);

    while (1)
    {
        if (programState == READING_MPU_DATA)
        {
            i2cMPU = I2C_open(Board_I2C0, &i2cMPUParams);
            if (i2cMPU == NULL) {
                System_abort("Error Initializing I2C\n");
            }
            mpu9250_get_data(&i2cMPU, new_data);
            time = GET_TIME_DS;
            I2C_close(i2cMPU);

            //addData(MPU_data_buffer, SLIDING_MEAN_WINDOW, new_data);
            //getAverage(MPU_data_buffer, new_mean_data);
            //new_mean_data[0] = time;
            addData(MPU_data, MPU_DATA_SPAN, new_data);

            programState = ANALYSING_DATA;
        }
        SLEEP(100);
    }
}


Void lightSensorTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization

    while (1) {

        if (programState == DETECTING_LIGHT_LEVEL) {
            // execute state function

            programState = READING_MPU_DATA;
        }
        SLEEP(100);
    }
}


Void gestureAnalysisTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization
    float variance[7], mean[7], max[7], min[7];

    while (1)
    {
        if ( programState == ANALYSING_DATA )
        {
            //analyseData(MPU_data, variance, mean, max, min);

            if (isPetting(MPU_data)) {
                currentGesture = PETTING;
            } (isPlaying(MPU_data)) {
                currentGesture = PLAYING;
            } else {
              currentGesture = NONE;
            }

            programState = LISTENING_UART;
        }
        SLEEP(100);
    }
}


Void signalTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization

    while (1)
    {
        if (programState == SIGNALLING_TO_USER) {
            // 

        } else if (programState == BEEP_RECIEVED) {
            // 
        }
        SLEEP(50);
    }
}


Void playBackgroundSongTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization

    while (1) {

        if (programState == PLAYING_BACKGROUND_MUSIC) {
            // execute state function
            
        }
        SLEEP(100);
    }
}





/* MAIN */
Int main(void) {

    /* Task handles and params */
    Task_Handle mpuSensorTask_Handle;
    Task_Params mpuSensorTask_Params;

    Task_Handle lightSensorTask_Handle;
    Task_Params lightSensorTask_Params;

    Task_Handle gestureAnalysisTask_Handle;
    Task_Params gestureAnalysisTask_Params;

    Task_Handle uartWriteTask_Handle;
    Task_Params uartWriteTask_Params;

    Task_Handle uartReadTask_Handle;
    Task_Params uartReadTask_Params;

    Task_Handle signalTask_Handle;
    Task_Params signalTask_Params;

    Task_Handle playBackgroundSongTask_Handle;
    Task_Params playBackgroundSongTask_Params;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();
    Board_initI2C();

    /* Initializing Tasks */
    Task_Params_init(&mpuSensorTask_Params);
    mpuSensorTask_Params.stackSize = STACKSIZE;
    mpuSensorTask_Params.stack = &mpuSensorTask_Stack;
    mpuSensorTask_Params.priority=2;
    mpuSensorTask_Handle = Task_create(mpuSensorTask_Fxn, &mpuSensorTask_Params, NULL);
    if (mpuSensorTask_Handle == NULL) {
        System_abort("mpuSensorTask create failed!");
    }

    Task_Params_init(&lightSensorTask_Params);
    lightSensorTask_Params.stackSize = STACKSIZE;
    lightSensorTask_Params.stack = &lightSensorTask_Stack;
    lightSensorTask_Params.priority=2;
    lightSensorTask_Handle = Task_create(lightSensorTask_Fxn, &lightSensorTask_Params, NULL);
    if (lightSensorTask_Handle == NULL) {
        System_abort("lightSensorTask create failed!");
    }

    Task_Params_init(&gestureAnalysisTask_Params);
    gestureAnalysisTask_Params.stackSize = STACKSIZE;
    gestureAnalysisTask_Params.stack = &gestureAnalysisTask_Stack;
    gestureAnalysisTask_Params.priority=2;
    gestureAnalysisTask_Handle = Task_create(gestureAnalysisTask_Fxn, &gestureAnalysisTask_Params, NULL);
    if (gestureAnalysisTask_Handle == NULL) {
        System_abort("gestureAnalysisTask create failed!");
    }

    Task_Params_init(&uartWriteTask_Params);
    uartWriteTask_Params.stackSize = STACKSIZE;
    uartWriteTask_Params.stack = &uartWriteTask_Stack;
    uartWriteTask_Params.priority=2;
    uartWriteTask_Handle = Task_create(uartWriteTask_Fxn, &uartWriteTask_Params, NULL);
    if (uartWriteTask_Handle == NULL) {
        System_abort("uartWriteTask create failed!");
    }

    Task_Params_init(&uartReadTask_Params);
    uartReadTask_Params.stackSize = STACKSIZE;
    uartReadTask_Params.stack = &uartReadTask_Stack;
    uartReadTask_Params.priority=2;
    uartReadTask_Handle = Task_create(uartReadTask_Fxn, &uartReadTask_Params, NULL);
    if (uartReadTask_Handle == NULL) {
        System_abort("uartReadTask create failed!");
    }

    Task_Params_init(&signalTask_Params);
    signalTask_Params.stackSize = STACKSIZE;
    signalTask_Params.stack = &signalTask_Stack;
    signalTask_Params.priority=2;
    signalTask_Handle = Task_create(signalTask_Fxn, &signalTask_Params, NULL);
    if (signalTask_Handle == NULL) {
        System_abort("signalTask create failed!");
    }

    Task_Params_init(&playBackgroundSongTask_Params);
    playBackgroundSongTask_Params.stackSize = STACKSIZE;
    playBackgroundSongTask_Params.stack = &playBackgroundSongTask_Stack;
    playBackgroundSongTask_Params.priority=2;
    playBackgroundSongTask_Handle = Task_create(playBackgroundSongTask_Fxn, &playBackgroundSongTask_Params, NULL);
    if (playBackgroundSongTask_Handle == NULL) {
        System_abort("playBackgroundSongTask create failed!");
    }


    /* Power pin for MPU */
    MPUPowerPinHandle = PIN_open( &MPUPowerPinState, MPUPowerPinConfig );
    if(!MPUPowerPinHandle) {
      System_abort("Error initializing MPU power pin\n");
    }


    /* Sanity check */
    System_printf("Setup complete! Starting BIOS!\n\n\n");
    System_flush();

    BIOS_start();

    return (0);
}
