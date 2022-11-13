    #include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include "DataHandling.h"

// shuffles all data in mpu data matrix and adds new data into first row
void addMpuData(float data[][7], const int span, float time, float ax, float ay, float az, float gx, float gy, float gz)
{
    int i, j;
    float newRow[] = {time, ax, ay, az, gx, gy, gz};

    for (j = span-1; j >= 1; j--)
    {
        for (i = 0; i < 7; i++)
        {
            data[j][i] = data[j-1][i];
        }
    }
    for (i = 0; i < 7; i++)
    {
        data[0][i] = newRow[i];
    }
}


void printMpuData(float d[][7], const int span)
{
    int i;
    char buffer[64];

    if (span > 1){
        System_printf("\nPrinting MPU data:\n\n");
        System_printf("time,ax,ay,az,gx,gy,gz\n");
        System_flush();
    }

    for (i = span-1; i >= 0; i--)
    {
        sprintf(buffer, "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n", d[i][0], d[i][1], d[i][2], d[i][3], d[i][4], d[i][5], d[i][6]);
        System_printf(buffer);
        System_flush();
    }
}


void setZeroMpuData(float data[][7], const int span)
{
    int i, j;
    for (j = 0; j < span; j++)
    {
        for (i = 0; i < 7; i++)
        {
            data[j][i] = 0.0;
        }
    }
}
