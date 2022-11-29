/* C Standard library */
#include <stdio.h>
#include <string.h>
#include <math.h>

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
#define CURRENT_TIME_MS         ((Double)Clock_getTicks() * (Double)Clock_tickPeriod / 1000)

#define MPU_DATA_SPAN           10
#define SLIDING_MEAN_WINDOW     3
#define BUFFER_SIZE             80
#define MESSAGE_COUNT           10

/* Task stacks */
#define STACKSIZE_MPU_SENSOR_TASK       1500
#define STACKSIZE_LIGHT_SENSOR_TASK     1025
#define STACKSIZE_GESTURE_SENSOR_TASK   512
#define STACKSIZE_UART_WRITE_TASK       2000
#define STACKSIZE_UART_READ_TASK        512
#define STACKSIZE_SIGNAL_TASK           512
#define STACKSIZE_PLAY_BG_SONG_TASK     1024

Char mpuSensorTask_Stack[ STACKSIZE_MPU_SENSOR_TASK ];
Char lightSensorTask_Stack[ STACKSIZE_LIGHT_SENSOR_TASK ];
Char gestureAnalysisTask_Stack[ STACKSIZE_GESTURE_SENSOR_TASK ];
Char uartWriteTask_Stack[ STACKSIZE_UART_WRITE_TASK ];
Char uartReadTask_Stack[ STACKSIZE_UART_READ_TASK ];
Char signalTask_Stack[ STACKSIZE_SIGNAL_TASK ];
Char playBackgroundSongTask_Stack[ STACKSIZE_PLAY_BG_SONG_TASK ];

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

// Left button
static PIN_Handle buttonLeft_Handle;
static PIN_State buttonLeft_State;

PIN_Config buttonLeft_Config[] = {
    Board_BUTTON1  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_BOTHEDGES,
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
            BEEP_RECIEVED, PLAYING_BACKGROUND_MUSIC, NO_RESPONSE_RECIEVED };
// for some reason defaultStartState needs to be declared before programState for playSongInterruptible() to work
enum state defaultStartState = READING_MPU_DATA; 
enum state programState = READING_MPU_DATA;

enum gesture { NO_GESTURE=0, PETTING, PLAYING, SLEEPING, EATING, WALKING, CHEATING };
enum gesture currentGesture = NO_GESTURE;

enum message { NO_MESSAGE=0, TOO_FULL, LOW_HEALTH, DEATH, NO_PONG_RECIEVED, DEACTIVATED_SM, ACTIVATED_SM, DATA_UPLOADED, TETRIS, WON };
enum message currentMessage = NO_MESSAGE;


/* Global flags */
uint8_t uartInterruptFlag = 0;
uint8_t pongRecievedFlag = 1;
uint8_t lastCommWasBeepFlag = 1; // tells the program if it sent a command or recieved a beep last
uint8_t sendDataToBeVisualizedFlag = 0; 

uint8_t cheatsUsed = 0;
uint8_t eastereggStarted = 0;
uint8_t cheatingFlag = 0;

uint8_t rightButtonPressed = 0;
uint8_t leftButtonPressed = 0;


/* Global variables */
uint8_t uartBuffer[BUFFER_SIZE];
char printBuffer[127];
int16_t commandSentTime = 0;
double buttonRight_PressTime = 0.0;
int MPU_setup_complete = 0;
int command_sendTime = 0;

char backendMessage1[60];
char backendMessage2[60];
uint8_t newBackendMessage = 0; // set to 1 for message 1, and 2 for message 2

/* DATA */
float MPU_data[MPU_DATA_SPAN][7];
float MPU_data_buffer[SLIDING_MEAN_WINDOW][7];

/* INTERRUPT HANDLERS */

static void uartFxn(UART_Handle handle, void *rxBuf, size_t len) 
{
    uartInterruptFlag = 1;
    UART_read(handle, rxBuf, BUFFER_SIZE);
}

// Timeout to check if pong was recieved recieved. 
Void timeoutClock_Fxn(UArg arg0)
{
    if (pongRecievedFlag == 0) {
        currentMessage = NO_PONG_RECIEVED;
    }
    Clock_stop(timeoutClock_Handle);
}

