/**
 *  @file sound_manager.c
 *  @author William Spinelli <william.spinelli(on)gmail>
 *  @brief Implementation for the functions defined in sound_manager.h.
 *  @warning Members listed here are intended for internal use only and should
 *  not be used directly!
 */

#include "sound_manager.h"
#include "tractor_model.h"

#include "engine_running.h"
#include "tractor_horn.h"

#ifdef __AVR__
#define AVR_PGM_READ_BYTE(A) pgm_read_byte(&(A))
#else
#include <string.h>
#define memcpy_P memcpy

#define AVR_PGM_READ_BYTE(A) (A)
#endif

/**
 *  @brief Duration for each note in a horn song.
 *
 *  This constant holds the note duration as a multiple of the audio manager
 *  update period TRACTOR_STATUS_UPDATE_CYCLE.
 */
static const uint8_t HORN_NOTE_DURATION = 160 / TRACTOR_STATUS_UPDATE_CYCLE;

/**
 *  @brief Duration for each pause (note 0) in a horn song.
 *
 *  This constant holds the pause duration as a multiple of the audio manager
 *  update period TRACTOR_STATUS_UPDATE_CYCLE.
 */
static const uint8_t HORN_PAUSE_DURATION = 40 / TRACTOR_STATUS_UPDATE_CYCLE;

/**
 *  @def MAX_SONG_SIZE
 *  @brief The maximum size for a horn song.
 */
#define MAX_SONG_SIZE       30

/**
 *  @brief Structure holding the details of a horn song.
 */
typedef struct {
    uint8_t size;                   /**< The actual size of the song. */
    uint8_t note[MAX_SONG_SIZE];    /**< Array containing the song notes. */
} Song;

/**
 *  @def SONG
 *  @brief Variadic macro used to initialize song size and notes.
 *
 *  This macro is used to generate the song
 */
#define SONG(...) \
{sizeof((uint8_t[]){__VA_ARGS__}), {__VA_ARGS__}}

/**
 *  @brief Structure holding the collection of all the horn songs.
 */
static const Song SONG_LIBRARY[] PROGMEM = {
    SONG(64, 64, 64),
    SONG(64, 64, 0, 64, 64),
    SONG(64, 80, 64, 64, 0, 64, 64, 0, 64, 72, 80, 85,
            96, 96, 0, 96, 96, 0, 96, 96, 0, 80, 80),
};

/**
 *  @brief Structure holding the details of the current song.
 *
 *  This structure contains some redundant information (e.g. the field #song is
 *  a RAM copy of the current song from @a SONG_LIBRARY, or the field
 *  @a index_increment could be obtained indexing the @a note field in
 *  @a song).  This is done in order to simplify access to PROGMEM or to
 *  improve performance.
 */
typedef struct {
    Song    song;               /**< The current horn song. */
    uint8_t current_note;       /**< The index of the current note. */
    uint8_t note_counter;       /**< The counter used to manage note and pause duration. */
    uint8_t index_increment;    /**< The current index increment. */
    bool    playing;            /**< Whether a horn song is being played. */
} Horn;

/**
 *  @brief Details of the current song.
 */
static Horn horn = {
    .song               = {0, {0}},
    .current_note       = 0,
    .note_counter       = 0,
    .index_increment    = 0,
    .playing            = false,
};

/**
 * @brief Structure holding the index of the audio samples.
 */
typedef struct {
    uint16_t    engine;     /**< Index of the engine audio sample. */
    uint16_t    horn;       /**< Index of the horn audio sample. */
} SampleIndex;

/**
 *  @brief Index of the current audio sample.
 */
static SampleIndex sample_index = {
    .engine = 0,
    .horn   = 0,
};

uint8_t audio_get_next_sample(uint8_t engine_speed)
{
    uint8_t engine_sample;
    if (engine_speed) {
        /*
         * The counter used for the engine track has a precision of 4 bits to
         * allow 16 different audio frequency per unit
         * Since the engine speed covers a range of
         *      (2100 - 800) / 800 = 1.62
         * this allows to simulate ~25 different speed values in the operating
         * range (plus 16 below the idle speed).
         */
        sample_index.engine += (engine_speed >> 2);
        if (sample_index.engine >= (ENGINE_RUNNING_SIZE << 4))
            sample_index.engine -= (ENGINE_RUNNING_SIZE << 4);
        uint16_t index = (sample_index.engine >> 4);
        uint8_t offset = (uint8_t)(sample_index.engine & 0x000F);
        if (offset < 16)
            engine_sample = AVR_PGM_READ_BYTE(ENGINE_RUNNING[index]);
        else if (offset < 48)
            engine_sample = (AVR_PGM_READ_BYTE(ENGINE_RUNNING[index]) >> 1) +
                    (AVR_PGM_READ_BYTE(ENGINE_RUNNING[index + 1]) >> 1);
        else
            engine_sample = AVR_PGM_READ_BYTE(ENGINE_RUNNING[index + 1]);
    } else {
        sample_index.engine = 0;
        engine_sample = 128;
    }

    uint8_t horn_sample;
    if (horn.playing && horn.index_increment) {
        /*
         * The counter used for the engine track has a precision of 6 bits to
         * allow 64 different audio frequency per octave.
         * Having an index_increment equal to 0 while a song is being played
         * is used to insert pauses between the notes.
         */
        sample_index.horn += horn.index_increment;
        if (sample_index.horn >= (TRACTOR_HORN_SIZE << 6))
            sample_index.horn -= (TRACTOR_HORN_SIZE << 6);
        uint16_t index = (sample_index.horn >> 6);
        uint8_t offset = (uint8_t)(sample_index.horn & 0x003F);
        if (offset < 16)
            horn_sample = AVR_PGM_READ_BYTE(TRACTOR_HORN[index]);
        else if (offset < 48)
            horn_sample = (AVR_PGM_READ_BYTE(TRACTOR_HORN[index]) >> 1) +
                    (AVR_PGM_READ_BYTE(TRACTOR_HORN[index + 1]) >> 1);
        else
            horn_sample = AVR_PGM_READ_BYTE(TRACTOR_HORN[index + 1]);
    } else {
        sample_index.horn = 0;
        horn_sample = 128;
    }

    /*
     * Mix the sounds adding the engine track to the horn track with offset of
     * 128 removed, and saturating the output between 0 and 255.
     */
    int16_t sample = (int16_t)engine_sample + horn_sample - 128;
    if (sample > UINT8_MAX)
        sample = UINT8_MAX;
    else if (sample < 0)
        sample = 0;
    return (uint8_t)sample;
}

void audio_play_horn_song(uint8_t song)
{
    if (song < SONG_COUNT) {
        memcpy_P(&horn.song, &SONG_LIBRARY[song], sizeof(Song));
        horn.current_note       = 0;
        horn.note_counter       = 0;
        horn.index_increment    = horn.song.note[0];
        horn.playing            = true;
    }
}

void audio_horn_manager(void)
{
    if (horn.playing) {
        uint8_t duration = horn.index_increment ?
                HORN_NOTE_DURATION : HORN_PAUSE_DURATION;
        if (++horn.note_counter >= duration) {
            horn.note_counter = 0;
            if (++horn.current_note > horn.song.size)
                horn.playing = false;
        }

        horn.index_increment = horn.song.note[horn.current_note];
    }
}
