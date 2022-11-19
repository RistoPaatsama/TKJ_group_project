#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>

#include "Board.h"
#include "buzzer.h"
#include "Music.h"

static int BMP = 120;
int interruptSongFlag = 0;


// for playing songs with notes defined as uint16_t arrays
int playSong(PIN_Handle buzzerHandle, const uint16_t song[])
{
    int songSize = song[0] & 0x00ff;
    int songBPM = (song[0] & 0xff00) >> 8;
    songBPM *= 4;

    buzzerOpen(buzzerHandle);
    setBMP(songBPM);

    int j, i, oct, octHold=0;
    float length, pause;

    int slideFlag, i_next, oct_next;
    float len_next, pause_next; // useless, just to pass into function

    for (j = 1; j <= songSize; j++)
    {
        parseNoteBits(song[j], &i, &oct, &length, &pause, &slideFlag);

        if ( (oct == 0) && (octHold != 0) ) oct = octHold; // hold prev oct so you don't need it for every note
        else if (oct != -1)                 octHold = oct;

        if (slideFlag == 0){
            _playNote(i, oct, length, pause);

        } else { // sliding note
            parseNoteBits(song[j+1], &i_next, &oct_next, &len_next, &pause_next, &slideFlag);
            _playSlideNote(i, i_next, oct, oct_next, length, pause);
        }


        if (interruptSongFlag){
            buzzerClose();
            interruptSongFlag = 0;
            return -1;
        }
    }
    buzzerClose();
    return 0;
}


int playSongInterruptible(PIN_Handle buzzerHandle, const uint16_t song[], int *state, int playCondition)
{
    while (*state == playCondition) {
        System_printf("LULLABY PLAYING!!!\n");
        System_flush();
    }
    return 0;
}


// plays notes with automatic small pause at end
void playNote(char *note, float length){
    playNoteSt(note, length, 0.05);
}


// play note with note given as string (st stands for staccato)
void playNoteSt(char *note, float length, float pause)
{
    int i, oct;
    extractNoteFromString(note, &i, &oct);
    _playNote(i, oct, length, pause);
}


// for finally playing the damn note
void _playNote(int i, int oct, float length, float pause)
{
    int freq;
    float duration, play_ms, pause_ms;

    duration = length * ( 1000 * 60 / (float)BMP ); // length of 1 notes in ms
    play_ms = (duration*(1-pause));
    pause_ms = (duration*(pause));

    if (i == 15){ // rest
        buzzerSetFrequency(0);
        Task_sleep((duration)*1000 / Clock_tickPeriod);
        return;
    }
    freq = getNoteFrequency(i, oct);


    buzzerSetFrequency(freq);
    Task_sleep(play_ms * 1000 / Clock_tickPeriod);
    if (pause_ms > 0) buzzerSetFrequency(0);
    Task_sleep(pause_ms * 1000 / Clock_tickPeriod);
}



/* For playing a sliding note between two pitches */
void _playSlideNote(int i, int i_next, int oct, int oct_next, float length, float pause)
{
    int freq_start, freq_end, freq_inc, curr_freq;
    float duration, play_ms, pause_ms;

    int inc_ms = 25, j;

    duration = length * ( 1000 * 60 / (float)BMP ); // length of 1 notes in ms
    //play_ms = (duration*(1-pause));
    //pause_ms = (duration*(pause));

    if (i == 15){ // rest
        buzzerSetFrequency(0);
        Task_sleep((duration)*1000 / Clock_tickPeriod);
        return;
    }

    freq_start = getNoteFrequency(i, oct);
    freq_end = getNoteFrequency(i_next, oct_next);
    curr_freq = freq_start;
    freq_inc = ( freq_end - freq_start ) / ( duration / inc_ms );

    for (j = 0; j < duration; j += inc_ms)
    {
        buzzerSetFrequency(curr_freq);
        Task_sleep(inc_ms * 1000 / Clock_tickPeriod);
        curr_freq += freq_inc;
    }
}


// Used to extract "octave" and "note in octave" information from string "note"
void extractNoteFromString(char *note, int *i, int *oct)
{
    if (note[0] == 'P') {
        *i = 15;
        return;
    }
    if (note[0] == 'A')     *i = 0;
    if (note[0] == 'B')     *i = 2;
    if (note[0] == 'C')     *i = 3;
    if (note[0] == 'D')     *i = 5;
    if (note[0] == 'E')     *i = 7;
    if (note[0] == 'F')     *i = 8;
    if (note[0] == 'G')     *i = 10;

    if (note[1] == '#'){
        *i += 1;
        *oct = note[2] - 48;
    } else {
        *oct = note[1] - 48;
    }
}


/*
 * ParseNoteBits() function
 * For extracting i, oct, len and pause information from uint16_t number
 *
 * uint16_t -> 0b0000000000000000
 * (from right to left) first 4 bits is note (15 means pause)
 * next 3 bits define octave
 * next 3 bits define length of note
 * final 6 bits define proportion of note duration to rest
 */
void parseNoteBits(uint16_t bits, int *i, int *oct, float *length, float *pause, int *slideFlag)
{
    uint8_t note_bits =     (bits & 0xf000) >> 12;
    uint8_t oct_bits =      (bits & 0b111000000000) >> 9;
    uint8_t length_bits =   (bits & 0b111000000) >> 6;
    uint8_t pause_bits =    (bits & 0b1111);
    uint8_t dot_bit =       (bits & 0b100000) >> 5;
    uint8_t slide_bit =    (bits & 0b10000) >> 4;

    *i = note_bits;
    *oct = oct_bits;
    *length = 4 / pow(2, length_bits) * (1 + 0.5*dot_bit);
    *pause = (float)pause_bits / 15;
    *slideFlag = slide_bit;
}


/* HELPER FUNCTIONS */

// Returns notes frequency given i and oct
int getNoteFrequency(const int i, const int oct)
{
    int octave = oct;

    if (oct == -1) return 0;

    if (i > 2) octave--;

    return 27.5 * pow(2, octave) * powf(2,((float)i/12));
}

void setBMP(int bmp){
    BMP = bmp;
}

int getBMP(){
    return BMP;
}





