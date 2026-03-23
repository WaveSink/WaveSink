#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QSet>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

/**
 * @brief The AudioRouter class captures system audio (loopback) and routes it 
 * to a list of specified sink devices.
 */
class AudioRouter : public QObject
{
    Q_OBJECT

public:
    explicit AudioRouter(QObject *parent = nullptr);
    virtual ~AudioRouter();

    /**
     * @brief Add a sink device to the router.
     * The system audio will be played out to this device.
     * @param sinkId The device ID string (from Scanner).
     */
    void addSink(const QString &sinkId);

    /**
     * @brief Remove a sink device from the router.
     * @param sinkId The device ID string.
     */
    void removeSink(const QString &sinkId);

    /**
     * @brief Start routing audio.
     * Begins capturing system loopback and playing to all added sinks.
     */
    void start();

    /**
     * @brief Stop routing audio.
     */
    void stop();

    /**
     * @brief Returns true if the router is currently active.
     */
    bool isRunning() const;

private:
    // Internal thread class to handle the real-time WASAPI audio loop
    class RouterThread : public QThread
    {
    public:
        RouterThread(QObject *parent = nullptr);
        ~RouterThread();

        void addSink(const QString &sinkId);
        void removeSink(const QString &sinkId);
        void stop();

    protected:
        void run() override;

    private:
        QMutex m_mutex;
        bool m_stopRequested;
        QSet<QString> m_targetSinks;
        bool m_sinksChanged; // Flag to indicate changes needed handling in the loop
    };

    RouterThread *m_thread;
};