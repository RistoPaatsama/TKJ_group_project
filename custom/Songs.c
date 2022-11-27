#include <stdlib.h>
#include <inttypes.h>

// if this is not included, Music.h will throw an error, "PIN_Handle is undefined" ??????????????????? WHAT
#include <ti/drivers/PIN.h>

#include "Music.h"
#include "Songs.h"


const uint16_t message_not_recieved_signal[] = {
                            (500 / 4) << 8 | 3,
                            NOTE_G  | OCT_6 | QUARTER_NOTE,
                            NOTE_E          | QUARTER_NOTE,
                            NOTE_AS | OCT_5 | QUARTER_NOTE  | DOT_NOTE
};


const uint16_t gesture_detected_signal[] = {
                            (550 / 4) << 8 | 5,
                            NOTE_C  | OCT_6     | QUARTER_NOTE,
                            NOTE_E              | QUARTER_NOTE,
                            NOTE_G              | QUARTER_NOTE,
                            NOTE_C  | OCT_7     | QUARTER_NOTE  | DOT_NOTE,
                            REST                | QUARTER_NOTE
};


const uint16_t too_full_signal[] = {
                            (500 / 4) << 8 | 4,
                            NOTE_C  | OCT_7     | QUARTER_NOTE,
                            NOTE_E              | QUARTER_NOTE,
                            NOTE_FS             | HALF_NOTE,
                            REST                | QUARTER_NOTE
};


const uint16_t activate_signal[] = {
                            (500 / 4) << 8 | 2,
                            NOTE_E  | OCT_6 | QUARTER_NOTE,
                            NOTE_C  | OCT_7 | HALF_NOTE
};

const uint16_t deactivate_signal[] = {
                            (500 / 4) << 8 | 2,
                            NOTE_C  | OCT_7 | QUARTER_NOTE,
                            NOTE_E  | OCT_6 | HALF_NOTE
};


const uint16_t requiem[] = {
                            (140 / 4) << 8 | 11,
                            NOTE_C  | OCT_6 | HALF_NOTE                 | 1,
                            NOTE_C          | QUARTER_NOTE | DOT_NOTE   | 1,
                            NOTE_C          | EIGHTHS_NOTE              | 1,
                            NOTE_C          | HALF_NOTE                 | 1,
                            NOTE_DS         | QUARTER_NOTE | DOT_NOTE   | 1,
                            NOTE_D          | EIGHTHS_NOTE              | 1,
                            NOTE_D          | QUARTER_NOTE | DOT_NOTE   | 1,
                            NOTE_C          | EIGHTHS_NOTE              | 1,
                            NOTE_C          | QUARTER_NOTE | DOT_NOTE   | 1,
                            NOTE_B  | OCT_5 | EIGHTHS_NOTE              | 1,
                            NOTE_C  | OCT_6 | WHOLE_NOTE                | 4
};




const uint16_t low_health_signal[] = {
                                (190 / 4) << 8 | 4,
                                NOTE_DS | OCT_6 | EIGHTHS_NOTE,
                                NOTE_D          | EIGHTHS_NOTE,
                                NOTE_C          | EIGHTHS_NOTE,
                                NOTE_B  | OCT_5 | EIGHTHS_NOTE | DOT_NOTE
};



const uint16_t low_battery_signal[] = {
                                (190 / 4) << 8 | 5,
                                NOTE_E  | OCT_5 | EIGHTHS_NOTE,
                                NOTE_DS         | EIGHTHS_NOTE,
                                NOTE_D          | EIGHTHS_NOTE,
                                NOTE_CS         | EIGHTHS_NOTE,
                                NOTE_C          | EIGHTHS_NOTE  | DOT_NOTE
};


