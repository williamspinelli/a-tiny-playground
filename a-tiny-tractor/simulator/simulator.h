#pragma once

#include <QWidget>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QIODevice>
#include <QTimer>

class AudioGenerator : public QIODevice
{
    Q_OBJECT

public:
    AudioGenerator(QObject *parent);

    void start();
    void stop();

    qint64 readData(char *data, qint64 maxSize);
    qint64 writeData(const char *data, qint64 maxSize);
    qint64 bytesAvailable() const;
};


namespace Ui {
    class Simulator;
}

class Simulator : public QWidget
{
    Q_OBJECT

public:
    Simulator(QWidget *parent = 0);
    ~Simulator();

private slots:
    void pushTimerExpired();

private:
    Ui::Simulator       *m_ui;
    QAudioOutput        *m_audioOutput;
    QIODevice           *m_output;
    AudioGenerator      *m_audioGenerator;
    QTimer              *m_pushTimer;
    char                *m_buffer;
    bool                m_ledStatus;

    void openAudioDevice();
    void closeAudioDevice();

    void setGuiStatus(bool status);

    void setLedStatus(bool status);
};
