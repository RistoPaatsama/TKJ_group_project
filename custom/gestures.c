#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include "gestures.h"

int isPetting(float data[][7], int span)
{
    enum index { ax=1, ay, az, gx, gy, gz };

    float ay_threshold = 5;
    float az_threshold = 9;
    float gx_threshold = 5;
    int i = 0;

    if (fabs(data[i][ay]) > ay_threshold && fabs(data[i][gx]) < gx_threshold && fabs(data[i][az]) > az_threshold) {
        return 1;
    }
    return 0;
}

int isEating(float data[][7], int span)
{
    enum index { ax=1, ay, az, gx, gy, gz };

    float dif_ay_az_threshold = 3.5;
    float gx_threshold = 60;
    int i = 0;

    if (fabs(data[i][ay] - data[i][az]) < dif_ay_az_threshold && data[i][gx] > gx_threshold) {
        return 1;
    }
    return 0;
}

int isPlaying(float data[][7], int span)
{
    enum index { ax=1, ay, az, gx, gy, gz };

    float dif_ay_az_threshold = 4;
    float ay_threshold = 2;
    int i = 0;

    if (fabs(data[i][ay] - data[i][az]) < dif_ay_az_threshold && fabs(data[i][ay] < ay_threshold)) {
        return 1;
    }
    return 0;
}

