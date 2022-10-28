/*
 * Library by: Matt Stirling
 * Created on: 27.10.2022
 *
 *
 */

#ifndef DATAHANDLING_H_
#define DATAHANDLING_H_

void addMpuData(float data[][7], const int span, float time, float ax, float ay, float az, float gx, float gy, float gz);
void setZeroMpuData(float data[][7], const int span);
void printMpuData(float data[][7], const int span);

#endif
