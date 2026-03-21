#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>
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

    QList<QString> getSinks() {
        QMutexLocker locker(&m_mutex);
        QList<QString> list;
        for (auto it = m_deviceFlows.begin(); it != m_deviceFlows.end(); ++it) {
            if (it.value() == eRender) list.append(it.key());
        }
        return list;
    }

    QList<QString> getSources() {
        QMutexLocker locker(&m_mutex);
        QList<QString> list;
        for (auto it = m_deviceFlows.begin(); it != m_deviceFlows.end(); ++it) {
            if (it.value() == eCapture) list.append(it.key());
        }
        list.append(m_sessions.keys());
        return list;
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
    
    // Device ID -> Session Manager
    QMap<QString, IAudioSessionManager2*> m_sessionManagers;
    
    // Session ID -> Event Listener
    QMap<QString, SessionEvents*> m_sessions;

    QMutex m_mutex;
    friend class SessionEvents;
};