#include "Scanner.h"
#include <QDebug>
#include <QMetaObject>
#include <functiondiscoverykeys_devpkey.h>
#include <audiopolicy.h>

// Helper class for monitoring individual audio sessions
class SessionEvents : public IAudioSessionEvents {
public:
    SessionEvents(Scanner* parent, IAudioSessionControl* control, const QString& id) 
        : m_parent(parent), m_control(control), m_id(id), m_refCount(1) {
        if (m_control) m_control->AddRef();
    }

    virtual ~SessionEvents() {
        if (m_control) m_control->Release();
    }

    void shutdown() {
        if (m_control) {
            m_control->UnregisterAudioSessionNotification(this);
            // We do not release m_control here, destructor does it. 
            // We just stop listening.
        }
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioSessionEvents)) {
            *ppv = static_cast<IAudioSessionEvents*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG ulRef = InterlockedDecrement(&m_refCount);
        if (0 == ulRef) delete this;
        return ulRef;
    }

    // IAudioSessionEvents
    STDMETHODIMP OnStateChanged(AudioSessionState NewState) override {
        if (NewState == AudioSessionStateActive) {
            QMetaObject::invokeMethod(m_parent, "sourceAdded", Qt::QueuedConnection, Q_ARG(QString, m_id));
        } else if (NewState == AudioSessionStateInactive || NewState == AudioSessionStateExpired) {
            QMetaObject::invokeMethod(m_parent, "sourceRemoved", Qt::QueuedConnection, Q_ARG(QString, m_id));
        }
        return S_OK;
    }

    STDMETHODIMP OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason) override {
        // Report removal
        QMetaObject::invokeMethod(m_parent, "sourceRemoved", Qt::QueuedConnection, Q_ARG(QString, m_id));
        // Trigger cleanup on main thread
        QMetaObject::invokeMethod(m_parent, "handleSessionRemoved", Qt::QueuedConnection, Q_ARG(QString, m_id));
        return S_OK;
    }

    STDMETHODIMP OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext) override { return S_OK; }
    STDMETHODIMP OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext) override { return S_OK; }
    STDMETHODIMP OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext) override { return S_OK; }
    STDMETHODIMP OnChannelVolumeChanged(DWORD ChannelCount, float *NewChannelVolumeArray, DWORD ChangedChannel, LPCGUID EventContext) override { return S_OK; }
    STDMETHODIMP OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext) override { return S_OK; }

private:
    Scanner* m_parent;
    IAudioSessionControl* m_control;
    QString m_id;
    LONG m_refCount;
};

// Scanner Implementation

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
        initialize();
    } else {
        qDebug() << "Failed to initialize Scanner. HRESULT: " << hr;
    }
}

Scanner::~Scanner()
{
    cleanup();
    if (m_enumerator) {
        m_enumerator->UnregisterEndpointNotificationCallback(this);
        m_enumerator->Release();
        m_enumerator = nullptr;
    }
    CoUninitialize();
}

void Scanner::initialize()
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
                    // Manually trigger the "Add" logic for existing devices
                    QString qId = QString::fromWCharArray(id);
                    handleDeviceStateChanged(qId, DEVICE_STATE_ACTIVE);
                    CoTaskMemFree(id);
                }
                pDevice->Release();
            }
        }
        pCollection->Release();
    }
}

void Scanner::cleanup()
{
    // Snapshot collections to iterate safely without holding lock during COM calls
    QList<SessionEvents*> sessionsToRelease;
    QList<IAudioSessionManager2*> managersToRelease;
    
    {
        QMutexLocker locker(&m_mutex);
        sessionsToRelease = m_sessions.values();
        m_sessions.clear();
        
        managersToRelease = m_sessionManagers.values();
        m_sessionManagers.clear();
        m_deviceFlows.clear();
    }

    // Perform COM cleanup without lock
    for (auto* session : sessionsToRelease) {
        session->shutdown();
        session->Release();
    }

    for (auto* manager : managersToRelease) {
        manager->UnregisterSessionNotification(this);
        manager->Release();
    }
}

// IUnknown
STDMETHODIMP Scanner::QueryInterface(REFIID riid, void **ppvInterface)
{
    if (riid == __uuidof(IUnknown)) {
        *ppvInterface = static_cast<IUnknown*>(static_cast<IMMNotificationClient*>(this));
    } else if (riid == __uuidof(IMMNotificationClient)) {
        *ppvInterface = static_cast<IMMNotificationClient*>(this);
    } else if (riid == __uuidof(IAudioSessionNotification)) {
        *ppvInterface = static_cast<IAudioSessionNotification*>(this);
    } else {
        *ppvInterface = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) Scanner::AddRef() { return InterlockedIncrement(&m_refCount); }
STDMETHODIMP_(ULONG) Scanner::Release() { return InterlockedDecrement(&m_refCount); }

// IMMNotificationClient
STDMETHODIMP Scanner::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
    // Offload to main thread to avoid blocking COM thread or deadlocks
    QString id = QString::fromWCharArray(pwstrDeviceId);
    QMetaObject::invokeMethod(this, "handleDeviceStateChanged", Qt::QueuedConnection, 
                              Q_ARG(QString, id), 
                              Q_ARG(unsigned long, dwNewState));
    return S_OK;
}

STDMETHODIMP Scanner::OnDeviceAdded(LPCWSTR pwstrDeviceId) { return S_OK; }

