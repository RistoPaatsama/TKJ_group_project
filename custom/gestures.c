#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include "gestures.h"

int isPetting(float data[][7], int span)
{
    enum index { ax=1, ay, az, gx, gy, gz };

    float ay_threshold = 5;
    float az_threshold = 9.8;
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

    float ay_threshold = 2.0;
    float az_threshold = 8.0;
    float gx_threshold_high = 85;
    float gx_threshold_low = 0;
    int i = 0;

    if (fabs(data[i][ay] - data[i][az]) < 3.5 && data[i][gx] > 60) {
        return 1;
    }
    return 0;
}

int isPlaying(float data[][7], int span)
{
    enum index { ax=1, ay, az, gx, gy, gz };

    float az_threshold_lower = 8;
    float az_threshold_higher = 9;
    float gx_threshold = 50;
    int i = 0;

    if (fabs(data[i][ay] - data[i][az]) < 4 && fabs(data[i][ay] < 2)) {
        return 1;
    }
    return 0;
}