const uint16_t session_completed_signal[] = {
    (450 / 4) << 8 | 19 ,
6 << 12 | OCT_7 | EIGHTHS_NOTE,
1 << 12 | OCT_6 | EIGHTHS_NOTE,
8 << 12 | OCT_6 | EIGHTHS_NOTE,
10 << 12 | OCT_7 | EIGHTHS_NOTE,
5 << 12 | OCT_7 | EIGHTHS_NOTE,
0 << 12 | OCT_6 | EIGHTHS_NOTE,
2 << 12 | OCT_7 | EIGHTHS_NOTE,
9 << 12 | OCT_7 | EIGHTHS_NOTE,
4 << 12 | OCT_7 | EIGHTHS_NOTE,
6 << 12 | OCT_8 | EIGHTHS_NOTE,
1 << 12 | OCT_7 | EIGHTHS_NOTE,
8 << 12 | OCT_7 | EIGHTHS_NOTE,
10 << 12 | OCT_8 | EIGHTHS_NOTE,
5 << 12 | OCT_8 | EIGHTHS_NOTE,
0 << 12 | OCT_7 | EIGHTHS_NOTE,
2 << 12 | OCT_8 | EIGHTHS_NOTE,
9 << 12 | OCT_8 | EIGHTHS_NOTE,
4 << 12 | OCT_8 | EIGHTHS_NOTE,
REST | HALF_NOTE
};


const uint16_t winning_signal[] = {
            (540 / 4) << 8 | 33,
            NOTE_D	| OCT_6 | EIGHTHS_NOTE,
            NOTE_E		    | EIGHTHS_NOTE,
            NOTE_FS		    | EIGHTHS_NOTE,
            NOTE_GS		    | EIGHTHS_NOTE,
            NOTE_A          | EIGHTHS_NOTE,
            NOTE_B		    | EIGHTHS_NOTE,
            NOTE_CS	| OCT_7 | EIGHTHS_NOTE,
            NOTE_D		    | EIGHTHS_NOTE,

            NOTE_D	| OCT_6 | EIGHTHS_NOTE,
            NOTE_E		    | EIGHTHS_NOTE,
            NOTE_FS		    | EIGHTHS_NOTE,
            NOTE_GS		    | EIGHTHS_NOTE,
            NOTE_A          | EIGHTHS_NOTE,
            NOTE_B		    | EIGHTHS_NOTE,
            NOTE_CS	| OCT_7 | EIGHTHS_NOTE,
            NOTE_D		    | EIGHTHS_NOTE,

            NOTE_D	| OCT_6 | EIGHTHS_NOTE,
            NOTE_E		    | EIGHTHS_NOTE,
            NOTE_FS		    | EIGHTHS_NOTE,
            NOTE_GS		    | EIGHTHS_NOTE,
            NOTE_A          | EIGHTHS_NOTE,
            NOTE_B		    | EIGHTHS_NOTE,
            NOTE_CS	| OCT_7 | EIGHTHS_NOTE,
            NOTE_D		    | EIGHTHS_NOTE,

            NOTE_D	| OCT_6 | EIGHTHS_NOTE,
            NOTE_E		    | EIGHTHS_NOTE,
            NOTE_FS		    | EIGHTHS_NOTE,
            NOTE_GS		    | EIGHTHS_NOTE,
            NOTE_A          | EIGHTHS_NOTE,
            NOTE_B		    | EIGHTHS_NOTE,
            NOTE_CS	| OCT_7 | EIGHTHS_NOTE,
            NOTE_D		    | EIGHTHS_NOTE,
            REST            | WHOLE_NOTE
};


const uint16_t panic_signal[] = {
            (270 / 4) << 8 | 13,
            NOTE_D | OCT_6  | QUARTER_NOTE,
            NOTE_GS         | HALF_NOTE,
            NOTE_A          | SIXTEENTHS_NOTE,
            NOTE_GS         | SIXTEENTHS_NOTE,
            NOTE_A          | SIXTEENTHS_NOTE,
            NOTE_GS         | SIXTEENTHS_NOTE,
            NOTE_A          | SIXTEENTHS_NOTE,
            NOTE_GS         | SIXTEENTHS_NOTE,
            NOTE_A          | SIXTEENTHS_NOTE,
            NOTE_GS         | SIXTEENTHS_NOTE,
            NOTE_A          | SIXTEENTHS_NOTE,
            NOTE_GS         | SIXTEENTHS_NOTE,
            REST            | HALF_NOTE
};


const uint16_t jingle[] = {
                           (600 / 4) << 8 | 8,
                           NOTE_G | OCT_7 | HALF_NOTE,
                           NOTE_E | HALF_NOTE,
                           NOTE_D | QUARTER_NOTE,
                           NOTE_F | QUARTER_NOTE,
                           NOTE_E | QUARTER_NOTE,
                           NOTE_D | QUARTER_NOTE,
                           NOTE_E | HALF_NOTE,
                           NOTE_C | HALF_NOTE
};


