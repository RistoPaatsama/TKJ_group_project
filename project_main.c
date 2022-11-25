/* C Standard library */
#include <stdio.h>
#include <string.h>

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
#include "sensors/bmp280.h"
#include "custom/DataHandling.h"
#include "custom/Utilities.h"
#include "custom/Music.h"
#include "custom/Songs.h"
#include "custom/gestures.h"

/* Marcos */
#define SLEEP(ms)               Task_sleep((ms)*1000 / Clock_tickPeriod)
#define GET_TIME_DS             (Double)Clock_getTicks() * (Double)Clock_tickPeriod / 100000

#define MPU_DATA_SPAN           10
#define SLIDING_MEAN_WINDOW     3
#define BUFFER_SIZE             80
#define MESSAGE_COUNT           10

/* Task stacks */
#define STACKSIZE           512
#define STACKSIZE_MEDIUM    1024
#define STACKSIZE_LARGE     2048
Char mpuSensorTask_Stack[STACKSIZE_LARGE];
Char lightSensorTask_Stack[STACKSIZE_MEDIUM];
Char pressureSensorTask_Stack[STACKSIZE_LARGE];
Char gestureAnalysisTask_Stack[STACKSIZE];
Char uartWriteTask_Stack[STACKSIZE_LARGE];
Char uartReadTask_Stack[STACKSIZE_LARGE];
Char signalTask_Stack[STACKSIZE];
Char playBackgroundSongTask_Stack[STACKSIZE_MEDIUM];

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

