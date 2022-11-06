/*
 * Library by: Risto Paatsama, Matt Stirling, Khalil Chakal
 * Created on: 05.11.2022
 *
 * ALL KINDS OF UTILITY FUNCTIONS BELONG IN HERE
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

void movavg(float *array, uint8_t array_size, uint8_t window_size);
void intoArray(float data[][], float *array, uint8_t col, uint8_t span);
void printArray(float *array, uint8_t span);

#endif