const uint16_t satisfied[] = {
                              (260 / 4) << 8    | 7,
                              NOTE_C | OCT_6    | QUARTER_NOTE | DOT_NOTE,
                              NOTE_C      | EIGHTHS_NOTE | 1,
                              NOTE_E      | EIGHTHS_NOTE | 1,
                              NOTE_G      | EIGHTHS_NOTE | 1,
                              NOTE_C | OCT_7  | QUARTER_NOTE | DOT_NOTE   | 10,
                              NOTE_C      | QUARTER_NOTE | DOT_NOTE   | 10,
                              NOTE_G | OCT_6  | WHOLE_NOTE            | 4
};



const uint16_t testing_slides[] = {
                                   (240/4) << 8 | 11,
                                   NOTE_C | OCT_6   | WHOLE_NOTE,
                                   NOTE_C           | WHOLE_NOTE    | SLIDE_NOTE,
                                   NOTE_G | OCT_6   | WHOLE_NOTE,
                                   NOTE_G           | HALF_NOTE     | SLIDE_NOTE,
                                   NOTE_C | OCT_6   | WHOLE_NOTE,
                                   NOTE_C           | QUARTER_NOTE  | SLIDE_NOTE,
                                   NOTE_E | OCT_6   | HALF_NOTE,
                                   NOTE_E           | QUARTER_NOTE  | SLIDE_NOTE,
                                   NOTE_G |OCT_6    | HALF_NOTE,
                                   NOTE_G           | WHOLE_NOTE    | SLIDE_NOTE,
                                   NOTE_C | OCT_7   | WHOLE_NOTE
};


const uint16_t lazer_beams[] = {
                                (280/4) << 8 | 18,
                                NOTE_C | OCT_6  | QUARTER_NOTE      | SLIDE_NOTE,
                                NOTE_C | OCT_1  | SIXTEENTHS_NOTE,
                                REST | HALF_NOTE,
                                NOTE_C | OCT_7  | QUARTER_NOTE      | SLIDE_NOTE,
                                NOTE_C | OCT_1  | SIXTEENTHS_NOTE,
                                REST | HALF_NOTE,
                                NOTE_C | OCT_6  | QUARTER_NOTE      | SLIDE_NOTE,
                                NOTE_C | OCT_3  | SIXTEENTHS_NOTE,
                                REST | HALF_NOTE,
                                NOTE_C | OCT_7  | QUARTER_NOTE      | SLIDE_NOTE,
                                NOTE_C | OCT_3  | SIXTEENTHS_NOTE,
                                REST | HALF_NOTE,
                                NOTE_C | OCT_6  | QUARTER_NOTE      | SLIDE_NOTE,
                                NOTE_C | OCT_3  | SIXTEENTHS_NOTE,
                                REST | HALF_NOTE,
                                NOTE_C | OCT_7  | QUARTER_NOTE      | SLIDE_NOTE,
                                NOTE_C | OCT_3  | SIXTEENTHS_NOTE,
                                REST | HALF_NOTE
};


/* Songs of old programs, long forgotten memories, bits dispersed, trampled by the neverending march of progress */
const uint16_t sounds_of_freed_memory[] = {
                                              (500/4) << 8 | 100
};