// Right button. 
// Short press: Toggle SM
// Long press: Activate easter egg
Void buttonRight_Fxn(PIN_Handle handle, PIN_Id pinId)
{
    if (CURRENT_TIME_MS > 2000)
    {
        uint_t buttonValue = PIN_getInputValue( pinId );
        if (rightButtonPressed && buttonValue) { // button released
            rightButtonPressed = 0;
            
            if ((CURRENT_TIME_MS - buttonRight_PressTime) > 1000) 
            {
                eastereggStarted = 1;
                currentMessage = TETRIS;

            }
            else {
                System_printf("SM activation toggled from state: %d\n", programState);
                System_flush();

                if (programState != IDLE_STATE) {
                    programState = IDLE_STATE;
                    currentMessage = DEACTIVATED_SM;
                    PIN_setOutputValue(ledRed_Handle, Board_LED1, 1);

                }
                else {
                    programState = defaultStartState;
                    currentMessage = ACTIVATED_SM;
                    PIN_setOutputValue(ledRed_Handle, Board_LED1, 0);
                }
            }

        } else { // button pushed
            if ( (CURRENT_TIME_MS - buttonRight_PressTime) > 100 ) {
                rightButtonPressed = 1;
                buttonRight_PressTime = CURRENT_TIME_MS;
            }

        }
    }
}

// Left button. Interrupt will trigger on both press and release. 
Void buttonLeft_Fxn(PIN_Handle handle, PIN_Id pinId)
{
    uint_t buttonValue = PIN_getInputValue( pinId );
    if (buttonValue) { // button released
        // Nothing and somthing

    } else { // button pushed
        
        if (CURRENT_TIME_MS > 2000) {
            if (programState != IDLE_STATE) {
                sendDataToBeVisualizedFlag = 1;
            }
        }
        
    }
}


/* TASK FUNCTIONS */

