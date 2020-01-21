# A Tiny Tractor

## Summary
This is an ATtiny85 project that generates some audio and visual effects for a
tractor model.

It is intended to work on a tractor model (see the demo video), that includes:
 * an ignition key (with OFF, ON and START position), that controls the engine
   status
 * a HORN button, that plays "Dixie" using a horn sound
 * a THROTTLE potentiometer, that controls the engine speed
 * a LED light, that blinks periodically
 * a DC motor, that turns at a speed proportional to the engine speed

The core of the project is the sound generation.  It uses a quick and dirty
looping technique to roughly generate engine sound effects with variable
speed.  It also allow to play three different kind of horn songs (including the
famous "Dixie" song).  The details of the audio playback are described in the
file sound_manager.h

Note that this is probably neither the most effective way to playback a given
soundwave on ATtiny, nor the one with the highest fidelity, since it doesn't
use any real soundwave interpolation.  It was done in this way since I wanted
to experiment with this particular approach.

## Schematic
The software is designed to work with the circuit shown in the schematic
directory, that uses the following pinout
 *  Pin 1: Not used
 *  Pin 2: Throttle Input
 *  Pin 3: Sound Output
 *  Pin 4: GND
 *  Pin 5: LED Output
 *  Pin 6: DC MOTOR Output
 *  Pin 7: Resistive network (Buttons ON, START, HORN)
 *  Pin 8: Vcc

## Demo Video
You can find a demo video here:

## Some final remarks...
The software is tailored to an ATtiny85, but it can be customized to work on
any other ATtiny MCU provided that it has enough flash.

All the software modules are thoroughly documented using Doxygen.
