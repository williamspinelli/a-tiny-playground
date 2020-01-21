/**
 *  @mainpage A Tiny Tractor
 *
 *  @section section_summary Summary
 *  This is an ATtiny85 project that generates some audio and visual effects
 *  for a tractor model.
 *
 *  It is intended to work on a tractor model (see the demo video), that
 *  includes:
 *   - an ignition key (with OFF, ON and START position) that controls the
 *     engine status
 *   - a HORN button, that plays "Dixie" using a horn sound
 *   - a THROTTLE potentiometer, that controls the engine speed
 *   - a LED light, that blinks periodically
 *   - a DC motor, that turns at a speed proportional to the engine speed
 *
 *  The core of the project is the sound generation.  It uses a quick and dirty
 *  looping technique to roughly generate engine sound effects with variable
 *  speed.  It also allow to play three different kind of horn songs (including
 *  the famous "Dixie" song).  The details of the audio playback are described
 *  in the file sound_manager.h
 *
 *  Note that this is probably neither the most effective way to playback a
 *  given soundwave on ATtiny, nor the one with the highest fidelity, since it
 *  doesn't use any real soundwave interpolation.  It was done in this way
 *  since I wanted to experiment with this particular approach.
 *
 *  @section section_schematic Schematic
 *  The software is designed to work with the circuit shown in the schematic
 *  directory, that uses the following pinout
 *  - Pin 1: Not used
 *  - Pin 2: Throttle Input
 *  - Pin 3: Sound Output
 *  - Pin 4: GND
 *  - Pin 5: LED Output
 *  - Pin 6: DC MOTOR Output
 *  - Pin 7: Resistive network (Buttons ON, START, HORN)
 *  - Pin 8: Vcc
 *
 *  @section section_video Demo Video
 *  You can find a demo video here:
 *
 *  @section section_remarks Some final remarks...
 *  The software is tailored to an ATtiny85, but it can be customized to work
 *  on any other ATtiny MCU provided that it has enough flash.
 *
 *  All the software modules are thoroughly documented using Doxygen.
 */

/**
 *  @file main.c
 *  @author William Spinelli <william.spinelli(on)gmail>
 */

#include <stdbool.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "button_manager.h"
#include "sound_manager.h"
#include "tractor_model.h"

/**
 *  @def LED_PIN
 *  @brief Define used to control level on pin connected to LED (pin 5).
 */
#define LED_PIN                 PB0

/**
 *  @def LED_OUT
 *  @brief Define used to set pin connected to LED (pin 5) as output.
 */
#define LED_OUT                 DDB0

/**
 *  @def DC_MOTOR_PIN
 *  @brief Define used to control level on pin connected to DC MOTOR (pin 6).
 */
#define DC_MOTOR_PIN            PB1

/**
 *  @def DC_MOTOR_OUT
 *  @brief Define used to set pin connected to DC MOTOR (pin 6) as output.
 */
#define DC_MOTOR_OUT            DDB1

/**
  *  @brief Period of the tractor model update.
  *
  *  This constant represents the cycle time used to downsample the audio
  *  sample update cycle (8 kHz) to the model update rate (25 Hz = 40 ms).
  */
static const uint16_t TRACTOR_MODEL_CYCLE = 8000 / 25;

/**
 *  @brief Flag used to trigger the generation of a new audio sample.
 */
static volatile bool update_audio_sample = false;

/**
 *  @def PWM_DC_MOTOR_MIN
 *  @brief The minimum duty cycle on the DC MOTOR pin (10%).
 */
#define PWM_DC_MOTOR_MIN        6

/**
 *  @def PWM_DC_MOTOR_MAX
 *  @brief The maximum duty cycle on the DC MOTOR pin (90%).
 */
#define PWM_DC_MOTOR_MAX        58

/**
 *  @brief The value of the duty cycle for the PWM on the DC MOTOR pin.
 *
 *  This variable holds the value of the duty cycle for the soft PWM generated
 *  on the DC MOTOR output pin.  The PWM has a resolution of 6 bits, therefore
 *  63 means a duty cycle of 100%.  The soft PWM has a carrier frequency of
 *  125 Hz, and is generated inside the TIMER0 ISR.
 */
static volatile uint8_t output_pwm_duty_cycle = 0;

