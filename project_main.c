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

<<<<<<< HEAD
/* Task stacks */
#define STACKSIZE           500
#define STACKSIZE_MEDIUM    1000
#define STACKSIZE_LARGE     1500
Char mpuSensorTask_Stack[STACKSIZE_LARGE];
Char lightSensorTask_Stack[STACKSIZE_MEDIUM];
Char gestureAnalysisTask_Stack[STACKSIZE];
Char uartWriteTask_Stack[STACKSIZE_LARGE];
Char uartReadTask_Stack[STACKSIZE];
Char signalTask_Stack[STACKSIZE];
Char playBackgroundSongTask_Stack[STACKSIZE_MEDIUM];
=======
/* State machine */
enum state { AWAKE=1, DATA_READY, GEST_FOUND, GEST_NOT_FOUND, COMMAND_SENT, BEEP_HEARD,SIGNAL_PLAYED, QUIET, WAKING_UP, FALLING_ASLEEP,ASLEEP };
enum state programState = WAITING;
>>>>>>> bfc0101a224c64c44a4d761f75ad2961443a72c2

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
   PIN_TERMINATE // Asetustaulukko lopetetaan aina t√§ll√§ vakiolla
};

// timeout Clock
static Clock_Handle timeoutClock_Handle;
static Clock_Params timeoutClock_Params;


/* Enums for State and Gesture */
enum state { IDLE_STATE=0, READING_MPU_DATA, DETECTING_LIGHT_LEVEL, ANALYSING_DATA, SENDING_MESSAGE_UART, SIGNALLING_TO_USER,
            LISTENING_UART, BEEP_RECIEVED, PLAYING_BACKGROUND_MUSIC, NO_RESPONSE_RECIEVED };
enum state defaultStartState = DETECTING_LIGHT_LEVEL;
enum state programState = DETECTING_LIGHT_LEVEL;


enum gesture { NO_GESTURE=0, PETTING, PLAYING, SLEEPING, EATING, WALKING };
enum gesture currentGesture = NO_GESTURE;


enum message { NO_MESSAGE=0, TOO_FULL, LOW_HEALTH, DEATH, NO_PONG_RECIEVED, DEACTIVATED_SM, ACTIVATED_SM };
enum message currentMessage = NO_MESSAGE;


/* Global flags */
uint8_t uartInterruptFlag = 0;
uint8_t pongRecievedFlag = 1;
uint8_t lastCommWasBeepFlag = 1;    // tells the program if it sent a command or recieved a beep last

/* Global variables */
uint8_t uartBuffer[BUFFER_SIZE];
char printBuffer[100];
int16_t commandSentTime = 0;
float buttonRight_PressTime = 0.0;
int MPU_setup_complete = 0;

/* DATA */
float MPU_data[MPU_DATA_SPAN][7];
float MPU_data_buffer[SLIDING_MEAN_WINDOW][7];


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
        "ACTIVATE:1;1;1",
        "id:2231,MSG1:Health: ##--- 40%",
        "id:2231,MSG2:State 2 / Value 2.21",
        "ENERGY:3"
    };

    UART_Params_init(&uartParams);
    uartParams.baudRate      = 9600;
    uartParams.readMode      = UART_MODE_CALLBACK; // Keskeytyspohjainen vastaanotto
    uartParams.readCallback  = &uartFxn; // K√§sittelij√§funktio
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

            sprintf(uartMsg, "%s,%s,ping", tag_id, msg[3]);
            System_printf("Sending uart message: %s\n", uartMsg);
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

<<<<<<< HEAD

/* 
 * 
 */
Void uartReadTask_Fxn(UArg arg0, UArg arg1)
=======
/* TASKS */
static void Signal_sending(UArg arg0, UArg arg1){ //SLEEP in every if??
    if (programState == COMMAND_SENT){
        //play song 1
        programState = SIGNAL_PLAYED;
    }
    if(programState == BEEP_HEARD){
        //play song2
        programState = SIGNAL_PLAYED;
    }
    if(programState == FALLING_ASLEEP){
        //play song 3
        //check if light under treshhold
                programState = ASLEEP;
    }
    if(programState == WAKING_UP){
        //play song 4
        //check if light above treshhold
                programState = AWAKE;
    }
    if (programState == ASLEEP) {
        //check light level
        //play song 5 OR WAKING_UP
    }
    Task_sleep(100*1000 / Clock_tickPeriod);
}
/* INTERRRRUPT HANDLER */


static void uartFxn(UART_Handle handle, void *rxBuf, size_t len) {

   UART_read(handle, rxBuf, BUFFER_SIZE);
   uartInterruptFlag = 1;
}

