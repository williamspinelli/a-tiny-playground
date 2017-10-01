/**
 *  @file tractor_model.h
 *  @author William Spinelli <william.spinelli(on)gmail>
 *
 *  @brief Simple simulation of a tractor model.
 *
 *  This module provides a simple simulation of a tractor model.  The following
 *  aspects are simulated:
 *  - Ignition key management
 *   + The engine is switched on if the ignition key is kept in position START
 *     for at least 4 seconds
 *   + The engine is switched off when the ignition key is put in position STOP
 *  - Throttle management
 *   + The engine speed is updated based on the value of the engine speed
 *     setpoint using a first order low-pass digital filter
 *  - Led management
 *   + The led lamp periodically blinks 6 times when engine speed is not idle
 *  - Sound management
 *   + The horn periodically plays double and single honks when engine speed is
 *     not idle
 *  - Electrical motor management
 *   + An electrical motor is driven with a low frequency PWM proportional to
 *     the current engine speed
 *
 *  All the engine speeds are represented internally as BP6 relative format
 *  (where 64 means the idle speed 800 rpm).  This is done to have faster
 *  formulas where shifts can be used in place of divisions.
 */

#ifndef TRACTOR_MODEL_H
#define TRACTOR_MODEL_H

#include <stdint.h>
#include <stdbool.h>

/**
 *  @def TRACTOR_STATUS_UPDATE_CYCLE
 *  @brief The call rate of the tractor status manager.
 *
 *  This define represents the call rate of the tractor status manager.  It is
 *  used as a time base for all the timers used in the code.
 */
#define TRACTOR_STATUS_UPDATE_CYCLE     40

/**
 *  @def ENGINE_SPEED_IDLE
 *  @brief The idle engine speed.
 *
 *  This define represents the idle engine speed (800 rpm) in BP6 format.
 */
#define ENGINE_SPEED_IDLE   64

/**
 *  @def ENGINE_SPEED_MIN
 *  @brief The minimum engine speed to enable audio and visual effects.
 *
 *  This define represents the minimum engine speed to enable audio and visual
 *  effects (850 rpm) in BP6 format.
 */
#define ENGINE_SPEED_MIN   68

/**
 *  @def ENGINE_SPEED_MAX
 *  @brief The maximum engine speed.
 *
 *  This define represents the maximum engine speed (2100 rpm) in BP6 format.
 */
#define ENGINE_SPEED_MAX    168

/**
 *  @brief Enumeration of the possible status for the ignition position.
 */
enum {
    IGNITION_OFF,       /**< Ignition position on OFF. */
    IGNITION_ON,        /**< Ignition position on ON. */
    IGNITION_START,     /**< Ignition position on START. */
};

/**
 *  @brief Update the tractor model.
 *
 *  This function updates the tractor model simulating ignition key management,
 *  engine speed based on throttle position, audio effects and light effects.
 *  @return The status of the led lamp.
 *  @note This function has to be called every 40 ms.
 */
bool tractor_update_model(void);

/**
 *  @brief Set the current position of the ignition key.
 *  @param position Set the current ignition position.
 */
void tractor_set_ignition_position(uint8_t position);

/**
 *  @brief Set the requested engine speed setpoint.
 *  @param setpoint The requested engine speed setpoint in BP6 format.
 */
void tractor_set_engine_speed_setpoint(uint8_t setpoint);

/**
 *  @brief Start (or restart) the playback of the "Dixie" horn tone.
 */
void tractor_play_dixie_song(void);

/**
 *  @brief Get the current engine speed.
 *  @return The engine speed in BP6 format.
 */
uint8_t tractor_get_engine_speed(void);

#endif
