#pragma once

#include <QObject>
#include <QString>
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

/**
 * @brief The AudioController class provides an interface to control
 * hardware audio properties of sink devices (Volume, Mute, State).
 */
class AudioController : public QObject
{
    Q_OBJECT

public:
    explicit AudioController(QObject *parent = nullptr);
    ~AudioController();

    /**
     * @brief Set the volume for a specific sink.
     * @param sinkId The device ID.
     * @param volume Normalized volume level (0.0 to 1.0).
     * @return True if successful.
     */
    bool setVolume(const QString &sinkId, float volume);

    /**
     * @brief Get the current volume of a sink.
     * @param sinkId The device ID.
     * @return Normalized volume level (0.0 to 1.0), or -1.0 if failed.
     */
    float getVolume(const QString &sinkId);

    /**
     * @brief Mute or Unmute a sink.
     * @param sinkId The device ID.
     * @param mute True to mute, False to unmute.
     * @return True if successful.
     */
    bool setMute(const QString &sinkId, bool mute);

    /**
     * @brief Check if a sink is muted.
     * @param sinkId The device ID.
     * @return True if muted, False if unmuted or failed.
     */
    bool getMute(const QString &sinkId);

    /**
     * @brief Check if a sink is currently active (plugged in and enabled).
     * @param sinkId The device ID.
     * @return True if active.
     */
    bool isSinkActive(const QString &sinkId);

private:
    // Helper to get the volume interface for a device ID
    IAudioEndpointVolume* getEndpointVolume(const QString &sinkId);
    
    // Helper to get the device interface
    IMMDevice* getDevice(const QString &sinkId);

    IMMDeviceEnumerator *m_enumerator;
};