/* UART Write Task 
 * This task is required to initialize UART Read.
 * 
 * Enter state: SENDING_MESSAGE_UART
 * Next state: SIGNALING_TO_USER
 * 
 * Will send command to backend corresponding to the currentGesture enum. Each message appended with ping.
 * After command sent, will start timeoutClock_Handle which triggers an interrupt to check if pong has been
 * recieved.
 * 
 * 2nd enter state: sendDataToBeVisualizedFlag set. 
 * This will start a session and send all data in MPU_data to backend to be visualized. 
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
        "ACTIVATE:5;5;5",
        "id:2231,MSG1:Health: ##--- 40%",
        "id:2231,MSG2:State 2 / Value 2.21",
        "ENERGY:3"
    };

    char session_start_msg[BUFFER_SIZE] = "session:start";
    char session_end_msg[BUFFER_SIZE] = "session:end";
    char msg1_front[] = "MSG1:";
    char msg2_front[] = "MSG2:";

    UART_Params_init(&uartParams);
    uartParams.baudRate      = 9600;
    uartParams.readMode      = UART_MODE_CALLBACK; 
    uartParams.readCallback  = &uartFxn; 
    uartParams.readDataMode  = UART_DATA_TEXT;
    uartParams.writeDataMode = UART_DATA_TEXT;

    uartHandle = UART_open(Board_UART, &uartParams);
    if (uartHandle == NULL) {
        System_abort("Error opening the UART");
    }

    UART_read(uartHandle, uartBuffer, BUFFER_SIZE);

    while (1) {

        if (programState == SENDING_MESSAGE_UART) {

            if (currentGesture == PETTING) {
                sprintf(uartMsg, "%s,%s,ping", tag_id, msg[1]);
            } else if(currentGesture == EATING){
                sprintf(uartMsg, "%s,%s,ping", tag_id, msg[2]);
            } else if(currentGesture == PLAYING){
                sprintf(uartMsg, "%s,%s,ping", tag_id, msg[3]);
            } else if (currentGesture == CHEATING) {
                sprintf(uartMsg, "%s,%s,ping", tag_id, msg[4]);
            }
            System_printf("Sending uart message: %s\n", uartMsg);
            System_flush();
            UART_write(uartHandle, uartMsg, sizeof(uartMsg));
            memset(uartMsg, '\0', BUFFER_SIZE);
            Clock_start(timeoutClock_Handle); // timeout clock for checking pong
            pongRecievedFlag = 0;
            command_sendTime = CURRENT_TIME_MS;

            if (programState != IDLE_STATE) {
                if (currentGesture != CHEATING) {
                    programState = SIGNALLING_TO_USER;
                } else {
                    programState = DETECTING_LIGHT_LEVEL;
                }
            }
        }

        else if (sendDataToBeVisualizedFlag == 1) { // Send data to backend to be visualized

            sprintf(uartMsg, "%s,%s", tag_id, session_start_msg);
            UART_write(uartHandle, uartMsg, sizeof(uartMsg));
            memset(uartMsg, '\0', BUFFER_SIZE);
            SLEEP(100);
            
            System_printf("Session started ...\n");
            System_flush();

            int i;
            for (i = 0; i < MPU_DATA_SPAN; i++){
                sprintf(uartMsg,"%s,time:%d,ax:%d,ay:%d,az:%d,gx:%d,gy:%d,gz:%d", tag_id, (int)(MPU_data[i][0]),  
                        (int)(MPU_data[i][1]*100), (int)(MPU_data[i][2]*100), (int)(MPU_data[i][3]*100),
                        (int)(MPU_data[i][4]*100), (int)(MPU_data[i][5]*100), (int)(MPU_data[i][6]*100) ); // acc with time data
                UART_write(uartHandle, uartMsg, sizeof(uartMsg));
                memset(uartMsg, '\0', BUFFER_SIZE);
                SLEEP(100);
            }
            
            sprintf(uartMsg, "%s,%s", tag_id, session_end_msg);
            UART_write(uartHandle, uartMsg, sizeof(uartMsg));
            memset(uartMsg, '\0', BUFFER_SIZE);

            System_printf("... session ended\n");
            System_flush();

            currentMessage = DATA_UPLOADED;
            sendDataToBeVisualizedFlag = 0;
        }

        else if (newBackendMessage == 1) {
            sprintf(uartMsg, "%s,%s%s,%s%s", tag_id, msg1_front, backendMessage1, msg2_front, backendMessage2);
            UART_write(uartHandle, uartMsg, sizeof(uartMsg));
            memset(uartMsg, '\0', BUFFER_SIZE);
            
            //System_printf("Backend message updated\n");
            //System_flush();

            newBackendMessage = 0;
        }
        SLEEP(100);
    }
}


/* UART Read Task.
 * Priority = 1
 * 
 * Doesn't actually initialize Uart or use Uart handle. Only checks to see if uartInterruptFlag has been set,
 * and then reads from global uartMsgRec string. 
 * 
 * If task reads a BEEP sent to this sensortag, will set currentMessage enum, which will run signalToUserTask
 * independent of the SM. 
 * 
 * Task will reset pong flag if pong message was recieved. 
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

        if (uartInterruptFlag == 1)
        {
            uartInterruptFlag = 0;

            strcpy(uartMsgRec, uartBuffer);
            memset(uartBuffer, 0, BUFFER_SIZE);

            //System_printf("Revieced UART message: %s\n", uartMsgRec);
            System_flush();

            if (stringContainsAt(uartMsgRec, beep_msg, 0)) { // BEEP RECIEVED

                int lastComm_timeout = 500;

                if ( ( (CURRENT_TIME_MS - command_sendTime) > lastComm_timeout ) && (
                    stringContainsAt(uartMsgRec, low_health_msg1, msg_start_index) ||
                    stringContainsAt(uartMsgRec, low_health_msg2, msg_start_index) ||
                    stringContainsAt(uartMsgRec, low_health_msg3, msg_start_index)
                    ) ) {
                    currentMessage = LOW_HEALTH;
                }
                else if ( ( (CURRENT_TIME_MS - command_sendTime) < lastComm_timeout ) && (
                    stringContainsAt(uartMsgRec, too_full_msg1, msg_start_index) ||
                    stringContainsAt(uartMsgRec, too_full_msg2, msg_start_index) ||
                    stringContainsAt(uartMsgRec, too_full_msg3, msg_start_index) 
                    ) ) {
                    currentMessage = TOO_FULL;
                }

                else if (stringContainsAt(uartMsgRec, death_msg, msg_start_index))
                {
                    currentMessage = DEATH;
                }
            
            } else if (stringContainsAt(uartMsgRec, pong_msg, 0)) { // PONG RECIEVED

                if (pongRecievedFlag == 0) {
                    pongRecievedFlag = 1;
                }
            }
        }
    } // END OF WHILE LOOP
}


/* Light Sensor Task
 * 
 * Enter state: DETECTING_LIGHT_LEVEL  or  PLAYING_BACKGROUND_MUSIC
 * Next state: READING_MPU_DATA  or  PLAYING_BACKGROUND_MUSIC
 * 
 * Task will only read light sensor after period of time, defined by lightSensor_timeout. Will change
 * to next state regardless. 
 * 
 * IMPORTANT: Task requires that MPU sensor task be run, else I2C will not be setup (to avoid conflicts)
 */
