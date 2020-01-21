/**
 *  @file button_manager.c
 *  @author William Spinelli <william.spinelli(on)gmail>
 *  @brief Implementation for the functions defined in button_manager.h.
 *  @warning Members listed here are intended for internal use only and should
 *  not be used directly!
 */

#include "button_manager.h"

/**
 *  @def BM(button)
 *  @brief Macro that returns the bitmask for the button with index \a button.
 */
#define BM(button) (1 << (button))

/**
 *  @brief Variable holding the button level.
 *
 *  This variable holds the level of the buttons packed in a single uint8_t.
 *  In order to extract the proper value, the variable should be masked using
 *  the macro BM.
 */
static uint8_t button_level = 0x00;

/**
 *  @brief Variable holding whether the button was clicked.
 *
 *  This variable holds the click status of the buttons packed in a single
 *  uint8_t.  In order to extract the proper value, the variable should be
 *  masked using the macro BM.
 */
static uint8_t button_clicked = 0x00;

/**
 *  @def ADC_THRESHOLD_OFF
 *  @brief Threshold below which no button is pressed.
 */
#define ADC_THRESHOLD_OFF       23

/**
 *  @def ADC_THRESHOLD_ON
 *  @brief Threshold below which ON button is pressed.
 */
#define ADC_THRESHOLD_ON        63

/**
 *  @def ADC_THRESHOLD_ON_HORN
 *  @brief Threshold below which ON and HORN buttons are pressed.
 */
#define ADC_THRESHOLD_ON_HORN   99

/**
 *  @def ADC_THRESHOLD_ON_START
 *  @brief Threshold below which ON and START buttons are pressed.
 */
#define ADC_THRESHOLD_ON_START  186

void button_set_adc_value(uint8_t adc_value)
{
    uint8_t button_new_level;

    // find the new button level based on the ADC value
    if (adc_value <= ADC_THRESHOLD_OFF)
        button_new_level = 0x00;
    else if (adc_value <= ADC_THRESHOLD_ON)
        button_new_level = BM(BUTTON_ON);
    else if (adc_value <= ADC_THRESHOLD_ON_HORN)
        button_new_level = BM(BUTTON_ON) | BM(BUTTON_HORN);
    else if (adc_value <= ADC_THRESHOLD_ON_START)
        button_new_level = BM(BUTTON_ON) | BM(BUTTON_START);
    else
        button_new_level = BM(BUTTON_ON) | BM(BUTTON_START) | BM(BUTTON_HORN);

    // check if there is a rising edge in the button level
    button_clicked |= (~button_level & button_new_level) &
            ((1 << BUTTON_COUNT) - 1);

    button_level = button_new_level;
}

bool button_is_pressed(uint8_t button)
{
    return (button_level & BM(button)) != 0;
}

bool button_is_clicked(uint8_t button)
{
    uint8_t mask = (uint8_t)BM(button);
    bool clicked = (button_clicked & mask) != 0;
    button_clicked &= ~mask;        // reset the click flag!
    return clicked;
}
