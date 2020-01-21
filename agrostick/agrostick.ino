#include <EEPROM.h>
#include <HID.h>
#include <Mouse.h>

#include <string.h>

#ifndef _USING_HID
#error "Agrostick require a USB MCU with PluggableHID core enabled."
#endif

// #define DEBUG_AGROSTICK
#ifndef DEBUG_AGROSTICK
constexpr uint8_t SLOWDOWN_FACTOR {1};
#else
constexpr uint8_t SLOWDOWN_FACTOR {25};
#endif

class Agrostick
{
public:
    void begin() {
        initButton();
        initOutput();

        initJoystickDescriptor();
        Mouse.begin();

        m_mode = emulationMode();

#ifdef DEBUG_AGROSTICK
        Serial.begin(9600);
#endif
    }

    void readInputs() {
        for (uint8_t i = 0; i < AXIS_COUNT; ++i)
            readAxis(i);
        for (uint8_t i = 0; i < BUTTON_COUNT; ++i)
            readButton(i);
        checkModeSwitch();
    }

    void writeOutput() {
        digitalWrite(PIN_JOYSTCK_MODE, m_joystickEmulation ? HIGH : LOW);
        digitalWrite(PIN_MOUSE_MODE, m_joystickEmulation ? LOW : HIGH);
    }

    void sendReport() {
        sendJoystickReport();
        sendMouseReport();
        sendSerialDebugInfo();
    }

    int16_t nextTick() const {
        return 20 * SLOWDOWN_FACTOR;
    }

private:
    // ** Mode switch management (joystick <-> mouse) ** //
    static constexpr uint8_t SWITCH_MODE_TIMER {100 / SLOWDOWN_FACTOR};
    static constexpr uint8_t EEPROM_MAGIC_VALUE {0x44};

    enum class Mode {
        JOYSTICK,
        MOUSE,
    };
    Mode        m_mode;
    bool        m_joystickEmulation;
    uint8_t     m_switchModeCount {SWITCH_MODE_TIMER};

    Mode emulationMode() const {
        return (EEPROM[0] == EEPROM_MAGIC_VALUE) ? Mode::JOYSTICK : Mode::MOUSE;
    }

    void toggleMode() {
        m_mode = (m_mode == Mode::MOUSE) ? Mode::JOYSTICK : Mode::MOUSE;
        EEPROM[0] = (m_mode == Mode::JOYSTICK) ? EEPROM_MAGIC_VALUE : 0x00; 
    }

    void checkModeSwitch() {
        if (m_switchModeCount > 0 && m_button[0] && m_button[1] && m_button[2] && m_button[3]) {
            m_switchModeCount--;
            if (m_switchModeCount == 0)
                toggleMode();
        } else {
            m_switchModeCount = SWITCH_MODE_TIMER;
        }
    }

    // ** Joystick HID descriptor and report management ** //
    static constexpr uint8_t REPORT_ID {0x03};
    static constexpr uint8_t HID_REPORT_DESCRIPTOR[] PROGMEM {
        0x05, 0x01,         // USAGE_PAGE (Generic Desktop)
        0x09, 0x04,         // USAGE (Joystick: 0x04)
        0xA1, 0x01,         // COLLECTION (Application)
        0x85, REPORT_ID,    // REPORT_ID (Default: 0x03)
        0x05, 0x09,         // USAGE_PAGE (Button)
        0x19, 0x01,         // USAGE_MINIMUM (Button: 1)
        0x29, 0x07,         // USAGE_MAXIMUM (Button: 7)
        0x15, 0x00,         // LOGICAL_MINIMUM (0)
        0x25, 0x01,         // LOGICAL_MAXIMUM (1)
        0x75, 0x01,         // REPORT_SIZE (1)
        0x95, 0x08,         // REPORT_COUNT (Button + Spare: 8)
        0x55, 0x00,         // UNIT_EXPONENT (0)
        0x65, 0x00,         // UNIT (None)
        0x81, 0x02,         // INPUT (Data, Var, Abs)
        0x05, 0x01,         // USAGE_PAGE (Generic Desktop)
        0x09, 0x01,         // USAGE (Pointer)
        0x16, 0x01, 0x80,   // LOGICAL_MINIMUM (-32767)
        0x26, 0xFF, 0x7F,   // LOGICAL_MAXIMUM (+32767)
        0x75, 0x10,         // REPORT_SIZE (16)
        0x95, 0x03,         // REPORT_COUNT (Axes: 3)
        0xA1, 0x00,         // COLLECTION (Physical)
        0x09, 0x30,         // USAGE (X)
        0x09, 0x31,         // USAGE (Y)
        0x09, 0x32,         // USAGE (Z)
        0x81, 0x02,         // INPUT (Data, Var, Abs)
        0xC0,               // END_COLLECTION (Physical)
        0xC0,               // END_COLLECTION
    };

    void initJoystickDescriptor() {
        static HIDSubDescriptor node(HID_REPORT_DESCRIPTOR, sizeof(HID_REPORT_DESCRIPTOR));
        HID().AppendDescriptor(&node);
    }

    void sendJoystickReport() {
        static uint8_t hidReport[7];

        if (m_mode == Mode::JOYSTICK) {
            uint8_t index {0};
            uint8_t buttonBitmask {0};
            for (uint8_t i = 0; i < BUTTON_COUNT; ++i)
                buttonBitmask |= (m_button[i] << i);
            hidReport[index++] = buttonBitmask;

            for (uint8_t i = 0; i < AXIS_COUNT; ++i) {
                hidReport[index++] = static_cast<uint8_t>(m_axis[i]);
                hidReport[index++] = static_cast<uint8_t>(m_axis[i] >> 8);
            }
        } else {
            memset(hidReport, 0x00, sizeof(hidReport));
        }

        HID().SendReport(REPORT_ID, hidReport, sizeof(hidReport));
    }

