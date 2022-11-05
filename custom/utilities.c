#include <stdio.h>
#include <inttypes.h>

#include "utilities.h"

// calculates moving average for array
void movavg(float *array, uint8_t array_size, uint8_t window_size)
{
    float avg;
    for (int i = 0; i < array_size - window_size + 1; i++)
    {
        for (int j = i; j < window_size + i; j++)
        {
            avg += array[j];
        }
        avg /= window_size;
        array[i] = avg;
        avg = 0;
    }
    // unable to calculate rolling mean for these, therefore assign 0.00
    for (int i = array_size; i > array_size - window_size; i--)
    {
        array[i] = 0.00;
    }
}
