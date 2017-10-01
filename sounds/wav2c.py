# !/usr/bin/env python
# -*- coding: utf-8 -*-
import os.path
import sys
import wave


def wav_to_c(wav_filename, code_filename, array_name):
    with wave.open(wav_filename, 'r') as wav_file, \
            open(code_filename, 'w') as code_file:
        length = wav_file.getnframes()

        if wav_file.getnchannels() != 1:
            print("Error: wav file must be a MONO track!")
            sys.exit(1)

        if wav_file.getsampwidth() != 1:
            print("Error: wav sample must be a 8 bits!")
            sys.exit(1)

        if wav_file.getframerate() != 8000:
            print("Error: wav sample rate must be 8 kHz!")
            sys.exit(1)

        code_file.write("""/* python3 wav2c.py {0} {1} {2} */
#ifndef {2}_H
#define {2}_H

#include <stdint.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PROGMEM
#endif

#define {2}_SIZE {3}
static const uint8_t {2}[{2}_SIZE] PROGMEM = {{
""".format(
            wav_filename,
            code_filename,
            array_name,
            length))

        row_count = 0
        for i in range(0, length):
            wav_data = wav_file.readframes(1)[0]
            code_file.write(str(wav_data) + ", ")
            row_count += 1
            if row_count >= 16:
                row_count = 0
                code_file.write('\n')
        code_file.write("""};
#endif
""")


if __name__ == "__main__":
    USAGE = """wav2c.py - Convert a 8KHz, 8bit, mono file to a C header file
    usage: python3 wav2c.py sound_file.wav code_file.h var_name"""

    if len(sys.argv) != 4:
        print(USAGE)
        sys.exit(1)

    if not os.path.isfile(sys.argv[1]):
        print("Error: wav file does not exist!")
        sys.exit(1)

    wav_to_c(sys.argv[1], sys.argv[2], sys.argv[3])
