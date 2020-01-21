/**
 *  @file tractor_model.c
 *  @author William Spinelli <william.spinelli(on)gmail>
 *  @brief Implementation for the functions defined in tractor_model.h.
 *  @warning Members listed here are intended for internal use only and should
 *  not be used directly!
 */

#include "tractor_model.h"

#include "sound_manager.h"

/**
 *  @brief Minimum time required to start the engine.
 *
 *  This constant holds the minimum time for which the ignition key has to stay
 *  in START position, as a multiple of the audio manager update period
 *  @a TRACTOR_STATUS_UPDATE_CYCLE.
 */
static const uint8_t CRANKING_MINIMUM_TIME = 4000 / TRACTOR_STATUS_UPDATE_CYCLE;

/**
 *  @brief Engine speed during cranking.
 *
 *  This constant holds the engine speed which is kept during the cranking
 *  stage.
 */
static const uint8_t CRANKING_ENGINE_SPEED = 2 * ENGINE_SPEED_IDLE / 5;

/**
 *  @brief Period of the automatic horn honking.
 *
 *  This constant holds the period of the automatic horn honking, as a multiple
 *  of the tractor manager update period @a TRACTOR_STATUS_UPDATE_CYCLE.
 *  A single honk is played at the end of the cycle and a single honk is played
 *  at the half way.
 */
static const uint16_t HORN_CYCLE = 16000 / TRACTOR_STATUS_UPDATE_CYCLE;

/**
 *  @brief Period of the automatic led blinking.
 *
 *  This constant holds the period of the automatic led blinking, as a multiple
 *  of the tractor manager update period @a TRACTOR_STATUS_UPDATE_CYCLE.
 *  The led blinking is performed at the beginning of the cycle.
 */
static const uint8_t LED_CYCLE = 3000 / TRACTOR_STATUS_UPDATE_CYCLE;

/**
 *  @brief Enumeration of the engine status managed by the tractor model.
 */
typedef enum {
    ENGINE_OFF,         /**< Engine status is not running. */
    ENGINE_CRANKING,    /**< Engine is cranking. */
    ENGINE_RUNNING,     /**< Engine is up and running. */
} EngineStatus;

/**
 *  @brief Structure holding the details of the current tractor model.
 */
typedef struct {
    uint16_t    engine_speed;   /**< Current engine speed in BP14 format */
    uint16_t    horn_counter;   /**< Counter to manage automatic horn honks. */
    uint8_t     status;         /**< Engine status. */
    uint8_t     engine_speed_setpoint;  /**< Current engine speed setpoint in BP6 format. */
    uint8_t     ignition_position;      /**< Current ignition position. */
    uint8_t     cranking_counter;       /**< Counter to manage cranking. */
    uint8_t     led_counter;            /**< Counter to manage hexa-blinking. */
} Tractor;

/**
 *  @brief Status of the tractor model.
 */
static Tractor tractor = {
    .engine_speed           = 0,
    .horn_counter           = 0,
    .led_counter            = 0,
    .status                 = ENGINE_OFF,
    .engine_speed_setpoint  = 0,
    .ignition_position      = IGNITION_OFF,
    .cranking_counter       = 0,
};

/**
 *  @brief Update current engine speed.
 *
 *  This function updates the current engine speed based on the value of the
 *  engine speed setpoint using a first order low-pass digital filter.  The
 *  filter has the following transfer function:
 *      x[t + 1] = (1/2^a) * u[t] + (1 - 1/2^a) * x[t]
 *  This is a fast (and rough) implementation of a first order low-pass digital
 *  filter.  This implementation totally avoids multiplication and division.
 */
static void update_engine_speed(uint8_t a)
{
    tractor.engine_speed = (uint16_t)((((uint32_t)tractor.engine_speed << a) -
            tractor.engine_speed +
            (uint16_t)((uint16_t)tractor.engine_speed_setpoint << 8)) >> a);
}

/**
 *  @brief Compute the status of the led associated to periodic blinking.
 *
 *  This function computes the status of the led because of the periodic
 *  hexa-blinking.  It performs 6 blinks that last 40 ms each and are
 *  separated by a 40 ms interval.
 *  @return Whether the led is on.
 */
static bool is_led_on(void)
{
    if (tractor_get_engine_speed() >= ENGINE_SPEED_MIN)
        return ((tractor.led_counter & 0x01) != 0 && tractor.led_counter < 12);
    else
        return false;
}

bool tractor_update_model(void)
{
    switch (tractor.status) {
        default:
            if (tractor.ignition_position == IGNITION_START) {
                tractor.status                  = ENGINE_CRANKING;
                tractor.engine_speed_setpoint   = CRANKING_ENGINE_SPEED;
                tractor.cranking_counter        = 0;
            }
            update_engine_speed(2);
            break;

        case ENGINE_CRANKING:
            if (++tractor.cranking_counter > CRANKING_MINIMUM_TIME) {
                tractor.status                  = ENGINE_RUNNING;
                tractor.engine_speed_setpoint   = ENGINE_SPEED_IDLE;
                tractor.horn_counter            = 0;
                tractor.led_counter             = LED_CYCLE;
            } else if (tractor.ignition_position != IGNITION_START) {
                tractor.status                  = ENGINE_OFF;
                tractor.engine_speed_setpoint   = 0;
            }
            update_engine_speed(4);
            break;

        case ENGINE_RUNNING:
            if (tractor.ignition_position == IGNITION_OFF) {
                tractor.status                  = ENGINE_OFF;
                tractor.engine_speed_setpoint   = 0;
            } else {
                if (tractor_get_engine_speed() >= ENGINE_SPEED_MIN) {
                    if (++tractor.horn_counter == HORN_CYCLE / 2) {
                        audio_play_horn_song(SONG_DOUBLE_HONK);
                    } else if (tractor.horn_counter >= HORN_CYCLE) {
                        audio_play_horn_song(SONG_SINGLE_HONK);
                        tractor.horn_counter = 0;
                    }
                    update_engine_speed(6);
                } else {
                    update_engine_speed(4);
                }

                if (++tractor.led_counter > LED_CYCLE)
                    tractor.led_counter = 0;
            }
            break;
    }

    audio_horn_manager();

    return is_led_on();
}

void tractor_set_ignition_position(uint8_t position)
{
    tractor.ignition_position = position;
}

void tractor_set_engine_speed_setpoint(uint8_t setpoint)
{
    if (tractor.status != ENGINE_RUNNING)
        return;

    if (setpoint > ENGINE_SPEED_MAX)
        setpoint = ENGINE_SPEED_MAX;
    else if (setpoint < ENGINE_SPEED_IDLE)
        setpoint = ENGINE_SPEED_IDLE;

    tractor.engine_speed_setpoint = setpoint;
}

void tractor_play_dixie_song(void)
{
    if (tractor.ignition_position == IGNITION_OFF)
        return;

    tractor.horn_counter = 0;
    audio_play_horn_song(SONG_DIXIE);
}

uint8_t tractor_get_engine_speed(void)
{
    return (uint8_t)(tractor.engine_speed >> 8);
}