/* Lullaby */
const uint16_t lullaby[] = {
                        (100 / 4) << 8 | 52,
                        NOTE_A  | OCT_6 | EIGHTHS_NOTE | 1,
                        NOTE_A          | EIGHTHS_NOTE,
                        NOTE_C  | OCT_7 | HALF_NOTE,
                        NOTE_A  | OCT_6 | EIGHTHS_NOTE | 1,
                        NOTE_A          | EIGHTHS_NOTE,
                        NOTE_C  | OCT_7 | HALF_NOTE,
                        NOTE_A  | OCT_6 | EIGHTHS_NOTE,
                        NOTE_C  | OCT_7 | EIGHTHS_NOTE,
                        NOTE_F          | QUARTER_NOTE,
                        NOTE_E          | QUARTER_NOTE,
                        NOTE_D          | QUARTER_NOTE | 1,
                        NOTE_D          | QUARTER_NOTE,
                        NOTE_C          | QUARTER_NOTE | 2,
                        NOTE_G  | OCT_6 | EIGHTHS_NOTE,
                        NOTE_A          | EIGHTHS_NOTE,
                        NOTE_AS         | QUARTER_NOTE,
                        NOTE_G          | QUARTER_NOTE | 1,
                        NOTE_G          | EIGHTHS_NOTE,
                        NOTE_A          | EIGHTHS_NOTE,
                        NOTE_AS         | HALF_NOTE | 1,
                        NOTE_G          | EIGHTHS_NOTE,
                        NOTE_AS         | EIGHTHS_NOTE,
                        NOTE_E  | OCT_7 | EIGHTHS_NOTE,
                        NOTE_D          | EIGHTHS_NOTE,
                        NOTE_C          | QUARTER_NOTE,
                        NOTE_E          | QUARTER_NOTE,
                        NOTE_F          | HALF_NOTE,

                        NOTE_F  | OCT_6 | EIGHTHS_NOTE | 1,
                        NOTE_F          | EIGHTHS_NOTE,
                        NOTE_F  | OCT_7 | HALF_NOTE,
                        NOTE_D          | EIGHTHS_NOTE,
                        NOTE_AS | OCT_6 | EIGHTHS_NOTE,
                        NOTE_C  | OCT_7 | HALF_NOTE,
                        NOTE_A  | OCT_6 | EIGHTHS_NOTE,
                        NOTE_F          | EIGHTHS_NOTE,
                        NOTE_AS         | QUARTER_NOTE,
                        NOTE_C  | OCT_7 | QUARTER_NOTE,
                        NOTE_D          | QUARTER_NOTE,
                        NOTE_C          | HALF_NOTE | 1,
                        NOTE_F  | OCT_6 | EIGHTHS_NOTE | 1,
                        NOTE_F          | EIGHTHS_NOTE,
                        NOTE_F  | OCT_7 | HALF_NOTE,
                        NOTE_D          | EIGHTHS_NOTE,
                        NOTE_AS | OCT_6 | EIGHTHS_NOTE,
                        NOTE_C  | OCT_7 | HALF_NOTE | 1,
                        NOTE_A  | OCT_6 | EIGHTHS_NOTE,
                        NOTE_F          | EIGHTHS_NOTE,
                        NOTE_AS         | QUARTER_NOTE,
                        NOTE_A          | QUARTER_NOTE,
                        NOTE_G          | QUARTER_NOTE,
                        NOTE_F          | HALF_NOTE
};