/**
 *  @brief The ADC value on the pin connected to throttle.
 *
 *  This variable holds the ADC value (only the 8 most significant bits) read
 *  on the pin connected to the throttle.  The throttle ADC value is converted
 *  to an engine speed setpoint using the following relation:
 *  Voltage: 0.15 Vcc -> ADC: 38 -> setpoint: 64 (800 rpm)
 *  Voltage: 0.90 Vcc -> ADC: 230 -> setpoint: 168 (2100 rpm)
 *  Relation: setpoint = 64 + (ADC - 38) * 104 / 192 ~> 64 + (ADC - 38) >> 1
 *  There is no need to saturate low setpoint value, since this is
 *  already done insider the function tractor_set_engine_speed_setpoint
 */
static volatile uint8_t adc_value_throttle = 0;

/**
 *  @def ADC_THROTTLE_IDLE
 *  @brief The throttle ADC value associated to the idle engine speed.
 */
#define ADC_THROTTLE_IDLE       38

/**
 *  @brief The ADC value on the pin connected to buttons.
 *
 *  This variable holds the ADC value (only the 8 most significant bits) read
 *  on the pin connected to the resistive network used to read the button
 *  status.
 */
static volatile uint8_t adc_value_buttons = 0;

/**
 *  @brief Enumeration of the ADC channels managed by the software.
 */
enum {
    ADC_READ_THROTTLE,  /**< The ADC channel connected to throttle. */
    ADC_READ_BUTTONS,   /**< The ADC channel connected to buttons. */
};

/**
 *  @brief Select between the two ADC channels.
 *
 *  This variable is used to toggle between the two ADC channels managed in the
 *  software.
 */
static volatile uint8_t adc_mux_selection = ADC_READ_THROTTLE;

/**
 *  @brief Initialize the ATtiny85 peripherals
 */
static inline void setup(void)
{
    cli();                          // disable interrupts

    /*
     * Init IO peripherals
     */
    MCUCR |= _BV(PUD);              // disable pull-ups globally
    DDRB = _BV(LED_OUT) |           // set LED pin as output
            _BV(DDB4) |             // set pin 2 as output (speaker)
            _BV(DC_MOTOR_OUT);      // set DC MOTOR pin as output

    /*
     * Init timer to manage audio samples and soft PWM
     * Setup timer T0: interrupt @ 8.0kHz
     * T0 => F_CPU / prescaler / (OCR0A + 1) = 8000000 / 8 / 125 = 8 kHz
     * Motor PWM => Soft PWM @ 125 Hz => 6 bit resolution
     */
    TCCR0A = _BV(WGM01);            // Clear Timer on Compare mode
    TCCR0B = _BV(CS01);             // set Clock Prescaler to 8
    OCR0A = 124;                    // set Output Compare Register
    TIMSK = _BV(OCIE0A);            // enable Compare Match interrupt

    /*
     * Init timer for audio output in fast PWM mode @ 250 kHz
     * PWM  => F_CPU * PLL_8 / (OCR0C + 1) = 8000000 * 8 / 256 = 250 kHz
     */
    PLLCSR |= _BV(PLLE) |           // enable PLL
            _BV(PCKE);              // enable PCK to set T1 to 64 MHz
    TCCR1 = _BV(CS10);              // set T1 Clock Prescaler to 1
    GTCCR = _BV(PWM1B) |            // enable PWM on OC1B
            _BV(COM1B1);            // OC1B cleared on compare match
    OCR1C = 255;

    /*
     * Setup the ADC input
     * Set ADC clock: 8000000 / 64 = 125 kHz
     */
    ADMUX = _BV(ADLAR);             // left adjust ADC read
    ADCSRA = _BV(ADEN) |            // enable ADC
            _BV(ADIE) |             // enable ADC interrupt
            _BV(ADPS2) |            // set Clock Prescaler to 64
            _BV(ADPS1);

    sei();                          // enable interrupts
}

/**
 *  @brief Update the LED output pin.
 *  @param led_status Whether the led is ON or OFF.
 */
static void output_set_led(bool led_status)
{
    if (led_status)
        PORTB |= _BV(LED_PIN);
    else
        PORTB &= ~_BV(LED_PIN);
}

/**
 *  @brief Set the duty cycle on the DC MOTOR pin.
 *
 *  This function computes the value of the duty cycle for the soft PWM.  The
 *  duty cycle is kept to 0, while the engine speed is below the minimum speed
 *  @a ENGINE_SPEED_MIN.  Otherwise the duty cycle is computed according to the
 *  following relation:
 *  Speed: 68 (850 rpm) -> PWM: 6 (10%)
 *  Speed: 168 (2100 rpm) -> PWM: 56 (90%)
 *  Relation: PWM = 6 + (speed - 68) * 50 / 100 ~> 6 + (speed - 68) >> 1
 *  @param engine_speed The current engine speed.
 */