// Buzzer
static PIN_Handle buzzerHandle;
static PIN_State sBuzzer;
PIN_Config cBuzzer[] = {
    Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// Right button
static PIN_Handle buttonRight_Handle;
static PIN_State buttonRight_State;

PIN_Config buttonRight_Config[] = {
    Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES,
    PIN_TERMINATE
};

// LED RED
static PIN_Handle ledRed_Handle;
static PIN_State ledRed_State;

PIN_Config ledRed_Config[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// timeout Clock
static Clock_Handle timeoutClock_Handle;
static Clock_Params timeoutClock_Params;


/* Enums for State and Gesture */
enum state { IDLE_STATE=0, READING_MPU_DATA, DETECTING_LIGHT_LEVEL, ANALYSING_DATA, SENDING_MESSAGE_UART, SIGNALLING_TO_USER,
            LISTENING_UART, BEEP_RECIEVED, PLAYING_BACKGROUND_MUSIC, NO_RESPONSE_RECIEVED, READING_BMP_DATA };
enum state defaultStartState = READING_BMP_DATA;
enum state programState = READING_BMP_DATA;


enum gesture { NO_GESTURE=0, PETTING, PLAYING, SLEEPING, EATING, WALKING };
enum gesture currentGesture = NO_GESTURE;


enum message { NO_MESSAGE=0, TOO_FULL, LOW_HEALTH, DEATH, NO_PONG_RECIEVED, DEACTIVATED_SM, ACTIVATED_SM };
enum message currentMessage = NO_MESSAGE;


/* Global flags */
uint8_t uartInterruptFlag = 0;
uint8_t pongRecievedFlag = 1;
uint8_t lastCommWasBeepFlag = 1;    // tells the program if it sent a command or recieved a beep last
uint8_t highPressureflag = 0;

/* Global variables */
uint8_t uartBuffer[BUFFER_SIZE];
char printBuffer[100];
int16_t commandSentTime = 0;
float buttonRight_PressTime = 0.0;
int MPU_setup_complete = 0;

/* DATA */
float MPU_data[MPU_DATA_SPAN][7];
float MPU_data_buffer[SLIDING_MEAN_WINDOW][7];

double pressure;
double temp_comp;


/* INTERRUPT HANDLERS */

static void uartFxn(UART_Handle handle, void *rxBuf, size_t len) 
{
    uartInterruptFlag = 1;
    UART_read(handle, rxBuf, BUFFER_SIZE);
}


Void timeoutClock_Fxn(UArg arg0)
{
    if (pongRecievedFlag == 0) {
        currentMessage = NO_PONG_RECIEVED;
    }
    Clock_stop(timeoutClock_Handle);
}

/**/
Void buttonRight_Fxn(PIN_Handle handle, PIN_Id pinId)
{
    uint_t buttonValue = PIN_getInputValue( pinId );

    if (buttonValue) { // button released
        
        // 

    } else { // button pushed

        if ( (GET_TIME_DS - buttonRight_PressTime) > 1 )
        {

            System_printf("SM activation toggled! State: %d\n", programState);
            System_flush();

            if (programState != IDLE_STATE) {
                programState = IDLE_STATE;
                currentMessage = DEACTIVATED_SM;
                PIN_setOutputValue( ledRed_Handle, Board_LED1, 1 );

            } else {
                programState = defaultStartState;
                currentMessage = ACTIVATED_SM;
                PIN_setOutputValue( ledRed_Handle, Board_LED1, 0 );
            }

            buttonRight_PressTime = GET_TIME_DS;
        }
    }
    
}


/* TASK FUNCTIONS */


/* 
 * 
 */
static void uartWriteTask_Fxn(UArg arg0, UArg arg1)
{
    UART_Handle uartHandle;
    UART_Params uartParams;

    char tag_id[] = "id:2231";
    char uartMsg[BUFFER_SIZE];
    memset(uartMsg, 0, BUFFER_SIZE);
    char msg[MESSAGE_COUNT][BUFFER_SIZE] = {
        "ping",
        "PET:1",
        "EAT:1",
        "EXERCISE:1",
        "ACTIVATE:1;1;1",
        "id:2231,MSG1:Health: ##--- 40%",
        "id:2231,MSG2:State 2 / Value 2.21",
        "ENERGY:3"
    };

    UART_Params_init(&uartParams);
    uartParams.baudRate      = 9600;
    uartParams.readMode      = UART_MODE_CALLBACK; // Keskeytyspohjainen vastaanotto
    uartParams.readCallback  = &uartFxn; // K�sittelij�funktio
    uartParams.readDataMode  = UART_DATA_TEXT;
    uartParams.writeDataMode = UART_DATA_TEXT;

    uartHandle = UART_open(Board_UART, &uartParams);
    if (uartHandle == NULL) {
        System_abort("Error opening the UART");
    }

    UART_read(uartHandle, uartBuffer, BUFFER_SIZE);

    // LOOP
    while (1) {

        if (programState == SENDING_MESSAGE_UART) {

            if (currentGesture == PETTING) {
                sprintf(uartMsg, "%s,%s,ping", tag_id, msg[1]);
                System_printf("%s\n", uartMsg);
            } else if(currentGesture == EATING){
                sprintf(uartMsg, "%s,%s,ping", tag_id, msg[2]);
                System_printf("%s\n", uartMsg);
            } else if(currentGesture == PLAYING){
                sprintf(uartMsg, "%s,%s,ping", tag_id, msg[3]);
                System_printf("%s\n", uartMsg);
            }
            //System_printf("Sending uart message: %s\n", uartMsg);
            System_flush();
            UART_write(uartHandle, uartMsg, sizeof(uartMsg));
            Clock_start(timeoutClock_Handle); // timeout clock for checking pong
            pongRecievedFlag = 0;
            lastCommWasBeepFlag = 0;

            if (programState != IDLE_STATE) programState = SIGNALLING_TO_USER;
        }
        SLEEP(100);
    }
}


/* 
 * 
 */
Void uartReadTask_Fxn(UArg arg0, UArg arg1)
{
    char uartMsgRec[80];
    char beep_msg[] = "2231,BEEP:";
    char pong_msg[] = "pong";
    char low_health_msg1[] = "Running";
    char low_health_msg2[] = "Severe warning";
    char low_health_msg3[] = "I could";
    char too_full_msg1[] = "Calm down";
    char too_full_msg2[] = "Too fitness";
    char too_full_msg3[] = "Feels good";

    char death_msg[] = "Too late";

    int msg_start_index = 10;

    while (1) {
        if (programState == LISTENING_UART) 
        {

            if (uartInterruptFlag == 1)
            {
                uartInterruptFlag = 0;

                strcpy(uartMsgRec, uartBuffer);
                memset(uartBuffer, 0, BUFFER_SIZE);

                System_printf("Revieced UART message: %s\n", uartMsgRec);
                System_flush();

                if (stringContainsAt(uartMsgRec, beep_msg, 0)) { // BEEP RECIEVED

                    if ( lastCommWasBeepFlag == 1 &&
                        stringContainsAt(uartMsgRec, low_health_msg1, msg_start_index) ||
                        stringContainsAt(uartMsgRec, low_health_msg2, msg_start_index) ||
                        stringContainsAt(uartMsgRec, low_health_msg3, msg_start_index) )
                    {
                        currentMessage = LOW_HEALTH;
                    }

                    else if ( lastCommWasBeepFlag == 0 &&
                                stringContainsAt(uartMsgRec, too_full_msg1, msg_start_index) ||
                                stringContainsAt(uartMsgRec, too_full_msg2, msg_start_index) ||
                                stringContainsAt(uartMsgRec, too_full_msg3, msg_start_index) )
                    {
                        currentMessage = TOO_FULL;
                    }

                    else if (stringContainsAt(uartMsgRec, death_msg, msg_start_index))
                    {
                        currentMessage = DEATH;
                    }

                    lastCommWasBeepFlag = 1;
                
                } else if (stringContainsAt(uartMsgRec, pong_msg, 0)) { // PONG RECIEVED

                    if (pongRecievedFlag == 0) {
                        pongRecievedFlag = 1;
                    }
                }
            }
            if (programState != IDLE_STATE) programState = READING_MPU_DATA;
        }
        //SLEEP(100);
    }
}


/* 
 * 
 */
Void pressureSensorTask_Fxn(UArg arg0, UArg arg1)
{
    System_printf("BMP280: Entering Task ...\n");
    System_flush();

    I2C_Handle      i2cBMP;
    I2C_Params      i2cBMPparams;

    while (0){
        SLEEP(100);
    }

    I2C_Params_init(&i2cBMPparams);
    i2cBMPparams.bitRate = I2C_400kHz;

    System_printf("BMP280: Opening I2C handle ...\n");
    System_flush();

    i2cBMP = I2C_open(Board_I2C_TMP, &i2cBMPparams);
    if (i2cBMP == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    System_printf("BMP280: Setting up I2C ...\n");
    System_flush();

    SLEEP(100);
    bmp280_setup(&i2cBMP);

    System_printf("BMP280: Setup OK\n");
    System_flush();

    I2C_close(i2cBMP);

    playSong(buzzerHandle, tetris_theme_song);

    while (1) {
        if (programState == READING_BMP_DATA) {

            i2cBMP = I2C_open(Board_I2C_TMP, &i2cBMPparams);
            if (i2cBMP == NULL) {
                System_abort("Error Initializing I2C\n");
            }
            bmp280_get_data(&i2cBMP, &pressure, &temp_comp);
            I2C_close(i2cBMP);

            sprintf(printBuffer, "%.2f, %.2f\n", pressure, temp_comp);
            System_printf(printBuffer);
            System_flush();
        }
        SLEEP(100);
    }
}



Void lightSensorTask_Fxn(UArg arg0, UArg arg1)
{
    double lux;
    double Sleep_Light_Threshold = 5;

    I2C_Handle      i2c;
    I2C_Params      i2cParams;

    while (MPU_setup_complete == 0){
        SLEEP(100);
    }

    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    System_printf("OPT3001: Opening I2C handle ...\n");
    System_flush();

    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    System_printf("OPT3001: Setting up I2C ...\n");
    System_flush();

    SLEEP(100);
    opt3001_setup(&i2c);

    System_printf("OPT3001: Setup OK\n");
    System_flush();

    I2C_close(i2c);

    while (1) {
        if (programState == DETECTING_LIGHT_LEVEL || programState == PLAYING_BACKGROUND_MUSIC || programState == READING_MPU_DATA) {

            
            i2c = I2C_open(Board_I2C_TMP, &i2cParams);
            if (i2c == NULL) {
                System_abort("Error Initializing I2C\n");
            }
            int i;
            for (i = 0; i < 10; i++){
                lux = opt3001_get_data(&i2c);
                if (lux != -1) break;
            }
            I2C_close(i2c);

            sprintf(printBuffer, "Light level: %.2f\n", lux);
            System_printf(printBuffer);
            System_flush();

            if (programState != IDLE_STATE) {
                if ( lux < Sleep_Light_Threshold ) {

                    programState = PLAYING_BACKGROUND_MUSIC;
                
                } else {
                    programState = READING_MPU_DATA;
                }
            }
        }
        SLEEP(999);
    }
}


/* 
 * 
 */
Void mpuSensorTask_Fxn(UArg arg0, UArg arg1)
{
    float time, ax, ay, az, gx, gy, gz;
    //float new_data[7], new_mean_data[7];

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
    MPU_setup_complete = 1;

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    I2C_close(i2cMPU);

    while (1) {
        if (programState == READING_MPU_DATA) {

            //System_printf("In state: MpuSensorTask\n");
            //System_flush();

            i2cMPU = I2C_open(Board_I2C0, &i2cMPUParams);
            if (i2cMPU == NULL) {
                System_abort("Error Initializing I2C\n");
            }
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
            time = GET_TIME_DS;
            I2C_close(i2cMPU);

            //addData(MPU_data_buffer, SLIDING_MEAN_WINDOW, new_data);
            //getAverage(MPU_data_buffer, new_mean_data);
            //new_mean_data[0] = time;

            addMpuData(MPU_data, MPU_DATA_SPAN, time, 10*ax, 10*ay, 10*az, gx, gy, gz);

            if (programState != IDLE_STATE) programState = ANALYSING_DATA;
        }
        SLEEP(100);
    }
}


/* 
 * 
 */
Void gestureAnalysisTask_Fxn(UArg arg0, UArg arg1)
{

    while (1) {
        if ( programState == ANALYSING_DATA ) {

            if (isPetting(MPU_data, MPU_DATA_SPAN)) {
                System_printf("Petting detected!\n");
                System_flush();
                currentGesture = PETTING;
            } else if (isEating(MPU_data, MPU_DATA_SPAN)) {
                System_printf("Eating detected!\n");
                System_flush();
                currentGesture = EATING;
            } else if (isPlaying(MPU_data, MPU_DATA_SPAN)) {
                System_printf("Playing detected!\n");
                System_flush();
                currentGesture = PLAYING;
            } else {
                currentGesture = NO_GESTURE;
            }

            // change state
            if (currentGesture != NO_GESTURE) {
                if (programState != IDLE_STATE) programState = SENDING_MESSAGE_UART;

            } else {
                if (programState != IDLE_STATE) programState = LISTENING_UART;
            }
        }
        SLEEP(100);
    }
}


/* 
 * 
 */
Void signalTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization

    while (1) {
        if (programState == SIGNALLING_TO_USER) { // Signal about gesture
            playSong(buzzerHandle, gesture_detected_signal);

            if (programState != IDLE_STATE) programState = LISTENING_UART;

        } else if (currentMessage != NO_MESSAGE) { // Signal about BEEP message and activation of SM  

            if (currentMessage == LOW_HEALTH) {
                playSong(buzzerHandle, low_health_signal);

            /*} else if (currentMessage == NO_PONG_RECIEVED) {
                playSong(buzzerHandle, message_not_recieved_signal);*/
            
            } else if (currentMessage == DEATH) {
                playSong(buzzerHandle, requiem);
            
            } else if (currentMessage == TOO_FULL) {
                playSong(buzzerHandle, too_full_signal);

            } else if (currentMessage == DEACTIVATED_SM) {
                playSong(buzzerHandle, deactivate_signal);
                System_printf("SM Deactivated\n");
                System_flush();

            } else if (currentMessage == ACTIVATED_SM) {
                playSong(buzzerHandle, activate_signal);
                System_printf("SM Activated\n");
                System_flush();

            }

            currentMessage = NO_MESSAGE;
            if (programState != IDLE_STATE) programState = READING_MPU_DATA; //LISTENING_UART;
        }
        SLEEP(100);
    }
}


/* 
 * 
 */
Void playBackgroundSongTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization testing

    while (1) {
        if (programState == PLAYING_BACKGROUND_MUSIC) {

            // execute state function
            playSongInterruptible(buzzerHandle, lullaby, &programState, PLAYING_BACKGROUND_MUSIC);
            //playSong(buzzerHandle, lullaby);
            
            //if (programState != IDLE_STATE) programState = READING_MPU_DATA;
        }
        SLEEP(50);
    }
}




/* MAIN */
Int main(void) {

    /* Task handles and params */
    Task_Handle mpuSensorTask_Handle;
    Task_Params mpuSensorTask_Params;

    Task_Handle lightSensorTask_Handle;
    Task_Params lightSensorTask_Params;

    Task_Handle pressureSensorTask_Handle;
    Task_Params pressureSensorTask_Params;

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
    Board_initUART();

    /* Initializing Tasks */
    
    /*Task_Params_init(&mpuSensorTask_Params);
    mpuSensorTask_Params.stackSize = STACKSIZE_LARGE;
    mpuSensorTask_Params.stack = &mpuSensorTask_Stack;
    mpuSensorTask_Params.priority=2;
    mpuSensorTask_Handle = Task_create(mpuSensorTask_Fxn, &mpuSensorTask_Params, NULL);
    if (mpuSensorTask_Handle == NULL) {
        System_abort("mpuSensorTask create failed!");
    }
    

    Task_Params_init(&lightSensorTask_Params);
    lightSensorTask_Params.stackSize = STACKSIZE_MEDIUM;
    lightSensorTask_Params.stack = &lightSensorTask_Stack;
    lightSensorTask_Params.priority=2;
    lightSensorTask_Handle = Task_create(lightSensorTask_Fxn, &lightSensorTask_Params, NULL);
    if (lightSensorTask_Handle == NULL) {
        System_abort("lightSensorTask create failed!");
    }*/

    
    Task_Params_init(&pressureSensorTask_Params);
    pressureSensorTask_Params.stackSize = STACKSIZE_LARGE;
    pressureSensorTask_Params.stack = &pressureSensorTask_Stack;
    pressureSensorTask_Params.priority=2;
    pressureSensorTask_Handle = Task_create(pressureSensorTask_Fxn, &pressureSensorTask_Params, NULL);
    if (pressureSensorTask_Handle == NULL) {
        System_abort("pressureSensorTask create failed!");
    }
    
/*
    Task_Params_init(&gestureAnalysisTask_Params);
    gestureAnalysisTask_Params.stackSize = STACKSIZE;
    gestureAnalysisTask_Params.stack = &gestureAnalysisTask_Stack;
    gestureAnalysisTask_Params.priority=2;
    gestureAnalysisTask_Handle = Task_create(gestureAnalysisTask_Fxn, &gestureAnalysisTask_Params, NULL);
    if (gestureAnalysisTask_Handle == NULL) {
        System_abort("gestureAnalysisTask create failed!");
    }
    

    Task_Params_init(&uartWriteTask_Params);
    uartWriteTask_Params.stackSize = STACKSIZE_LARGE;
    uartWriteTask_Params.stack = &uartWriteTask_Stack;
    uartWriteTask_Params.priority=2;
    uartWriteTask_Handle = Task_create(uartWriteTask_Fxn, &uartWriteTask_Params, NULL);
    if (uartWriteTask_Handle == NULL) {
        System_abort("uartWriteTask create failed!");
    }

    Task_Params_init(&uartReadTask_Params);
    uartReadTask_Params.stackSize = STACKSIZE_LARGE;
    uartReadTask_Params.stack = &uartReadTask_Stack;
    uartReadTask_Params.priority=1;
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
    playBackgroundSongTask_Params.stackSize = STACKSIZE_MEDIUM;
    playBackgroundSongTask_Params.stack = &playBackgroundSongTask_Stack;
    playBackgroundSongTask_Params.priority=2;
    playBackgroundSongTask_Handle = Task_create(playBackgroundSongTask_Fxn, &playBackgroundSongTask_Params, NULL);
    if (playBackgroundSongTask_Handle == NULL) {
        System_abort("playBackgroundSongTask create failed!");
    }*/


    /* Power pin for MPU */
    MPUPowerPinHandle = PIN_open( &MPUPowerPinState, MPUPowerPinConfig );
    if(!MPUPowerPinHandle) {
      System_abort("Error initializing MPU power pin\n");
    }

    /* Pin for buzzer */
    buzzerHandle = PIN_open(&sBuzzer, cBuzzer);
    if (buzzerHandle == NULL) {
        System_abort("Pin open failed!");
    }

    /* Button */
    /*
    buttonRight_Handle = PIN_open( &buttonRight_State, buttonRight_Config );
    if(!buttonRight_Handle) {
        System_abort("Error initializing button 0\n");
    }
    if (PIN_registerIntCb( buttonRight_Handle, &buttonRight_Fxn ) != 0) {
        System_abort("Error registering button 0 callback function");
    }
    */

    /* LEDs */
    ledRed_Handle = PIN_open(&ledRed_State, ledRed_Config);
    if(!ledRed_Handle) {
      System_abort("Error initializing LED pins\n");
    }

    /* Clock for checking ping timeout */
    Clock_Params_init(&timeoutClock_Params);
    timeoutClock_Params.period = 0;
    timeoutClock_Params.startFlag = FALSE;

    timeoutClock_Handle = Clock_create((Clock_FuncPtr)timeoutClock_Fxn, 1000*1000 / Clock_tickPeriod, &timeoutClock_Params, NULL);
    if (timeoutClock_Handle == NULL) {
      System_abort("Clock create failed");
    }


    /* Sanity check */
    System_printf("Setup complete! Starting BIOS!\n\n\n");
    System_flush();

    BIOS_start();
    programState = defaultStartState;

    return (0);
}

