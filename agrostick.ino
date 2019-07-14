#include <HID.h>

#ifndef _USING_HID
#error "Agrostick require a USB MCU with PluggableHID core enabled."
#endif

// #define DEBUG_AGROSTICK

class Agrostick
{
public:
    void begin() {
        static HIDSubDescriptor node(HID_REPORT_DESCRIPTOR, sizeof(HID_REPORT_DESCRIPTOR));
        HID().AppendDescriptor(&node);

#ifdef DEBUG_AGROSTICK
        Serial.begin(9600);
#endif
    }

    void readInputs() {
        for (uint8_t i = 0; i < AXIS_COUNT; ++i)
            readAxis(i);
        for (uint8_t i = 0; i < BUTTON_COUNT; ++i)
            readButton(i);
    }

    void sendReport() {
        static uint8_t hidReport[7];
        uint8_t index {0};

        uint8_t buttonBitmask {0};
        for (uint8_t i = 0; i < BUTTON_COUNT; ++i)
            buttonBitmask |= (m_button[i] << i);
        hidReport[index++] = buttonBitmask;

        for (uint8_t i = 0; i < AXIS_COUNT; ++i) {
            hidReport[index++] = static_cast<uint8_t>(m_axis[i]);
            hidReport[index++] = static_cast<uint8_t>(m_axis[i] >> 8);
        }

        HID().SendReport(REPORT_ID, hidReport, sizeof(hidReport));
        sendSerialDebugInfo();
    }

    int16_t nextTick() const {
#ifndef DEBUG_AGROSTICK
        return 20;          // 50Hz
#else
        return 500;         // 2Hz
#endif
    }

private:
    // ** HID descriptor details ** //
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
        {0, 1023, 512, 0, false, A0},
        {0, 1023, 512, 0, false, A1},
        {0, 1023, 512, 0, false, A2},
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
        int16_t     threshold;
        bool        reversed;
        uint8_t     pin;
    };
    static constexpr uint8_t BUTTON_COUNT {7};
    static constexpr AgrostickButton AGROSTICK_BUTTON[BUTTON_COUNT] {
        {512, false, A3},
        {512, false, A4},
        {512, false, A5},
        {512, false, A6},
        {512, false, A7},
        {512, false, A8},
        {512, false, A9},
    };

    int16_t     m_rawButtonAi[BUTTON_COUNT] {};
    uint8_t     m_button[BUTTON_COUNT] {};
    uint8_t     m_oldButton[BUTTON_COUNT] {};

    void readButton(uint8_t index) {
        auto value = analogRead(AGROSTICK_BUTTON[index].pin);
        m_rawButtonAi[index] = value;

        uint8_t button {static_cast<uint8_t>(((value > AGROSTICK_BUTTON[index].threshold) ? 1 : 0))};
        if (AGROSTICK_BUTTON[index].reversed)
            button ^= 1;

        if (m_oldButton[index] == button)
            m_button[index] = button;
        m_oldButton[index] = button;
    }

    void sendSerialDebugInfo() {
#ifdef DEBUG_AGROSTICK
        char buffer[50];
        for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
            sprintf(buffer, "Button #%d: %d/%d [raw: %d]", i, m_oldButton[i], m_button[i], m_rawButtonAi[i]);
            Serial.println(buffer);
        }

        for (uint8_t i = 0; i < AXIS_COUNT; ++i) {
            sprintf(buffer, "Axis #%d: %d [raw: %d]", i, m_axis[i], m_rawAxisAi[i]);
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
        agrostick.sendReport();
        nextTick += agrostick.nextTick();
    }
}
