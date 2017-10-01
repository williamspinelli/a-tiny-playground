#include "simulator.h"
#include "ui_simulator.h"

#include <QAudioDeviceInfo>
#include <QMessageBox>

extern "C" {
#include "../attiny/button_manager.h"
#include "../attiny/sound_manager.h"
#include "../attiny/tractor_model.h"
}

static auto DATA_SAMPLE_RATE_HZ = 8000;
static auto BUFFER_SIZE         = 4000;

static uint8_t ADC_LEVEL_OFF            = 0;
static uint8_t ADC_LEVEL_ON             = 56;
static uint8_t ADC_LEVEL_ON_HORN        = 70;
static uint8_t ADC_LEVEL_ON_START       = 128;
static uint8_t ADC_LEVEL_ON_START_HORN  = 245;

static uint8_t PWM_MIN  = 6;
static uint8_t PWM_MAX  = 58;


AudioGenerator::AudioGenerator(QObject *parent) : QIODevice{parent}
{
}

void AudioGenerator::start()
{
    open(QIODevice::ReadOnly);
}

void AudioGenerator::stop()
{
    close();
}

qint64 AudioGenerator::readData(char *data, qint64 maxSize)
{
    // override maxSize to avoid high latency in the reaction!
    maxSize = DATA_SAMPLE_RATE_HZ / 25;

    for (auto i = 0; i < maxSize; i++)
        data[i] = static_cast<char>(
                audio_get_next_sample(tractor_get_engine_speed()));

    return maxSize;
}

qint64 AudioGenerator::writeData(const char *data, qint64 maxSize)
{
    Q_UNUSED(data);
    Q_UNUSED(maxSize);

    return 0;
}

qint64 AudioGenerator::bytesAvailable() const
{
    return QIODevice::bytesAvailable();
}



Simulator::Simulator(QWidget *parent) : QWidget{parent},
    m_ui{new Ui::Simulator{}},
    m_audioOutput{0},
    m_audioGenerator{0},
    m_pushTimer{new QTimer{this}},
    m_buffer{new char[BUFFER_SIZE]},
    m_ledStatus{false}
{
    m_ui->setupUi(this);
    m_ui->progressBar_engineSpeed->setMaximum(ENGINE_SPEED_MAX);

    openAudioDevice();

    connect(m_pushTimer, &QTimer::timeout,
            this, &Simulator::pushTimerExpired);

    connect(m_ui->pushButton_ignition, &QPushButton::toggled,
            [=](bool checked) {
                button_set_adc_value(checked ?
                        ADC_LEVEL_ON : ADC_LEVEL_OFF);
                setGuiStatus(checked);
            });

    connect(m_ui->pushButton_ignition_start, &QPushButton::pressed,
            [=]() {
                button_set_adc_value(m_ui->pushButton_horn->isDown() ?
                        ADC_LEVEL_ON_START_HORN : ADC_LEVEL_ON_START);
            });

    connect(m_ui->pushButton_ignition_start, &QPushButton::released,
            [=]() {
                button_set_adc_value(m_ui->pushButton_horn->isDown() ?
                        ADC_LEVEL_ON_HORN : ADC_LEVEL_ON);
            });

    connect(m_ui->pushButton_horn, &QPushButton::pressed,
            [=]() {
                button_set_adc_value(m_ui->pushButton_ignition_start->isDown() ?
                        ADC_LEVEL_ON_START_HORN : ADC_LEVEL_ON_HORN);
            });

    connect(m_ui->pushButton_horn, &QPushButton::released,
            [=]() {
                button_set_adc_value(m_ui->pushButton_ignition_start->isDown() ?
                        ADC_LEVEL_ON_START : ADC_LEVEL_ON);
            });
}

Simulator::~Simulator()
{
    closeAudioDevice();
    delete[] m_buffer;
}

void Simulator::pushTimerExpired()
{
    // manage button levels
    if (button_is_clicked(BUTTON_HORN))
        tractor_play_dixie_song();

    if (button_is_pressed(BUTTON_START))
        tractor_set_ignition_position(IGNITION_START);
    else if (button_is_pressed(BUTTON_ON))
        tractor_set_ignition_position(IGNITION_ON);
    else
        tractor_set_ignition_position(IGNITION_OFF);

    // manage tractor model
    bool ledStatus = tractor_update_model();
    setLedStatus(ledStatus);

    auto setpoint{m_ui->horizontalSlider_throttle->value()};
    uint8_t engineSpeed{tractor_get_engine_speed()};

    tractor_set_engine_speed_setpoint(static_cast<uint8_t>(ENGINE_SPEED_IDLE +
            setpoint * 0.01 * (ENGINE_SPEED_MAX - ENGINE_SPEED_IDLE)));
    m_ui->progressBar_engineSpeed->setValue(engineSpeed);

    uint8_t dutyCycle;
    if (engineSpeed < ENGINE_SPEED_MIN) {
        dutyCycle = 0;
    } else {
        dutyCycle = PWM_MIN + ((engineSpeed - ENGINE_SPEED_IDLE) >> 1);
        if (dutyCycle > PWM_MAX)
            dutyCycle = PWM_MAX;
    }
    m_ui->progressBar_motorPwm->setValue(dutyCycle);

    if (m_audioOutput && m_audioOutput->state() != QAudio::StoppedState) {
        auto chunks{qMin(1, m_audioOutput->bytesFree() / m_audioOutput->periodSize())};

        while (chunks) {
            auto chunkSize{m_audioGenerator->read(m_buffer, m_audioOutput->periodSize())};
            if (chunkSize)
                m_output->write(m_buffer, chunkSize);
            --chunks;
        }
    }
}

void Simulator::openAudioDevice()
{
    QAudioFormat audioFormat;
    audioFormat.setSampleRate(DATA_SAMPLE_RATE_HZ);
    audioFormat.setChannelCount(1);
    audioFormat.setSampleSize(8);
    audioFormat.setCodec("audio/pcm");
    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    audioFormat.setSampleType(QAudioFormat::UnSignedInt);

    QAudioDeviceInfo defaultDeviceInfo{QAudioDeviceInfo::defaultOutputDevice()};
    if (!defaultDeviceInfo.isFormatSupported(audioFormat)) {
        QMessageBox::critical(this, windowTitle(), "Audio format is not supported");
        qFatal("Audio format is not supported");
    }

    if (m_audioGenerator)
        delete m_audioGenerator;
    m_audioGenerator = new AudioGenerator{this};
    m_audioGenerator->start();

    m_audioOutput = new QAudioOutput{defaultDeviceInfo, audioFormat, this};
    m_output = m_audioOutput->start();
    m_audioOutput->resume();

    m_pushTimer->start(40);
}

void Simulator::closeAudioDevice()
{
    m_audioOutput->suspend();
    m_audioGenerator->stop();
    m_audioOutput->stop();
}

void Simulator::setGuiStatus(bool status)
{
    m_ui->pushButton_ignition->setText(status ? "ON" : "OFF");
    m_ui->pushButton_ignition_start->setEnabled(status);
    m_ui->pushButton_horn->setEnabled(status);

    m_ui->frame_io->setEnabled(status);
    m_ui->horizontalSlider_throttle->setValue(
            m_ui->horizontalSlider_throttle->minimum());
}

void Simulator::setLedStatus(bool status)
{
    if (status != m_ledStatus) {
        m_ledStatus = status;
        m_ui->widget_led->setStyleSheet(
            QString("QWidget {"
            "border: 4px solid #aaa;"
            "border-radius: 20px;"
            "background-color: %1;"
            "}").arg(m_ledStatus ? "#fff" : "#ddd"));
    }
}
