#include <stdio.h>
#include <inttypes.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include "utilities.h"

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