Void lightSensorTask_Fxn(UArg arg0, UArg arg1)
{
    double lux;
    double Sleep_Light_Threshold = 5;
    uint8_t lightLevelBars = 0;
    uint8_t lightLevelBarsPrev = 255;
    uint8_t lastSleepState = 0;
    int lightSensor_lastCalled = 0;
    int lightSensor_timeout = 1000;

    double pressureDiffThreshold = 200;
    double averagePressure;
    double pressureThreshold;
    double pressure;
    double temp_comp;
    int pressureCounter = 0; // for debugging purposes

    I2C_Handle      i2c;
    I2C_Params      i2cParams;

    while (MPU_setup_complete == 0) {
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

    System_printf("BMP280: Setting up I2C ...\n");
    System_flush();

    SLEEP(100);
    bmp280_setup(&i2c);

    /* Calculate average pressure */
    int i;
    for (i = 0; i < 1; i++) {
        bmp280_get_data(&i2c, &pressure, &temp_comp); // first reading(s) is/are bad for some reason
        SLEEP(100);
    }
    for (i = 0; i < 5; i++) {
        bmp280_get_data(&i2c, &pressure, &temp_comp);
        averagePressure += pressure;
        SLEEP(100);
        //sprintf(printBuffer, "(Finding average pressure) Pressure level: %d\n", (int)pressure);
        //System_printf(printBuffer);
        //System_flush();
    }
    averagePressure = averagePressure / i;
    pressureThreshold = averagePressure + pressureDiffThreshold;

    System_printf("BMP280: Setup OK and average pressure found\n");
    System_flush();
    I2C_close(i2c);

    while (1) {

        /* LIGHT LEVEL SENSING */
         if (programState == DETECTING_LIGHT_LEVEL || programState == PLAYING_BACKGROUND_MUSIC) {

            if ((CURRENT_TIME_MS - lightSensor_lastCalled) > lightSensor_timeout) // only actually sense light level every 1000 ms
            {
                i2c = I2C_open(Board_I2C_TMP, &i2cParams);
                if (i2c == NULL) {
                    System_abort("OPT3001: Error Initializing I2C\n");
                }
                int i;
                for (i = 0; i < 10; i++){
                    lux = opt3001_get_data(&i2c);
                    if (lux != -1) break;
                }
                I2C_close(i2c);

                // updating light level indicator
                lightLevelBars = (uint8_t) (2*(8.5 * pow((lux), (0.15)) - 9) - 1);
                if (lightLevelBars > 100) lightLevelBars = 0;
                
                if (lightLevelBars != lightLevelBarsPrev) {
                    createLightLevelBar(&backendMessage2, lightLevelBars);
                    lightLevelBarsPrev = lightLevelBars;
                    newBackendMessage = 1;
                }

                //sprintf(printBuffer, "Bars: %d Light level: %.2f\n", lightLevelBars, lux);
                //System_printf(printBuffer);
                //System_flush();
                //if (newBackendMessage == 1) {
                //    sprintf(printBuffer, "msg to backend: %s\n", backendMessage2);
                //    System_printf(printBuffer);
                //    System_flush();
                //}

                lightSensor_lastCalled = CURRENT_TIME_MS;
            }

            if (programState != IDLE_STATE) {
                if ( lux < Sleep_Light_Threshold ) {
                    programState = PLAYING_BACKGROUND_MUSIC;
                    if (lastSleepState == 1) {
                        newBackendMessage = 1;
                        sprintf(backendMessage1, "zzzzzzzzzzzzz");
                        lastSleepState = 0;
                    }
                } else {
                    programState = READING_MPU_DATA;
                    if (lastSleepState == 0) {
                        newBackendMessage = 1;
                        sprintf(backendMessage1, "I'm awake!");
                        lastSleepState = 1;
                    }
                }
            }
        }

        /* PRESSURE SENSING */
        if (eastereggStarted == 1 && programState != PLAYING_BACKGROUND_MUSIC) { 
        
            i2c = I2C_open(Board_I2C_TMP, &i2cParams);
            if (i2c == NULL) {
                System_abort("BMP280: Error Initializing I2C\n");
            }
        
            bmp280_get_data(&i2c, &pressure, &temp_comp);

            I2C_close(i2c);
            SLEEP(100);

            if (pressure > pressureThreshold) {
                if (cheatsUsed >= 1) {
                    eastereggStarted = 0;
                    cheatsUsed = 0;
                }
                currentMessage = WON;
                cheatingFlag = 1;
            }

            pressureCounter++;
            if (pressureCounter >= 15) {
                pressureCounter = 0;
                //sprintf(printBuffer, "Pressure level: %d, average pressure: %d, diff: %d\n", (int)pressure, (int)averagePressure, (int)(pressureThreshold-pressure));
                //System_printf(printBuffer);
                //System_flush();
            }
        }
        SLEEP(50);
    }
}


/* MPU Sensor Task
 * 
 * Enter state: READING_MPU_DATA
 * Next state: ANALYSING_DATA
 * 
 * Task will read mpu data and add it to global MPU_data array. Task frequency is also monitored and
 * printed to console after certain number of calls. 
 */
Void mpuSensorTask_Fxn(UArg arg0, UArg arg1)
{
    float time, ax, ay, az, gx, gy, gz;

    int mpu_pc = 0; // debug purposes
    int mpu_lastPrinted = 0;

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

            i2cMPU = I2C_open(Board_I2C0, &i2cMPUParams);
            if (i2cMPU == NULL) {
                System_abort(" MPU: Error Initializing I2C\n");
            }
            mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
            time = CURRENT_TIME_MS;
            I2C_close(i2cMPU);

            //addData(MPU_data_buffer, SLIDING_MEAN_WINDOW, new_data);
            //getAverage(MPU_data_buffer, new_mean_data);
            //new_mean_data[0] = time;

            addMpuData(MPU_data, MPU_DATA_SPAN, time, 10*ax, 10*ay, 10*az, gx, gy, gz);

            if (programState != IDLE_STATE) programState = ANALYSING_DATA;

            // Debug
            mpu_pc++;
            if (mpu_pc >= 50){
                float freq = (float)mpu_pc * 1000 / (CURRENT_TIME_MS - mpu_lastPrinted);
                mpu_lastPrinted = CURRENT_TIME_MS;

                //sprintf(printBuffer, "MPU freq: %.1f Hz\n", freq);
                //System_printf(printBuffer);
                //System_flush();
                //printMpuData(MPU_data, MPU_DATA_SPAN);
                mpu_pc = 0;
            }
        }
        SLEEP(100);
    }
}


