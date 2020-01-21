/**
 *  @file sound_manager.h
 *  @author William Spinelli <william.spinelli(on)gmail>
 *
 *  @brief Provide basic sound effects for a tractor model.
 *
 *  This module provides some Lo-Fi sound effects for a tractor model.  One new
 *  audio sample is generated every cycle, with a rate of 8 kHz (calling the
 *  function audio_get_next_sample).
 *
 *  There are two main audio tracks that can be played together:
 *   - an engine audio track
 *   - a horn audio track
 *  When the horn track is being played the volume of the engine track is
 *  lowered to make it easier to understand.
 *
 *  The tracks are stored as arrays (in PROGMEM on the ATtiny).  They can be
 *  generated from a mono WAV file with 8-bit unsigned samples using the Python
 *  script wav2c.py in the sound directory.
 *
 *  In order to simulate different engine speeds, the samples are played back
 *  at different speeds.  This is done using a BP4 (binary point 4) counter
 *  which is incremented in steps proportional to the engine speed.  A step
 *  increment of 16 means that the track is played back at original speed.
 *  A step increment of 40 means that the track is played back at 2.5x the
 *  original speed.
 *
 *  Based on the value of <em>index</em>, a rough sample interpolation is done
 *  choosing the closest value among:
 *   - <em>TRACK[floor(index)]</em>
 *   - <em>(TRACK[floor(index)] >> 1) + (TRACK[ceil(index)] >> 1)</em>
 *   - <em>TRACK[ceil(index)]</em>
 *
 *  The same idea is used for simulating different horn notes.  Here a BP6
 *  counter is used instead and the step increment is chosen according to the
 *  note that has to be played.  The same interpolation approach is used.
 *
 *  @warning This is probably neither the most effective way to playback a
 *  given soundwave on ATtiny, nor the one with the highest fidelity.  But I
 *  wanted to try out this approach to test out something new.  Also one target
 *  in this project was to completely avoid multiplication and division and
 *  rely only on left and right shifts. And in the end it was not that bad for
 *  its purpose =)
 */

#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/**
 *  @brief Enumeration of the horn songs managed by this module.
 */
enum {
    SONG_SINGLE_HONK,   /**< Play a single honk with the horn. */
    SONG_DOUBLE_HONK,   /**< Play a double honk with the horn. */
    SONG_DIXIE,         /**< Play the "Dixie" song with the horn. */
    SONG_COUNT,         /**< Total songs managed by the module. */
};

/**
 *  @brief Compute the next audio sample to play.
 *
 *  This function computes the next audio sample to play as a 8-bit unsigned
 *  sample.  The default output when no sound is being played is therefore 128.
 *  @param engine_speed The current engine speed.
 *  @return The next audio sample to play.
 *  @note This function has to be called every 125 us (8 kHz).
 */
uint8_t audio_get_next_sample(uint8_t engine_speed);

/**
 *  @brief Start (or restart) the playback of the given horn song.
 *  @param song The index of the horn song to play.
 */
void audio_play_horn_song(uint8_t song);

/**
 *  @brief Manage the playback of the horn track.
 *  @note This function has to be called every 40 ms.
 */
void audio_horn_manager(void);

#endif
