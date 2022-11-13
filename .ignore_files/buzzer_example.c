#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>

/* Board Header files */
#include "Board.h"
#include "custom/Music.h"
#include "custom/Songs.h"

#define SLEEP(ms) Task_sleep((ms)*1000 / Clock_tickPeriod)

/* Task */
#define STACKSIZE 2048
Char taskStack[STACKSIZE];

static PIN_Handle hBuzzer;
static PIN_State sBuzzer;
PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};

static PIN_Handle buttonHandle;
static PIN_State buttonState;
PIN_Config buttonConfig[] = {
    Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

int playingMusicFlag = 1;
volatile int interruptFlag = 0;



void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    interruptFlag = 1;
    interruptSongFlag = 1;
}

Void taskFxn(UArg arg0, UArg arg1)
{
    char buffer[64];

    while (1) {

        if (interruptFlag) {
            interruptFlag = 0;
            playingMusicFlag = !playingMusicFlag;
            sprintf(buffer, "Flipping play music flag to %d\n", playingMusicFlag);
            System_printf(buffer);
            System_flush();
        }

        if (playingMusicFlag)
        {

            int i;
            uint16_t jingles[] = {gesture_detected, message_not_recieved, gesture_detected, too_much, activate, deactivate, low_health};
            //uint16_t jingles[] = {lullaby};

            for (i = 0; i < sizeof(jingles) / sizeof(jingles[0]); i++)
            {
                playSong(hBuzzer, jingles[i]);
                SLEEP(1500);
                if (interruptFlag){
                    break;
                }
            }
        }

        SLEEP(1000);
    }
}

Int main(void) {

      Task_Handle task;
      Task_Params taskParams;

      // Initialize board
      Board_initGeneral();

      // Buzzer
      hBuzzer = PIN_open(&sBuzzer, cBuzzer);
      if (hBuzzer == NULL) {
          System_abort("Pin open failed!");
      }

      buttonHandle = PIN_open(&buttonState, buttonConfig);
      if (buttonHandle == NULL){
          System_abort("Button  open failed!");
      }
      if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0){
          System_abort("Error registering button callback function!");
      }

      Task_Params_init(&taskParams);
      taskParams.stackSize = STACKSIZE;
      taskParams.stack = &taskStack;
      task = Task_create((Task_FuncPtr)taskFxn, &taskParams, NULL);
      if (task == NULL) {
          System_abort("Task create failed!");
      }

      /* Sanity check */
      System_printf("Beeps!\n");
      System_flush();

      /* Start BIOS */
      BIOS_start();

      return (0);
}


