/**
 *  @file button_manager.h
 *  @author William Spinelli <william.spinelli(on)gmail>
 *
 *  @brief Manage multiple buttons connected to a single ADC input.
 *
 *  This module manages multiple buttons connected to a single ADC input with a
 *  resistive network.  It is not configurable to support a generic resistive
 *  network configuration but it is hardcoded to the resistive network used by
 *  this particular application.
 *
 *  It provides function to detect button status and button click.
 *
 *  This function doesn't perform button debounce specifically.  A natural
 *  debounce is introduced due to the call rate of the button manager.
 */

#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/**
 *  @brief Enumeration of the buttons managed by this module.
 */
enum {
    BUTTON_ON,      /**< Button associated to the ignition position ON. */
    BUTTON_START,   /**< Button associated to the ignition position START. */
    BUTTON_HORN,    /**< Button associated to the horn. */
    BUTTON_COUNT,   /**< Total buttons managed by the module. */
};

/**
 *  @brief Set the ADC value read on the resistive network.
 *
 *  This function receives the value read on the ADC (only the 8 most
 *  significant bits) and decodes the status of the three buttons connected to
 *  the resistive network.  This function detects also a click on the button as
 *  a rising edge on the button status.
 *
 *  This function doesn't perform button debounce specifically.  However
 *  considering that it is called every 40 ms there is a natural debounce of
 *  the inputs.
 *  @param adc_value The ADC value (only the 8 most significant bits).
 *  @note This function has to be called every 40 ms.
 */
void button_set_adc_value(uint8_t adc_value);

/**
 *  @brief Check whether the button is pressed.
 *  @param button The identifier of the requested button.
 *  @return true if the button is pressed.
 */
bool button_is_pressed(uint8_t button);

/**
 *  @brief Check whether the button has been clicked.
 *
 *  This function checks whether there was a rising edge on the button status,
 *  which is interpreted as a click.
 *  @param button The identifier of the requested button.
 *  @return true if the button is pressed.
 *  @note When called this function also reset the clicked flag.
 */
bool button_is_clicked(uint8_t button);

#endif