/* TETRIS THEME SONG! */
const uint16_t tetris_theme_song[] = {
                                   (320/4) << 8       | 81, // left 8 is BMP/4, right 8 is number if notes

                                   NOTE_E | OCT_6  | HALF_NOTE     | 1,
                                   NOTE_B | OCT_5  | QUARTER_NOTE  | 1,
                                   NOTE_C | OCT_6  | QUARTER_NOTE  | 1,
                                   NOTE_D          | HALF_NOTE     | 1,
                                   NOTE_C          | QUARTER_NOTE  | 1,
                                   NOTE_B | OCT_5  | QUARTER_NOTE  | 1,
                                   NOTE_A          | HALF_NOTE     | 1,
                                   NOTE_A          | QUARTER_NOTE  | 1,
                                   NOTE_C | OCT_6  | QUARTER_NOTE  | 1,
                                   NOTE_E          | HALF_NOTE     | 1,
                                   NOTE_D          | QUARTER_NOTE  | 1,
                                   NOTE_C          | QUARTER_NOTE  | 1,
                                   NOTE_B | OCT_5  | HALF_NOTE,
                                   REST            | QUARTER_NOTE,
                                   NOTE_C | OCT_6  | QUARTER_NOTE  | 1,
                                   NOTE_D          | HALF_NOTE     | 1,
                                   NOTE_E          | HALF_NOTE     | 1,
                                   NOTE_C          | HALF_NOTE     | 1,
                                   NOTE_A | OCT_5  | HALF_NOTE     | 1,
                                   NOTE_A          | WHOLE_NOTE    | 1,

                                   REST            | QUARTER_NOTE  | 1,
                                   NOTE_D | OCT_6  | HALF_NOTE     | 1,
                                   NOTE_F          | QUARTER_NOTE  | 1,
                                   NOTE_A          | HALF_NOTE     | 1,
                                   NOTE_G          | QUARTER_NOTE  | 1,
                                   NOTE_F          | QUARTER_NOTE  | 1,
                                   NOTE_E          | HALF_NOTE      | DOT_NOTE,
                                   NOTE_C          | QUARTER_NOTE  | 1,
                                   NOTE_E          | HALF_NOTE     | 1,
                                   NOTE_D          | QUARTER_NOTE  | 1,
                                   NOTE_C          | QUARTER_NOTE  | 1,
                                   NOTE_B | OCT_5  | HALF_NOTE,
                                   NOTE_B          | QUARTER_NOTE  | 1,
                                   NOTE_C | OCT_6  | QUARTER_NOTE  | 1,
                                   NOTE_D          | HALF_NOTE     | 1,
                                   NOTE_E          | HALF_NOTE     | 1,
                                   NOTE_C          | HALF_NOTE     | 1,
                                   NOTE_A | OCT_5  | HALF_NOTE     | 1,
                                   NOTE_A          | WHOLE_NOTE    | 1,

                                   NOTE_E | OCT_6  | HALF_NOTE     | 1,
                                  NOTE_B | OCT_5  | QUARTER_NOTE  | 1,
                                  NOTE_C | OCT_6  | QUARTER_NOTE  | 1,
                                  NOTE_D          | QUARTER_NOTE     | 1,
                                  NOTE_E          | EIGHTHS_NOTE,
                                  NOTE_D          | EIGHTHS_NOTE,
                                  NOTE_C          | QUARTER_NOTE  | 1,
                                  NOTE_B | OCT_5  | QUARTER_NOTE  | 1,
                                  NOTE_A          | HALF_NOTE     | 1,
                                  NOTE_A          | QUARTER_NOTE  | 1,
                                  NOTE_C | OCT_6  | QUARTER_NOTE  | 1,
                                  NOTE_E          | HALF_NOTE     | 1,
                                  NOTE_D          | QUARTER_NOTE  | 1,
                                  NOTE_C          | QUARTER_NOTE  | 1,
                                  REST            | QUARTER_NOTE,
                                  NOTE_B | OCT_5  | HALF_NOTE     | 1,
                                  NOTE_C | OCT_6  | QUARTER_NOTE  | 1,
                                  NOTE_D          | HALF_NOTE     | 1,
                                  NOTE_E          | HALF_NOTE     | 1,
                                  NOTE_C          | HALF_NOTE     | 1,
                                  NOTE_A | OCT_5  | HALF_NOTE     | 1,
                                  NOTE_A          | WHOLE_NOTE    | 1,

                                  REST            | QUARTER_NOTE  | 1,
                                  NOTE_D | OCT_6  | HALF_NOTE     | 1,
                                  NOTE_F          | QUARTER_NOTE  | 1,
                                  NOTE_A          | HALF_NOTE     | 1,
                                  NOTE_G          | QUARTER_NOTE  | 1,
                                  NOTE_F          | QUARTER_NOTE  | 1,
                                  NOTE_E          | HALF_NOTE,
                                  NOTE_E          | QUARTER_NOTE  | 1,
                                  NOTE_C          | QUARTER_NOTE  | 1,
                                  NOTE_E          | HALF_NOTE     | 1,
                                  NOTE_D          | QUARTER_NOTE  | 1,
                                  NOTE_C          | QUARTER_NOTE  | 1,
                                  NOTE_B | OCT_5  | HALF_NOTE,
                                  NOTE_B          | QUARTER_NOTE  | 1,
                                  NOTE_C | OCT_6  | QUARTER_NOTE  | 1,
                                  NOTE_D          | HALF_NOTE     | 1,
                                  NOTE_E          | HALF_NOTE     | 1,
                                  NOTE_C          | HALF_NOTE     | 1,
                                  NOTE_A | OCT_5  | HALF_NOTE     | 1,
                                  NOTE_A          | WHOLE_NOTE    | 1
};





const uint16_t random_song[] = {
                         (255) << 8 | 26, // bmp and size of song
                         NOTE_E | OCT_6,
                         NOTE_F,
                         NOTE_G,
                         NOTE_G,
                         NOTE_F,
                         NOTE_E,
                         NOTE_D,
                         NOTE_E,
                         NOTE_F,
                         NOTE_F,
                         REST,
                         NOTE_F,
                         NOTE_E,
                         NOTE_E,
                         NOTE_D,
                         NOTE_E,
                         NOTE_E | HALF_NOTE,
                         NOTE_G | HALF_NOTE,
                         NOTE_E,
                         NOTE_D,
                         NOTE_D,
                         NOTE_D,
                         NOTE_C,
                         REST,
                         NOTE_C,
                         NOTE_C
};