/* Gesture Analysis Task
 * 
 * Enter state: ANALYSING_DATA
 * Next state: SENDING_MESSAGE_UART or DETECTING_LIGHT_LEVEL
 * 
 * State will 'analyse' MPU_data for gestures. Currently only looks at 0 row. If gesture is detected, will
 * change currentGesture enum to relevant gesture.
 */
Void gestureAnalysisTask_Fxn(UArg arg0, UArg arg1)
{
    //float variance[7], mean[7], max[7], min[7];

    while (1) {
        if ( programState == ANALYSING_DATA ) {

            ////System_printf("In state: gestureAnalysis\n");
            //System_flush();

            //analyseData(MPU_data, variance, mean, max, min);
            //printMpuData(MPU_data, 1);

            if (isPetting(MPU_data, MPU_DATA_SPAN)) { //isPetting(MPU_data, MPU_DATA_SPAN))
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
                
            } else if(cheatingFlag == 1){
                System_printf("You Cheated!\n");
                System_flush();
                currentGesture = CHEATING;
                cheatingFlag = 0;
               
            }
            else {
                currentGesture = NO_GESTURE;
            }

            // change state
            if (programState != IDLE_STATE)
            {
                if (currentGesture != NO_GESTURE) {
                    programState = SENDING_MESSAGE_UART;

                } else {
                    programState = DETECTING_LIGHT_LEVEL;
                }
            }
        }
        SLEEP(100);
    }
}


