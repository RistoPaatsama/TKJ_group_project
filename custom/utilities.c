#include <stdio.h>
#include <inttypes.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include "Utilities.h"

// calculates moving average for array
void movavg(float *array, uint8_t array_size, uint8_t window_size)
{
    float avg;
    int i, j;
    for (i = 0; i < array_size - window_size + 1; i++)
    {
        for (j = i; j < window_size + i; j++)
        {
            avg += array[j];
        }
        avg /= window_size;
        array[i] = avg;
        avg = 0;
    }
    // unable to calculate rolling mean for these, therefore assign 0.00
    for (i = array_size; i > array_size - window_size; i--)
    {
        array[i] = 0.00;
    }
}

void intoArray(float data[][7], float *array, uint8_t col, uint8_t span)
{
    int i;
    for (i = 0; i < span; i++)
    {
        array[i] = data[i][col];
    }
}

void printArray(float *array, uint8_t span)
{
    int i;
    char buffer[32];

    System_printf("printing array values\n");
    System_flush();

    for(i = 0; i < span; i++)
    {
        sprintf(buffer, "%.2f,", array[i]);
        System_printf(buffer);
        System_flush();
    }
    System_printf("\n");
    System_flush();
}

/* Will return 1 if str contains the string sub starting at index i. 
 * 
 * Function by Matti
*/ 
int stringContainsAt(char *str, char *sub, int i)
{
    char *ptr1 = str;
    char *ptr2 = sub;
    int j;

    // move pointer 1 to the i:th character of str
    for (j = 0; j < i; j++){
        ptr1++;
        if (*ptr1 == '\0'){
            return 0;
        }
    }

    while (*ptr2 != '\0'){
        if ( *ptr1++ != *ptr2++ ) {
            return 0;
        }
    }

    return 1;
}

/* 
 * Will create a "graphical" light level bar to be shown as backend message.
 * bars go from 0 to 20. Will also show percentage after bar. 
*/
void createLightLevelBar(char *str, uint8_t bars) {
    char *ptr = str;
    char front[] = "Light level:  ";
    char back[6];
    int i, perc;
    
    for (i = 0; i < strlen(front); i++){
        *ptr = front[i];
        if (*ptr != '\0') ptr++;
    }
    for (i = 0; i < 20; i++) {
        if (i < bars) {
            *ptr++ = '#';
        } else {
            *ptr++ = '_';
        }
    }
    *ptr++ = ' ';
    *ptr++ = ' ';
    *ptr++ = ' ';
    perc = bars * 5;
    if (perc > 100) perc = 100;

    sprintf(back, "%d%%", perc);

    for (i = 0; i < strlen(back); i++) {
        *ptr++ = back[i];
    }
    *ptr = '\0';
}