static inline void output_set_dc_motor_pwm(uint8_t engine_speed)
{
    if (engine_speed < ENGINE_SPEED_MIN) {
        output_pwm_duty_cycle = 0;
    } else {
        uint8_t duty_cycle = PWM_DC_MOTOR_MIN +
                ((engine_speed - ENGINE_SPEED_IDLE) >> 1);
        if (duty_cycle > (PWM_DC_MOTOR_MAX))
            duty_cycle = PWM_DC_MOTOR_MAX;
        output_pwm_duty_cycle = duty_cycle;
    }
}


/**
 *  @brief Start the ADC conversion for the given input.
 *
 *  This function is used to start the ADC conversion for the given ADC
 *  channel.  The result of the conversion is read inside the @a ADC_vect
 *  interrupt service routine.
 *  @param mux The identifier of the ADC channel to use.
 */
static inline void adc_start_conversion(uint8_t mux)
{
    adc_mux_selection = mux;

    switch (adc_mux_selection) {
        default:
            ADMUX |= _BV(MUX0) |    // select ADC3
                    _BV(MUX1);
            break;

        case ADC_READ_BUTTONS:
            ADMUX &= ~_BV(MUX1);    // clear selection
            ADMUX |= _BV(MUX0);     // select ADC1
            break;
    }

    ADCSRA |= _BV(ADSC);            // start the ADC conversion
}

/**
 *  @brief ISR associated to TIMER0 overflow.
 *
 *  This function is run with a rate of 8 kHz.  It basically does two things:
 *  - triggers the generation of a new audio sample.
 *  - generate a soft PWM with a carrier frequency of 125 Hz and a resolution
 *    of 6 bits.  Output is set at the beginning of the cycle and is reset
 *    when the counter is higher than the requested PWM duty cycle.
 *    If duty cycle is 0, then the output is not set to avoid a possible spike
 *    on the output.
 *  @note The audio sample update and all the main logic is performed outside
 *  the interrupt, to keep the soft PWM generation nice and smooth.
 */
ISR (TIMER0_COMPA_vect)
{
    update_audio_sample = true;

    static uint8_t pwm_timer = 0;

    if (++pwm_timer >= 64) {
        pwm_timer = 0;

        if (output_pwm_duty_cycle > 0)
            PORTB |= _BV(DC_MOTOR_PIN);
    }

    if (pwm_timer >= output_pwm_duty_cycle)
        PORTB &= ~_BV(DC_MOTOR_PIN);
}


/**
 *  @brief ISR associated to ADC conversion.
 *
 *  This function simply stores the ADC value in some temporary variable when
 *  the conversion is complete.  Since two ADC have to be read, the second
 *  conversion is started in here when the first conversion is complete.
 */
ISR (ADC_vect)
{
    if (adc_mux_selection == ADC_READ_THROTTLE) {
        adc_value_throttle = ADCH;
        adc_start_conversion(ADC_READ_BUTTONS);
    } else {
        adc_value_buttons = ADCH;
    }
}

/**
 *  @brief The main function where all the magic happens =)
 *  @return Exit status (never reached).
 */
int main(void)
{
    static uint16_t update_status_timer = 0;

    // Initialize hardware
    setup();

    while (1) {
        // Busy wait for a new trigger to process a new audio sample
        while (!update_audio_sample);

        // Update audio samples @ 8 kHz.
        uint8_t engine_speed = tractor_get_engine_speed();
        OCR1B = audio_get_next_sample(engine_speed);

        // Update tractor model @ 25 Hz
        if (++update_status_timer >= TRACTOR_MODEL_CYCLE) {
            update_status_timer = 0;

            // Manage button status
            button_set_adc_value(adc_value_buttons);
            if (button_is_clicked(BUTTON_HORN))
                tractor_play_dixie_song();

            if (button_is_pressed(BUTTON_START))
                tractor_set_ignition_position(IGNITION_START);
            else if (button_is_pressed(BUTTON_ON))
                tractor_set_ignition_position(IGNITION_ON);
            else
                tractor_set_ignition_position(IGNITION_OFF);

            // Update tractor model
            bool led_status;
            tractor_set_engine_speed_setpoint(ENGINE_SPEED_IDLE +
                    ((adc_value_throttle - ADC_THROTTLE_IDLE) >> 1));
            led_status = tractor_update_model();

            // Update outputs to slave ECU
            output_set_led(led_status);
            output_set_dc_motor_pwm(tractor_get_engine_speed());

            // Start ADC reading to have values ready on the next loop
            adc_start_conversion(ADC_READ_THROTTLE);
        }
        update_audio_sample = false;
    }
    return 0;                       // Never reached
}
