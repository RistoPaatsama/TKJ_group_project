#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include "gestures.h"

int isPetting(float *ay, float *ax, float *az, uint8_t span)
{
    float ay_threshold = 0.4;
    float ax_threshold = 0.1;
    float az_threshold = 0.9;
    int i;

    for (i = 0; i < span; i++)
    {
        if (fabs(ay[i]) > ay_threshold && fabs(ax[i]) < ax_threshold && fabs(az[i]) > az_threshold)
        {
            return 1;
        }
    }
    return 0;
}
