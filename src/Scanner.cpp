#include "Scanner.h"
#include <QDebug>
#include <functiondiscoverykeys_devpkey.h>

Scanner::Scanner(QObject *parent)
    : QObject(parent)
    , m_refCount(1)
    , m_enumerator(nullptr)
{
    CoInitialize(nullptr);

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&m_enumerator);

    if (SUCCEEDED(hr) && m_enumerator) {
        m_enumerator->RegisterEndpointNotificationCallback(this);
        startListening();
    } else {
        qDebug() << "Failed to initialize Scanner. HRESULT: " << hr;
    }
}

Scanner::~Scanner()
{
    if (m_enumerator) {
        m_enumerator->UnregisterEndpointNotificationCallback(this);
        m_enumerator->Release();
        m_enumerator = nullptr;
    }
    CoUninitialize();
}

STDMETHODIMP Scanner::QueryInterface(REFIID riid, void **ppvInterface)
{
    if (riid == __uuidof(IUnknown)) {
        *ppvInterface = static_cast<IUnknown*>(static_cast<IMMNotificationClient*>(this));
    } else if (riid == __uuidof(IMMNotificationClient)) {
        *ppvInterface = static_cast<IMMNotificationClient*>(this);
    } else {
        *ppvInterface = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) Scanner::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) Scanner::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_refCount);
    return ulRef;
}

STDMETHODIMP Scanner::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
    QString id = QString::fromWCharArray(pwstrDeviceId);
    QMutexLocker locker(&m_mutex);

    if (dwNewState == DEVICE_STATE_ACTIVE) {
        if (!m_deviceFlows.contains(id)) {
            IMMDevice *pDevice = nullptr;
            HRESULT hr = m_enumerator->GetDevice(pwstrDeviceId, &pDevice);
            if (SUCCEEDED(hr) && pDevice) {
                IMMEndpoint *pEndpoint = nullptr;
                hr = pDevice->QueryInterface(__uuidof(IMMEndpoint), (void**)&pEndpoint);
                if (SUCCEEDED(hr) && pEndpoint) {
                    EDataFlow flow;
                    if (SUCCEEDED(pEndpoint->GetDataFlow(&flow))) {
                        m_deviceFlows.insert(id, flow);
                        if (flow == eRender) {
                            emit sinkAdded(id);
                        } else if (flow == eCapture) {
                            emit sourceAdded(id);
                        }
                    }
                    pEndpoint->Release();
                }
                pDevice->Release();
            }
        }
    } else {
        if (m_deviceFlows.contains(id)) {
            EDataFlow flow = m_deviceFlows.take(id);
            if (flow == eRender) {
                emit sinkRemoved(id);
            } else if (flow == eCapture) {
                emit sourceRemoved(id);
            }
        }
    }
    return S_OK;
}

STDMETHODIMP Scanner::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
    return S_OK;
}

STDMETHODIMP Scanner::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
    QString id = QString::fromWCharArray(pwstrDeviceId);
    QMutexLocker locker(&m_mutex);
    if (m_deviceFlows.contains(id)) {
        EDataFlow flow = m_deviceFlows.take(id);
        if (flow == eRender) {
            emit sinkRemoved(id);
        } else if (flow == eCapture) {
            emit sourceRemoved(id);
        }
    }
    return S_OK;
}

STDMETHODIMP Scanner::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
{
    return S_OK;
}

STDMETHODIMP Scanner::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
    return S_OK;
}

void Scanner::startListening()
{
    if (!m_enumerator) return;

    IMMDeviceCollection *pCollection = nullptr;
    HRESULT hr = m_enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &pCollection);

    if (SUCCEEDED(hr) && pCollection) {
        UINT count = 0;
        pCollection->GetCount(&count);

        for (UINT i = 0; i < count; i++) {
            IMMDevice *pDevice = nullptr;
            if (SUCCEEDED(pCollection->Item(i, &pDevice)) && pDevice) {
                LPWSTR id = nullptr;
                if (SUCCEEDED(pDevice->GetId(&id)) && id) {
                    IMMEndpoint *pEndpoint = nullptr;
                    if (SUCCEEDED(pDevice->QueryInterface(__uuidof(IMMEndpoint), (void**)&pEndpoint)) && pEndpoint) {
                        EDataFlow flow;
                        if (SUCCEEDED(pEndpoint->GetDataFlow(&flow))) {
                            QString qId = QString::fromWCharArray(id);
                            QMutexLocker locker(&m_mutex);
                            if (!m_deviceFlows.contains(qId)) {
                                m_deviceFlows.insert(qId, flow);
                                if (flow == eRender) {
                                    emit sinkAdded(qId);
                                } else if (flow == eCapture) {
                                    emit sourceAdded(qId);
                                }
                            }
                        }
                        pEndpoint->Release();
                    }
                    CoTaskMemFree(id);
                }
                pDevice->Release();
            }
        }
        pCollection->Release();
    }
}
