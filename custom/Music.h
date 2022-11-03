/* 
 * Library by Matt Stirling
 * For making playing musical notes easier on the Sensor Tag buzzer
*/ 


#ifndef MUSIC_H_
#define MUSIC_H_

/* Define musical notes */
#define     NOTE_A          0 << 12
#define     NOTE_AS         1 << 12
#define     NOTE_B          2 << 12
#define     NOTE_C          3 << 12
#define     NOTE_CS         4 << 12
#define     NOTE_D          5 << 12
#define     NOTE_DS         6 << 12
#define     NOTE_E          7 << 12
#define     NOTE_F          8 << 12
#define     NOTE_FS         9 << 12
#define     NOTE_G          10 << 12
#define     NOTE_GS         11 << 12

#define     REST            15 << 12

/* Define note lengths */
#define     WHOLE_NOTE          0 << 6
#define     HALF_NOTE           1 << 6
#define     QUARTER_NOTE        2 << 6
#define     EIGHTHS_NOTE        3 << 6
#define     SIXTEENTHS_NOTE     4 << 6
#define     THIRTYTWO_NOTE      5 << 6

#define     DOT_NOTE            1 << 5
#define     SLIDE_NOTE          1 << 4  // must define octave explicitly for next note

/* Octaves */
#define     OCT_1               1 << 9
#define     OCT_2               2 << 9
#define     OCT_3               2 << 9
#define     OCT_4               3 << 9
#define     OCT_5               4 << 9
#define     OCT_6               5 << 9
#define     OCT_7               6 << 9
#define     OCT_8               7 << 9


extern int interruptSongFlag;

/* Functions for 'easy' playing */
void playNote(char *note, float length);
void playNoteSt(char *note, float length, float pause);
void _playNote(int i, int oct, float length, float pause);
void _playSlideNote(int i, int i_next, int oct, int oct_next, float length, float pause);
void extractNoteFromString(char *note, int *i, int *oct);

/* Dealing with notes as bits, better for playing songs */
int playSong(PIN_Handle buzzerHandle, const uint16_t song[]);
void parseNoteBits(uint16_t bits, int *i, int *oct, float *length, float *pause, int *slideFlag);

/* Helper functions */
int getNoteFrequency(const int i, const int oct);
void setBMP(int bmp);
int getBMP();

#endif