/* The Imitation Game Theme Song Melody */
const uint16_t imitation_game_song[] = {
                                   (120/4) << 8    |     29,

                                   NOTE_F  | QUARTER_NOTE  | OCT_5,
                                   NOTE_G  | QUARTER_NOTE           ,
                                   NOTE_GS | QUARTER_NOTE           ,
                                   NOTE_C  | QUARTER_NOTE  | OCT_6  ,
                                   NOTE_C  | QUARTER_NOTE           ,
                                   NOTE_AS | QUARTER_NOTE  | OCT_5  ,
                                   NOTE_AS | QUARTER_NOTE           ,
                                   NOTE_GS | QUARTER_NOTE           ,
                                   NOTE_GS | HALF_NOTE,
                                   NOTE_GS | QUARTER_NOTE           ,

                                   NOTE_DS | QUARTER_NOTE  | OCT_6  ,
                                   NOTE_DS | QUARTER_NOTE           ,
                                   NOTE_D  | QUARTER_NOTE           ,
                                   NOTE_D  | QUARTER_NOTE           ,
                                   NOTE_C  | QUARTER_NOTE           ,
                                   NOTE_C  | WHOLE_NOTE,
                                   NOTE_AS | QUARTER_NOTE  | OCT_5  ,
                                   NOTE_GS | QUARTER_NOTE           ,
                                   NOTE_G  | QUARTER_NOTE           ,
                                   NOTE_DS | QUARTER_NOTE           ,
                                   NOTE_F  | QUARTER_NOTE,
                                   NOTE_F  | QUARTER_NOTE  | OCT_6  ,
                                   NOTE_F  | QUARTER_NOTE           ,
                                   NOTE_DS | QUARTER_NOTE           ,
                                   NOTE_DS | QUARTER_NOTE           ,
                                   NOTE_F  | QUARTER_NOTE           ,
                                   NOTE_F  | QUARTER_NOTE           ,
                                   NOTE_C  | QUARTER_NOTE           ,
                                   NOTE_C  | WHOLE_NOTE
};


/* Mary had a little lamb */
const uint16_t mary_had_a_little_lamb[] = {
                                     (120/4) << 8 | 29,

                                     NOTE_E | OCT_6 | QUARTER_NOTE | 2,
                                     NOTE_D  | QUARTER_NOTE | 2,
                                     NOTE_C | QUARTER_NOTE | 2,
                                     NOTE_D | QUARTER_NOTE | 2,
                                     NOTE_E | QUARTER_NOTE | 2,
                                     NOTE_E | QUARTER_NOTE | 2,
                                     NOTE_E | QUARTER_NOTE | 2,
                                     REST | QUARTER_NOTE | 2,
                                     NOTE_D | QUARTER_NOTE | 2,
                                     NOTE_D | QUARTER_NOTE | 2,
                                     NOTE_D | QUARTER_NOTE | 2,
                                     REST | QUARTER_NOTE | 2,
                                     NOTE_E | QUARTER_NOTE | 2,
                                     NOTE_G | QUARTER_NOTE | 2,
                                     NOTE_G | QUARTER_NOTE | 2,
                                     REST | QUARTER_NOTE | 2,
                                     NOTE_E | QUARTER_NOTE | 2,
                                     NOTE_D |  QUARTER_NOTE | 2,
                                     NOTE_C | QUARTER_NOTE | 2,
                                     NOTE_D | QUARTER_NOTE | 2,
                                     NOTE_E |  QUARTER_NOTE | 2,
                                     NOTE_E | QUARTER_NOTE | 2,
                                     NOTE_E | QUARTER_NOTE | 2,
                                     NOTE_E | QUARTER_NOTE | 2,
                                     NOTE_D |  QUARTER_NOTE | 2,
                                     NOTE_D | QUARTER_NOTE | 2,
                                     NOTE_E |  QUARTER_NOTE | 2,
                                     NOTE_D |  QUARTER_NOTE | 2,
                                     NOTE_C | HALF_NOTE | 2
};
