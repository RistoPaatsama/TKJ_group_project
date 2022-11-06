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

#define SLEEP(ms) Task_sleep((ms)*1000 / Clock_tickPeriod)

/* Task stacks */
#define STACKSIZE 2048
Char mpuSensorTask_Stack[STACKSIZE];
Char lightSensorTask_Stack[STACKSIZE];
Char uartWriteTask_Stack[STACKSIZE];
Char uartReadTask_Stack[STACKSIZE];
Char gestureAnalysisTask_Stack[STACKSIZE];
Char signalTask_Stack[STACKSIZE];
Char playBackgroundSongTask_Stack[STACKSIZE];


/* Power pin for MPU */
static PIN_Handle MPUPowerPinHandle;
static PIN_State MPUPowerPinState;
// Hox! Samalle painonapille kaksi erilaista konfiguraatiota
PIN_Config MPUPowerPinConfig[] = {
   Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};
// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

Int main(void) {

    // Task variables
    Task_Handle mpuSensorTask_Handle;
    Task_Params mpuSensorTask_Params;

    Task_Handle lightSensorTask_Handle;
    Task_Params lightSensorTask_Params;

    Task_Handle uartWriteTask_Handle;
    Task_Params uartWriteTask_Params;

    Task_Handle uartReadTask_Handle;
    Task_Params uartReadTask_Params;

    Task_Handle gestureAnalysisTask_Handle;
    Task_Params gestureAnalysisTask_Params;

    Task_Handle signalTask_Handle;
    Task_Params signalTask_Params;

    Task_Handle playBackgroundSongTask_Handle;
    Task_Params playBackgroundSongTask_Params;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();
    Board_initI2C();

    /* Initializing Tasks */
    Task_Params_init(&dataCollectionTaskParams);
    dataCollectionTaskParams.stackSize = STACKSIZE;
    dataCollectionTaskParams.stack = &sensorTaskStack;
    dataCollectionTaskParams.priority=2;
    dataCollectionTaskHandle = Task_create(dataCollectionTaskFxn, &dataCollectionTaskParams, NULL);
    if (dataCollectionTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }


    /* Power pin for MPU */
    MPUPowerPinHandle = PIN_open( &MPUPowerPinState, MPUPowerPinConfig );
    if(!MPUPowerPinHandle) {
      System_abort("Error initializing MPU power pin\n");
    }

    /* Push buttons */
    button0Handle = PIN_open( &button0State, button0Config );
    if(!button0Handle) {
        System_abort("Error initializing button 0\n");
    }
    if (PIN_registerIntCb( button0Handle, &button0Fxn ) != 0) {
        System_abort("Error registering button 0 callback function");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    BIOS_start();

    return (0);
}
