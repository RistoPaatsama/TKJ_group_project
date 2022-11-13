#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include "gestures.h"

int isPetting(float data[][7], int span)
{
    float ay_threshold = 5;
    float ax_threshold = 0.5;
    float az_threshold = 0.5;
    int i;

    for (i = 0; i < 1; i++)
    {
        if ( fabs(data[i][2]) > ay_threshold ) {
        //if ( ( fabs(data[i][1]) < ax_threshold ) && ( fabs(data[i][2]) > ay_threshold ) && ( fabs(data[i][3]-10) > az_threshold ) ) {
            return 1;
        }
    }
    return 0;
}
