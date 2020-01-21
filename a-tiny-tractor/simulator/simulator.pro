TEMPLATE    =   app

TARGET      =   simulator

QT          +=  widgets multimedia

CONFIG      +=  c++11

HEADERS     =   simulator.h \
                ../attiny/button_manager.h \
                ../attiny/sound_manager.h \
                ../attiny/tractor_model.h \
                ../attiny/engine_running.h \
                ../attiny/tractor_horn.h \

SOURCES     =   main.cpp \
                simulator.cpp \
                ../attiny/sound_manager.c \
                ../attiny/button_manager.c \
                ../attiny/tractor_model.c

FORMS       =   simulator.ui

INCLUDEPATH +=  ../attiny