STDMETHODIMP Scanner::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
    // Offload to main thread
    QString id = QString::fromWCharArray(pwstrDeviceId);
    QMetaObject::invokeMethod(this, "handleDeviceRemoved", Qt::QueuedConnection, Q_ARG(QString, id));
    return S_OK;
}

STDMETHODIMP Scanner::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) { return S_OK; }
STDMETHODIMP Scanner::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) { return S_OK; }

// Main thread slots

void Scanner::handleDeviceStateChanged(const QString &id, unsigned long state)
{
    if (state == DEVICE_STATE_ACTIVE) {
        // Check if already registered
        {
            QMutexLocker locker(&m_mutex);
            if (m_deviceFlows.contains(id)) return;
        }

        // Determine Flow
        EDataFlow flow = eAll;
        bool flowFound = false;

        IMMDevice *pDevice = nullptr;
        HRESULT hr = m_enumerator->GetDevice((LPCWSTR)id.utf16(), &pDevice);
        if (SUCCEEDED(hr) && pDevice) {
            IMMEndpoint *pEndpoint = nullptr;
            if (SUCCEEDED(pDevice->QueryInterface(__uuidof(IMMEndpoint), (void**)&pEndpoint))) {
                if (SUCCEEDED(pEndpoint->GetDataFlow(&flow))) {
                    flowFound = true;
                }
                pEndpoint->Release();
            }
            pDevice->Release();
        }

        if (flowFound) {
            // Update map and emit signal
            {
                QMutexLocker locker(&m_mutex);
                m_deviceFlows.insert(id, flow);
            }
            
            if (flow == eRender) emit sinkAdded(id);
            else if (flow == eCapture) emit sourceAdded(id);

            // Setup session monitoring for sinks
            if (flow == eRender) {
                setupSessionMonitoring(id);
            }
        }

    } else {
        // Not active (Disabled, Unplugged, NotPresent)
        handleDeviceRemoved(id);
    }
}

void Scanner::handleDeviceRemoved(const QString &id)
{
    EDataFlow flow = eAll;
    bool found = false;
    IAudioSessionManager2* mgr = nullptr;

    {
        QMutexLocker locker(&m_mutex);
        if (m_deviceFlows.contains(id)) {
            flow = m_deviceFlows.take(id);
            found = true;
        }
        if (m_sessionManagers.contains(id)) {
            mgr = m_sessionManagers.take(id);
        }
    }

    if (found) {
        if (flow == eRender) emit sinkRemoved(id);
        else if (flow == eCapture) emit sourceRemoved(id);
    }

    if (mgr) {
        // Release COM object outside lock to prevent deadlock
        mgr->UnregisterSessionNotification(this);
        mgr->Release();
    }
}

void Scanner::handleSessionRemoved(const QString &sessionId)
{
    SessionEvents* events = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        if (m_sessions.contains(sessionId)) {
            events = m_sessions.take(sessionId);
        }
    }
    
    if (events) {
        events->shutdown();
        events->Release();
    }
}

void Scanner::setupSessionMonitoring(const QString &deviceId)
{
    IMMDevice *pDevice = nullptr;
    HRESULT hr = m_enumerator->GetDevice((LPCWSTR)deviceId.utf16(), &pDevice);
    if (SUCCEEDED(hr) && pDevice) {
        IAudioSessionManager2 *pManager = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pManager);
        if (SUCCEEDED(hr) && pManager) {
            
            // Register for future sessions
            pManager->RegisterSessionNotification(this);
            
            {
                QMutexLocker locker(&m_mutex);
                m_sessionManagers.insert(deviceId, pManager);
            }

            // Enumerate existing sessions
            IAudioSessionEnumerator *pSessionEnum = nullptr;
            if (SUCCEEDED(pManager->GetSessionEnumerator(&pSessionEnum))) {
                int count = 0;
                pSessionEnum->GetCount(&count);
                for (int i = 0; i < count; i++) {
                    IAudioSessionControl *pControl = nullptr;
                    if (SUCCEEDED(pSessionEnum->GetSession(i, &pControl))) {
                        // OnSessionCreated handles internal locking
                        OnSessionCreated(pControl);
                        pControl->Release();
                    }
                }
                pSessionEnum->Release();
            }
        }
        pDevice->Release();
    }
}

// IAudioSessionNotification
STDMETHODIMP Scanner::OnSessionCreated(IAudioSessionControl *NewSession)
{
    if (!NewSession) return E_POINTER;

    IAudioSessionControl2 *pControl2 = nullptr;
    if (SUCCEEDED(NewSession->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pControl2))) {
        DWORD pid = 0;
        pControl2->GetProcessId(&pid);
        
        if (pid != 0) {
            QString sessionId = QString("App:%1").arg(pid);
            
            QMutexLocker locker(&m_mutex);
            if (!m_sessions.contains(sessionId)) {
                SessionEvents *events = new SessionEvents(this, NewSession, sessionId);
                HRESULT hr = NewSession->RegisterAudioSessionNotification(events);
                
                if (SUCCEEDED(hr)) {
                    m_sessions.insert(sessionId, events);
                    AudioSessionState state;
                    if (SUCCEEDED(NewSession->GetState(&state)) && state == AudioSessionStateActive) {
                        // Signal is thread-safe
                        emit sourceAdded(sessionId);
                    }
                } else {
                    events->Release();
                }
            }
        }
        pControl2->Release();
    }
    return S_OK;
}