#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QList>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>

class SessionEvents;

class Scanner : public QObject, public IMMNotificationClient, public IAudioSessionNotification
{
    Q_OBJECT

public:
    explicit Scanner(QObject *parent = nullptr);
    ~Scanner();

    // Returns ID -> Name map
    QMap<QString, QString> getSinks() {
        QMutexLocker locker(&m_mutex);
        QMap<QString, QString> map;
        for (auto it = m_deviceFlows.begin(); it != m_deviceFlows.end(); ++it) {
            if (it.value() == eRender) {
                // If name isn't found, fallback to ID
                map.insert(it.key(), m_deviceNames.value(it.key(), it.key()));
            }
        }
        return map;
    }

    // Returns ID -> Name map
    QMap<QString, QString> getSources() {
        QMutexLocker locker(&m_mutex);
        QMap<QString, QString> map;
        for (auto it = m_deviceFlows.begin(); it != m_deviceFlows.end(); ++it) {
            if (it.value() == eCapture) {
                map.insert(it.key(), m_deviceNames.value(it.key(), it.key()));
            }
        }
        // Add active application sessions
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
             map.insert(it.key(), m_deviceNames.value(it.key(), it.key()));
        }
        return map;
    }

    // Returns FormFactor (EndpointFormFactor enum value)
    uint getFormFactor(const QString &id) {
        QMutexLocker locker(&m_mutex);
        return m_deviceFormFactors.value(id, 10); // 10 = UnknownFormFactor
    }

signals:
    void sinkAdded(const QString &id);
    void sinkRemoved(const QString &id);
    void sourceAdded(const QString &id);
    void sourceRemoved(const QString &id);

private slots:
    // Internal slots for handling events on the main thread to avoid COM deadlocks
    void handleDeviceStateChanged(const QString &id, unsigned long state);
    void handleDeviceRemoved(const QString &id);
    void handleSessionRemoved(const QString &id);

private:
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvInterface) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IMMNotificationClient methods
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    STDMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    STDMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

    // IAudioSessionNotification methods
    STDMETHODIMP OnSessionCreated(IAudioSessionControl *NewSession) override;

    void initialize();
    void cleanup();

    void setupSessionMonitoring(const QString &deviceId);

    LONG m_refCount;
    IMMDeviceEnumerator *m_enumerator;

    // Device ID -> Flow
    QMap<QString, EDataFlow> m_deviceFlows;

    // Device ID -> Name (Friendly Name)
    QMap<QString, QString> m_deviceNames;

    // Device ID -> Form Factor
    QMap<QString, uint> m_deviceFormFactors;

    // Device ID -> Session Manager
    QMap<QString, IAudioSessionManager2*> m_sessionManagers;

    // Session ID -> Event Listener
    QMap<QString, SessionEvents*> m_sessions;

    QMutex m_mutex;
    friend class SessionEvents;
};