/* Signal To User Task
 * 
 * Enter state: SIGNALLING_TO_USER or currentMessage is not NO_MESSAGE
 * Next state: DETECTING_LIGHT_LEVEL
 * 
 * Task will signal to the user via the buzzer of certain things. Signal will occur if:
 * - Gesture has been detected
 * - No pong was recieved after command send (no connection)
 * - State machine activated/deactivated
 * - Backend has sent BEEP to tell of low health, high health, or death. 
 * - Data upload session has finished. 
 * 
 * Task will set currentMessage to NO_MESSAGE after. 
 * 
 */
Void signalTask_Fxn(UArg arg0, UArg arg1)
{

    while (1) {
        if (programState == SIGNALLING_TO_USER || (currentMessage != NO_MESSAGE) ) { 

            if (programState == SIGNALLING_TO_USER) {
                //System_printf("Signaling to user with buzzer!\n");
                //System_flush();
                playSong(buzzerHandle, gesture_detected_signal);
            }

            if (currentMessage == DEACTIVATED_SM) {
                playSong(buzzerHandle, deactivate_signal);
            } else if (currentMessage == ACTIVATED_SM) {
                playSong(buzzerHandle, activate_signal);
            }
            
            if ( (programState != IDLE_STATE) && (programState != PLAYING_BACKGROUND_MUSIC) ) { // prevent signalling when SM deactivated
                if (currentMessage == LOW_HEALTH) {
                    playSong(buzzerHandle, low_health_signal);

                } else if (currentMessage == NO_PONG_RECIEVED) {
                    playSong(buzzerHandle, message_not_recieved_signal);
                
                } else if (currentMessage == DEATH) {
                    playSong(buzzerHandle, requiem);
                
                } else if (currentMessage == TOO_FULL) {
                    playSong(buzzerHandle, too_full_signal);
                
                } else if (currentMessage == DATA_UPLOADED) {
                    playSong(buzzerHandle, data_uploaded_signal);

                } else if (currentMessage == TETRIS) {
                    System_printf("Playing Tetris!\n");
                    System_flush();
                    programState = IDLE_STATE;
                    playSong(buzzerHandle, tetris_theme_song);
                    programState = defaultStartState;
                }
                else if (currentMessage == WON) {
                    programState = IDLE_STATE;
                    playSong(buzzerHandle, winning_signal);
                    cheatsUsed = cheatsUsed + 1;
                    programState = defaultStartState;
                }
            }
            currentMessage = NO_MESSAGE;

            if (programState != IDLE_STATE) programState = DETECTING_LIGHT_LEVEL;
        }
        SLEEP(100);
    }
}


/* Play Background Song Task
 * 
 * Enter state: PLAYING_BACKGROUND_MUSIC
 * 
 * Task will simply call playSongInterruptible() function and give it pointer to progam state and
 * continue state. Function will stop playing song if programState changes from continue state. 
 * 
 */
