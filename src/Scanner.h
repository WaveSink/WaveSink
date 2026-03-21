#pragma once

#include <QObject>
#include <QMap>
#include <QMutex>
#include <windows.h>
#include <mmdeviceapi.h>

class Scanner : public QObject, public IMMNotificationClient
{
    Q_OBJECT

public:
    explicit Scanner(QObject *parent = nullptr);
    ~Scanner();

signals:
    void sinkAdded(const QString &id);
    void sinkRemoved(const QString &id);
    void sourceAdded(const QString &id);
    void sourceRemoved(const QString &id);

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

    void startListening();

    LONG m_refCount;
    IMMDeviceEnumerator *m_enumerator;
    QMap<QString, EDataFlow> m_deviceFlows;
    QMutex m_mutex;
};
