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
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "wireless/comm_lib.h"
#include "sensors/opt3001.h"

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

/* State machine */
enum state { WAITING=1, DATA_READY };
enum state programState = WAITING;

double ambientLight = -1000.0;

/* Power pin for MPU */
static PIN_Handle MPUPowerPinHandle;
static PIN_State MPUPowerPinState;

// Hox! Samalle painonapille kaksi erilaista konfiguraatiota
PIN_Config MPUPowerPinConfig[] = {
   Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE
};

/* Push buttons */
int button0Flag = 0;

static PIN_Handle button0Handle;
static PIN_State button0State;

PIN_Config button0Config[] = {
    Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES,
    PIN_TERMINATE
};

void button0Fxn(PIN_Handle handle, PIN_Id pinId)
{
    uint_t buttonValue = PIN_getInputValue( pinId );

    if (buttonValue) {
        button0Flag = 0;
        //System_printf("Button 0 released\n");
        //System_flush();
    } else {
        button0Flag = 1;
        //System_printf("Button 0 pressed\n");
        //System_flush();
    }
}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {

    // JTKJ: Teht‰v‰ 4. Lis‰‰ UARTin alustus: 9600,8n1
    // JTKJ: Exercise 4. Setup here UART connection as 9600,8n1

    while (1) {

        // JTKJ: Teht‰v‰ 3. Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
        //       Muista tilamuutos
        // JTKJ: Exercise 3. Print out sensor data as string to debug window if the state is correct
        //       Remember to modify state

        // JTKJ: Teht‰v‰ 4. L‰het‰ sama merkkijono UARTilla
        // JTKJ: Exercise 4. Send the same sensor data string with UART

        // Just for sanity check for exercise, you can comment this out
        System_printf("uartTask\n");
        System_flush();

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1)
{
    double lux;
    char merkkijono[64];

    // I2C setup
    I2C_Handle      i2c,        i2cMPUCfg;
    I2C_Params      i2cParams,  i2cMPUParams;

    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // HUOM: need to assign Board_I2C0_SDA1 ja Board_I2C0_SCL1 to i2cMPUCfg
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    // HUOM: need to assign i2cMPUCfg to i2cMPUParams

    /* Open MPU I2C channel and setup MPU */
    i2cMPUCfg = I2C_open(Board_I2C0, &i2cMPUParams); // Board_I2C_TMP should be something different. Maybe Board_I2C0 ??
    if (i2cMPUCfg == NULL) {
      System_abort("Error Initializing I2C\n");
    }

    PIN_setOutputValue( MPUPowerPinHandle, Board_MPU_POWER, 1 ); // turn on MPU power pin
    Task_sleep(100000 / Clock_tickPeriod);
    mpu9250_setup(&i2cMPUCfg);
    I2C_close(i2cMPUCfg);


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
        if (0)
        {
            System_printf("(Button 0 pressed) ");
            System_flush();
            button0Flag = 0;
        }
        // JTKJ: Exercise 2. Read sensor data and print it to the Debug window as string
        lux = opt3001_get_data(&i2c);
        sprintf(merkkijono, "(in sensorTask) light intensity: %.4f lux\n", lux);

        System_printf(merkkijono);
        System_flush();

        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();
    Init6LoWPAN();
    Board_initI2C();

    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
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

    /* Start BIOS */
    BIOS_start();

    return (0);
}