Void playBackgroundSongTask_Fxn(UArg arg0, UArg arg1)
{
    // initialization testing

    while (1) {
        if (programState == PLAYING_BACKGROUND_MUSIC) {

            System_printf("Playing lullaby ... zzzz\n");
            System_flush();

            // execute state function
            playSongInterruptible(buzzerHandle, lullaby, &programState, PLAYING_BACKGROUND_MUSIC);
            //playSong(buzzerHandle, lullaby);
            
            if (programState != IDLE_STATE) programState = READING_MPU_DATA;
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
    mpuSensorTask_Params.stackSize = STACKSIZE_MPU_SENSOR_TASK;
    mpuSensorTask_Params.stack = &mpuSensorTask_Stack;
    mpuSensorTask_Params.priority=2;
    mpuSensorTask_Handle = Task_create(mpuSensorTask_Fxn, &mpuSensorTask_Params, NULL);
    if (mpuSensorTask_Handle == NULL) {
        System_abort("mpuSensorTask create failed!");
    }
    
    Task_Params_init(&lightSensorTask_Params);
    lightSensorTask_Params.stackSize = STACKSIZE_LIGHT_SENSOR_TASK;
    lightSensorTask_Params.stack = &lightSensorTask_Stack;
    lightSensorTask_Params.priority=2;
    lightSensorTask_Handle = Task_create(lightSensorTask_Fxn, &lightSensorTask_Params, NULL);
    if (lightSensorTask_Handle == NULL) {
        System_abort("lightSensorTask create failed!");
    }

    Task_Params_init(&gestureAnalysisTask_Params);
    gestureAnalysisTask_Params.stackSize = STACKSIZE_GESTURE_SENSOR_TASK;
    gestureAnalysisTask_Params.stack = &gestureAnalysisTask_Stack;
    gestureAnalysisTask_Params.priority=2;
    gestureAnalysisTask_Handle = Task_create(gestureAnalysisTask_Fxn, &gestureAnalysisTask_Params, NULL);
    if (gestureAnalysisTask_Handle == NULL) {
        System_abort("gestureAnalysisTask create failed!");
    }
    
    Task_Params_init(&uartWriteTask_Params);
    uartWriteTask_Params.stackSize = STACKSIZE_UART_WRITE_TASK;
    uartWriteTask_Params.stack = &uartWriteTask_Stack;
    uartWriteTask_Params.priority=2;
    uartWriteTask_Handle = Task_create(uartWriteTask_Fxn, &uartWriteTask_Params, NULL);
    if (uartWriteTask_Handle == NULL) {
        System_abort("uartWriteTask create failed!");
    }

    Task_Params_init(&uartReadTask_Params);
    uartReadTask_Params.stackSize = STACKSIZE_UART_READ_TASK;
    uartReadTask_Params.stack = &uartReadTask_Stack;
    uartReadTask_Params.priority=1;
    uartReadTask_Handle = Task_create(uartReadTask_Fxn, &uartReadTask_Params, NULL);
    if (uartReadTask_Handle == NULL) {
        System_abort("uartReadTask create failed!");
    }
    Task_Params_init(&signalTask_Params);
    signalTask_Params.stackSize = STACKSIZE_SIGNAL_TASK;
    signalTask_Params.stack = &signalTask_Stack;
    signalTask_Params.priority=2;
    signalTask_Handle = Task_create(signalTask_Fxn, &signalTask_Params, NULL);
    if (signalTask_Handle == NULL) {
        System_abort("signalTask create failed!");
    }

    
    Task_Params_init(&playBackgroundSongTask_Params);
    playBackgroundSongTask_Params.stackSize = STACKSIZE_PLAY_BG_SONG_TASK;
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

    buttonLeft_Handle = PIN_open( &buttonLeft_State, buttonLeft_Config );
    if(!buttonLeft_Handle) {
        System_abort("Error initializing button 0\n");
    }
    if (PIN_registerIntCb( buttonLeft_Handle, &buttonLeft_Fxn ) != 0) {
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

