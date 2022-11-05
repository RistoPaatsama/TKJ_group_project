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

#define GET_TIME_DS     (Double)Clock_getTicks() * (Double)Clock_tickPeriod / 100000
#define MPU_DATA_SPAN 40
#define BUFFER_SIZE 80

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

/* State machine */
enum state { WAITING=1, DATA_READY };
enum state programState = WAITING;

uint8_t uartInterruptFlag = 0;
uint8_t uartBuffer[BUFFER_SIZE];
double ambientLight = -1000.0;

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

/* Push buttons */
int button0Flag = 0;

static PIN_Handle button0Handle;
static PIN_State button0State;

PIN_Config button0Config[] = {
    Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES,
    PIN_TERMINATE
};

int collectDataFlag = 0;


//const int MPU_DATA_SPAN = 40; // 10 readings per second
float MPU_data[MPU_DATA_SPAN][7];


/* button interrupt */
void button0Fxn(PIN_Handle handle, PIN_Id pinId)
{
    uint_t buttonValue = PIN_getInputValue( pinId );

    if (buttonValue) {
        //button0Flag = 0;
        //System_printf("Button 0 released\n");
        //System_flush();
    } else { // button pushed
        collectDataFlag = !collectDataFlag;
        //button0Flag = 1;
        //System_printf("Button 0 pressed\n");
        //System_flush();
    }
}

/* TASKS */
/* INTERRRRUPT HANDLER */
static void uartFxn(UART_Handle handle, void *rxBuf, size_t len) {

   UART_read(handle, rxBuf, BUFFER_SIZE);
   uartInterruptFlag = 1;
}

static void uartTask(UArg arg0, UArg arg1) {

   UART_Handle uartHandle;
   UART_Params uartParams;

   char rec_msg[255];
   char msg[BUFFER_SIZE];

   UART_Params_init(&uartParams);
   uartParams.baudRate      = 9600;
   uartParams.readMode      = UART_MODE_CALLBACK; // Keskeytyspohjainen vastaanotto
   uartParams.readCallback  = &uartFxn; // Käsittelijäfunktio
   uartParams.readDataMode  = UART_DATA_TEXT;
   uartParams.writeDataMode = UART_DATA_TEXT;

    // UART käyttöön ohjelmassa
   uartHandle = UART_open(Board_UART, &uartParams);
   if (uartHandle == NULL) {
      System_abort("Error opening the UART");
   }

   // Nyt tarvitsee käynnistää datan odotus
   UART_read(uartHandle, uartBuffer, BUFFER_SIZE);

   while(1) {

       // sending message with UART through serial port
       if (programState == DATA_READY) {
           sprintf(msg, "id:2231,EAT:1");
           UART_write(uartHandle, msg, sizeof(msg));
           memset(msg, 0, BUFFER_SIZE);
       }

       if (uartInterruptFlag == 1) {
           sprintf(rec_msg, "Interrupt happened. Received message is: %s\n", uartBuffer);
           System_printf("%s\n", rec_msg);
           System_flush();
           uartInterruptFlag = 0;
       }
       programState = WAITING;
       Task_sleep(100*1000 / Clock_tickPeriod);
   }
}

Void dataCollectionTaskFxn(UArg arg0, UArg arg1)
{
    //System_printf("sensorTaskFxn()\n");
    //System_flush();

    int dataToPrintFlag = 0;
    int setupNeededFlag = 0;
    int firstTimeFlag = 1;

    double lux;
    char buffer[128];

    // I2C setup
    I2C_Handle      i2c,        i2cMPU;
    I2C_Params      i2cParams,  i2cMPUParams;

    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

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


    // open other I2C channel and setup periferals
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL) {
      System_abort("Error Initializing I2C\n");
    }

    Task_sleep(100000 / Clock_tickPeriod);
    opt3001_setup(&i2c);
    I2C_close(i2c);

    while (1)
    {
        float time, ax, ay, az, gx, gy, gz;

        /*sprintf(buffer, "time since program start in 1/10th seconds %.5f\n", GET_TIME_DS );
        System_printf(buffer);
        System_flush();*/

        // Read light sensor data
        /*i2c = I2C_open(Board_I2C_TMP, &i2cParams);
        if (i2c == NULL) {
            System_abort("Error Initializing I2C\n");
        }
        lux = opt3001_get_data(&i2c);
        I2C_close(i2c);*/

        //sprintf(buffer, "(in sensorTask) light intensity: %.4f lux\n", lux);

        //System_printf(buffer);
        //System_flush();

        /* GET AND PRINT MPU DATA */

        if (collectDataFlag)
        {
            if (setupNeededFlag && !firstTimeFlag) // setup again after button press
            {
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
                setupNeededFlag = 0;
            }
            firstTimeFlag = 0;

            if (!dataToPrintFlag && programState == WAITING) // start data collecting after not collecting
            {
                dataToPrintFlag = 1;
                setZeroMpuData(MPU_data, MPU_DATA_SPAN);
                System_printf("\nStarting data collection\n");
                System_flush();
            }

            i2cMPU = I2C_open(Board_I2C0, &i2cMPUParams);
            if (i2cMPU == NULL) {
                System_abort("Error Initializing I2C\n");
            }
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
            time = GET_TIME_DS;
            I2C_close(i2cMPU);

            addMpuData(MPU_data, MPU_DATA_SPAN, time, ax, ay, az, gx, gy, gz);
            // programState = DATA_READY;

        } else { // not collecting data
            if (dataToPrintFlag)
            {
                dataToPrintFlag = 0;
                System_printf("Data collection stopped");
                System_flush();
                printMpuData(MPU_data, MPU_DATA_SPAN);
                programState = DATA_READY;
            }
            if (!setupNeededFlag) setupNeededFlag = 1;
        }
        Task_sleep(100*1000 / Clock_tickPeriod);
    }
}

Int main(void) {

    // Task variables
    Task_Handle dataCollectionTaskHandle;
    Task_Params dataCollectionTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;


    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();

    // Init i2c bus
    Board_initI2C();

    // init serial communication port
    Board_initUART();

    /* Task */
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
    uartTaskHandle = Task_create(uartTask, &uartTaskParams, NULL);
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