    // ** Mouse emulation management ** //
    int8_t virtualMouseMovement(uint8_t index, int8_t range) const {
        return (m_mode == Mode::MOUSE) ? map(m_axis[index], -32767, 32767, -range, range) : 0;
    }

    uint8_t virtualMouseButton(uint8_t index) const {
        return (m_mode == Mode::MOUSE) ? m_button[index] : 0;
    }

    void sendMouseReport() {
        Mouse.move(virtualMouseMovement(0, 18), virtualMouseMovement(1, 18),
                virtualMouseMovement(2, 2));

        constexpr uint8_t MOUSE_BUTTON[3] {MOUSE_MIDDLE, MOUSE_LEFT, MOUSE_RIGHT};
        for (uint8_t i = 0; i < 3; ++i) {
            if (virtualMouseButton(i + 4)) {
                if (!Mouse.isPressed(MOUSE_BUTTON[i]))
                    Mouse.press(MOUSE_BUTTON[i]);
            } else {
                if (Mouse.isPressed(MOUSE_BUTTON[i]))
                    Mouse.release(MOUSE_BUTTON[i]);
            }
        }
    }

    // ** Analog axes management ** //
    struct AgrostickAxis {
        int16_t     minValue;
        int16_t     maxValue;
        int16_t     zeroValue;
        uint8_t     deadBand;
        bool        reversed;
        uint8_t     pin;
    };

    static constexpr uint8_t AXIS_COUNT {3};
    static constexpr AgrostickAxis AGROSTICK_AXIS[AXIS_COUNT] {
        {85, 935, 512, 30, false, A0},
        {85, 935, 515, 30, true, A1},
        {95, 925, 500, 30, false, A2},
    };

    int16_t     m_rawAxisAi[AXIS_COUNT] {};
    int16_t     m_axis[AXIS_COUNT] {};

    void readAxis(uint8_t index) {
        auto value = analogRead(AGROSTICK_AXIS[index].pin);
        m_rawAxisAi[index] = value;

        const int16_t zeroValue {AGROSTICK_AXIS[index].zeroValue};
        const int16_t deadBand {AGROSTICK_AXIS[index].deadBand};
        const int16_t lowMin {AGROSTICK_AXIS[index].minValue};
        const int16_t lowMax {zeroValue - deadBand};
        const int16_t highMin {zeroValue + deadBand};
        const int16_t highMax {AGROSTICK_AXIS[index].maxValue};
        value = constrain(value, lowMin, highMax);

        int16_t scaledValue;
        if (value >= lowMin && value <= lowMax)
            scaledValue = map(value, lowMin, lowMax, -32767, 0);
        else if (value >= highMin && value <= highMax)
            scaledValue = map(value, highMin, highMax, 0, 32767);
        else
            scaledValue = 0;

        m_axis[index] = AGROSTICK_AXIS[index].reversed ? -scaledValue : scaledValue;
    }

    // ** Digital buttons management ** //
    struct AgrostickButton {
        bool        reversed;
        bool        internalPullup;
        uint8_t     pin;
    };
    static constexpr uint8_t BUTTON_COUNT {7};
    static constexpr AgrostickButton AGROSTICK_BUTTON[BUTTON_COUNT] {
        {true, true, 2},
        {true, true, 3},
        {true, true, 4},
        {true, true, 5},
        {false, false, 6},
        {false, false, 7},
        {false, false, 8},
    };

    uint8_t     m_button[BUTTON_COUNT] {};
    uint8_t     m_oldButton[BUTTON_COUNT] {};

    void initButton() {
        for (uint8_t i = 0; i < BUTTON_COUNT; ++i)
            pinMode(AGROSTICK_BUTTON[i].pin,
                    AGROSTICK_BUTTON[i].internalPullup ? INPUT_PULLUP : INPUT);
    }

    void readButton(uint8_t index) {
        auto button = digitalRead(AGROSTICK_BUTTON[index].pin);

        if (AGROSTICK_BUTTON[index].reversed)
            button ^= 1;

        if (m_oldButton[index] == button)
            m_button[index] = button;
        m_oldButton[index] = button;
    }

    // ** Digital output management ** //
    static constexpr uint8_t PIN_JOYSTCK_MODE {11};
    static constexpr uint8_t PIN_MOUSE_MODE {12};

    void initOutput() {
        pinMode(PIN_JOYSTCK_MODE, OUTPUT);
        pinMode(PIN_MOUSE_MODE, OUTPUT);
    }

    // ** Debug ** //
    void sendSerialDebugInfo() {
#ifdef DEBUG_AGROSTICK
        char buffer[50];
        for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
            sprintf(buffer, "B%d: %d/%d", i, m_oldButton[i], m_button[i]);
            Serial.println(buffer);
        }

        for (uint8_t i = 0; i < AXIS_COUNT; ++i) {
            sprintf(buffer, "A%d: %d [raw: %d]", i, m_axis[i], m_rawAxisAi[i]);
            Serial.println(buffer);
        }
#endif
    }
};

constexpr uint8_t Agrostick::HID_REPORT_DESCRIPTOR[] PROGMEM;
constexpr Agrostick::AgrostickAxis Agrostick::AGROSTICK_AXIS[];
constexpr Agrostick::AgrostickButton Agrostick::AGROSTICK_BUTTON[];

Agrostick agrostick;

void setup() {
    agrostick.begin();
}

void loop() {
    static uint32_t nextTick = 0;
    if (millis() >= nextTick) {
        agrostick.readInputs();
        agrostick.writeOutput();
        agrostick.sendReport();

        nextTick += agrostick.nextTick();
    }
}