static void uartTask(UArg arg0, UArg arg1) {

   UART_Handle uartHandle;
   UART_Params uartParams;

   char rec_msg[255];
   char uartMsg[BUFFER_SIZE];
   char msg[MESSAGE_COUNT][BUFFER_SIZE] = {
        "id:2231,ping",
        "id:2231,PET:1",
        "id:2231,EAT:1",
   };

   UART_Params_init(&uartParams);
   uartParams.baudRate      = 9600;
   uartParams.readMode      = UART_MODE_CALLBACK; // Keskeytyspohjainen vastaanotto
   uartParams.readCallback  = &uartFxn; // K‰sittelij‰funktio
   uartParams.readDataMode  = UART_DATA_TEXT;
   uartParams.writeDataMode = UART_DATA_TEXT;

    // UART k‰yttˆˆn ohjelmassa
   uartHandle = UART_open(Board_UART, &uartParams);
   if (uartHandle == NULL) {
      System_abort("Error opening the UART");
   }

   // Nyt tarvitsee k‰ynnist‰‰ datan odotus

   while(1) {

       if (programState == GEST_NOT_FOUND || programState == SIGNAL_PLAYED){


       if(UART_read(uartHandle, uartBuffer, BUFFER_SIZE)){
           //parse beep
           programState = BEEP_HEARD;
       }
       else{
           programState = QUIET;

       }

       }
       // sending message with UART through serial port
       if (programState == GEST_FOUND && currentGesture != NONE) { //PROGRAM STATE GEST_FOUND
           sprintf(uartMsg, msg[currentGesture]);
           System_printf("uartMsg is: %s\n", uartMsg);
           System_flush();
           UART_write(uartHandle, uartMsg, sizeof(uartMsg));
           memset(uartMsg, 0, BUFFER_SIZE);
           programState = COMMAND_SENT;
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
>>>>>>> bfc0101a224c64c44a4d761f75ad2961443a72c2
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
        if (programState == DETECTING_LIGHT_LEVEL || programState == PLAYING_BACKGROUND_MUSIC) {

            
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

            // if SM not deactivated
            if (programState != IDLE_STATE) {
                if ( lux < Sleep_Light_Threshold ) {
                    //System_printf("Dark enough to sleep\n");
                    //System_flush();
                    programState = PLAYING_BACKGROUND_MUSIC;
                
                } else {
                    programState = DETECTING_LIGHT_LEVEL;
                }
            }
        }
        SLEEP(1000);
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

<<<<<<< HEAD
            //System_printf("In state: MpuSensorTask\n");
            //System_flush();
=======
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

            if (!dataToPrintFlag && programState == AWAKE) // start data collecting after not collecting
            {
                dataToPrintFlag = 1;
                setZeroMpuData(MPU_data, MPU_DATA_SPAN);
                System_printf("\nStarting data collection\n");
                System_flush();
            }
>>>>>>> bfc0101a224c64c44a4d761f75ad2961443a72c2

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

<<<<<<< HEAD
            addMpuData(MPU_data, MPU_DATA_SPAN, time, 10*ax, 10*ay, 10*az, gx, gy, gz);

            if (programState != IDLE_STATE) programState = ANALYSING_DATA;
=======
        } else { // not collecting data // NEEDS TO BE ON ITS OWN TASK
            if (dataToPrintFlag)
            {
                dataToPrintFlag = 0;
                System_printf("Data collection stopped\n");
                System_flush();
                //printMpuData(MPU_data, MPU_DATA_SPAN);
                programState = DATA_READY;
                currentGesture = NONE;
            }
            if (!setupNeededFlag) setupNeededFlag = 1;
>>>>>>> bfc0101a224c64c44a4d761f75ad2961443a72c2
        }
        SLEEP(100);
    }
}


/* 
 * 
 */
Void gestureAnalysisTask_Fxn(UArg arg0, UArg arg1)
{
    //float variance[7], mean[7], max[7], min[7];

    while (1) {
        if ( programState == ANALYSING_DATA ) {

            //System_printf("In state: gestureAnalysis\n");
            //System_flush();

            //analyseData(MPU_data, variance, mean, max, min);
            //printMpuData(MPU_data, 1);

            if (isPetting(MPU_data, MPU_DATA_SPAN)) {
                System_printf("Petting detected!\n");
                System_flush();
<<<<<<< HEAD
                currentGesture = PETTING;

            } else {
                currentGesture = NO_GESTURE;
            }

            // change state
            if (currentGesture != NO_GESTURE) {
                if (programState != IDLE_STATE) programState = SENDING_MESSAGE_UART;

            } else {
                if (programState != IDLE_STATE) programState = LISTENING_UART;
=======
                currentGesture = PETTING;//A FLAG FOR WHICH GESTURE
                programState = GEST_FOUND;//GEST_FOUND
            } else {
                currentGesture = NONE;
                programState = GEST_NOT_FOUND; //NO_GEST_FOUND
>>>>>>> bfc0101a224c64c44a4d761f75ad2961443a72c2
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

            System_printf("Signaling to user with buzzer!\n");
            System_flush();
            playSong(buzzerHandle, gesture_detected_signal);

            if (programState != IDLE_STATE) programState = LISTENING_UART;

        } else if (currentMessage != NO_MESSAGE) { // Signal about BEEP message and activation of SM  

            if (currentMessage == LOW_HEALTH) {
                playSong(buzzerHandle, low_health_signal);

            } else if (currentMessage == NO_PONG_RECIEVED) {
                playSong(buzzerHandle, message_not_recieved_signal);
            
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

            System_printf("STARTING: LULLABY\n");
            System_flush();

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
    
    Task_Params_init(&mpuSensorTask_Params);
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
    }
    //MPU_setup_complete = 1;
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
    uartReadTask_Params.stackSize = STACKSIZE;
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
    buttonRight_Handle = PIN_open( &buttonRight_State, buttonRight_Config );
    if(!buttonRight_Handle) {
        System_abort("Error initializing button 0\n");
    }
    if (PIN_registerIntCb( buttonRight_Handle, &buttonRight_Fxn ) != 0) {
        System_abort("Error registering button 0 callback function");
    }

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

