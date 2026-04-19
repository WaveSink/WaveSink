#include "AudioRouter.h"
#include <QDebug>
#include <QThread>
#include <QList>
#include <vector>
#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Simple Biquad Filter
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    float process(float in) {
        float out = in * b0 + z1;
        z1 = in * b1 - out * a1 + z2;
        z2 = in * b2 - out * a2;
        return out;
    }
};

// ----------------------------------------------------------------------------
// Helper Structs
// ----------------------------------------------------------------------------

struct RenderClientContext {
    QString deviceId;
    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioRenderClient* renderClient = nullptr;
    UINT32 bufferSize = 0;
    bool valid = false;

    void cleanup() {
        if (renderClient) { renderClient->Release(); renderClient = nullptr; }
        if (audioClient) {
            audioClient->Stop();
            audioClient->Release();
            audioClient = nullptr;
        }
        if (device) { device->Release(); device = nullptr; }
        valid = false;
    }
};

// ----------------------------------------------------------------------------
// AudioRouter Implementation
// ----------------------------------------------------------------------------

AudioRouter::AudioRouter(QObject *parent)
    : QObject(parent)
    , m_thread(nullptr)
{
}

AudioRouter::~AudioRouter()
{
    stop();
}

void AudioRouter::addSink(const QString &sinkId)
{
    if (m_thread) {
        m_thread->addSink(sinkId);
    }
}

void AudioRouter::removeSink(const QString &sinkId)
{
    if (m_thread) {
        m_thread->removeSink(sinkId);
    }
}

bool AudioRouter::hasSink(const QString &sinkId) const
{
    if (m_thread) {
        return m_thread->hasSink(sinkId);
    }
    return false;
}

void AudioRouter::setEqualizer(const QString &sinkId, const QList<int> &eqValues)
{
    if (m_thread) {
        m_thread->setEqualizer(sinkId, eqValues);
    }
}

void AudioRouter::start()
{
    if (isRunning()) return;

    m_thread = new RouterThread(this);
    m_thread->start();
}

void AudioRouter::stop()
{
    if (m_thread) {
        m_thread->stop();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
}

bool AudioRouter::isRunning() const
{
    return (m_thread && m_thread->isRunning());
}

// ----------------------------------------------------------------------------
// RouterThread Implementation
// ----------------------------------------------------------------------------

AudioRouter::RouterThread::RouterThread(QObject *parent)
    : QThread(parent)
    , m_stopRequested(false)
    , m_sinksChanged(false)
{
}

AudioRouter::RouterThread::~RouterThread()
{
    stop();
    wait();
}

void AudioRouter::RouterThread::addSink(const QString &sinkId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_targetSinks.contains(sinkId)) {
        m_targetSinks.insert(sinkId);
        m_sinksChanged = true;
    }
}

void AudioRouter::RouterThread::removeSink(const QString &sinkId)
{
    QMutexLocker locker(&m_mutex);
    if (m_targetSinks.contains(sinkId)) {
        m_targetSinks.remove(sinkId);
        m_sinksChanged = true;
    }
}

bool AudioRouter::RouterThread::hasSink(const QString &sinkId) const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));
    return m_targetSinks.contains(sinkId);
}

void AudioRouter::RouterThread::setEqualizer(const QString &sinkId, const QList<int> &eqValues)
{
    QMutexLocker locker(&m_mutex);
    m_eqSettings.insert(sinkId, eqValues);
}

void AudioRouter::RouterThread::stop()
{
    m_stopRequested = true;
}

// Helper to toggle mute on a device
static void SetDeviceMute(IMMDeviceEnumerator* pEnumerator, const QString& devId, bool mute) {
    if (!pEnumerator) return;
    IMMDevice* pDev = nullptr;
    // Note: The caller must ensure COM is initialized
    HRESULT hr = pEnumerator->GetDevice(reinterpret_cast<LPCWSTR>(devId.utf16()), &pDev);
    if (SUCCEEDED(hr)) {
        IAudioEndpointVolume* pVol = nullptr;
        hr = pDev->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&pVol);
        if (SUCCEEDED(hr)) {
            pVol->SetMute(mute, nullptr);
            pVol->Release();
        }
        pDev->Release();
    }
}

void AudioRouter::RouterThread::run()
{
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        qWarning() << "AudioRouter: Failed to CoInitialize";
        return;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    WAVEFORMATEX* pwfx = nullptr;
    UINT32 packetLength = 0;
    UINT32 numFramesAvailable;
    BYTE* pData;
    DWORD flags;

    QList<RenderClientContext*> activeSinks;
    QSet<QString> activeTargetIds;

    // 1. Initialize System Loopback Capture
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, 
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    QString captureId;
    if (SUCCEEDED(hr)) {
        // Capture from default render device (System Audio)
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (SUCCEEDED(hr)) {
            LPWSTR wstrId = nullptr;
            pDevice->GetId(&wstrId);
            if (wstrId) {
                captureId = QString::fromWCharArray(wstrId);
                CoTaskMemFree(wstrId);
            }
        }
    }

    // Mute all active render devices initially to ensure no audio plays unless requested
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* pCollection = nullptr;
        if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection))) {
            UINT count = 0;
            pCollection->GetCount(&count);
            for (UINT i = 0; i < count; i++) {
                IMMDevice* pEndpoint = nullptr;
                if (SUCCEEDED(pCollection->Item(i, &pEndpoint))) {
                    LPWSTR wstrId = nullptr;
                    pEndpoint->GetId(&wstrId);
                    QString id = QString::fromWCharArray(wstrId);
                    CoTaskMemFree(wstrId);

                    if (id != captureId) {
                        IAudioEndpointVolume* pVol = nullptr;
                        if (SUCCEEDED(pEndpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&pVol))) {
                            pVol->SetMute(TRUE, nullptr);
                            pVol->Release();
                        }
                    }
                    pEndpoint->Release();
                }
            }
            pCollection->Release();
        }
    }

    // Ensure any sinks that are already targets are unmuted immediately
    if (SUCCEEDED(hr)) {
        QMutexLocker locker(&m_mutex);
        for (const QString& sinkId : m_targetSinks) {
            SetDeviceMute(pEnumerator, sinkId, false);
        }
    }

    if (SUCCEEDED(hr)) {
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    }

    if (SUCCEEDED(hr)) {
        hr = pAudioClient->GetMixFormat(&pwfx);
    }

    if (SUCCEEDED(hr)) {
        // Initialize loopback
        // REFTIMES_PER_SEC = 10000000 (100ns units)
        // Request 100ms buffer
        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 
                                      AUDCLNT_STREAMFLAGS_LOOPBACK, 
                                      1000000, 0, pwfx, nullptr);
    }

    if (SUCCEEDED(hr)) {
        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    }

    if (SUCCEEDED(hr)) {
        hr = pAudioClient->Start();
    }

    if (FAILED(hr)) {
        qWarning() << "AudioRouter: Failed to initialize loopback capture. hr=" << hr;
        // Cleanup initialization failure
        if (pCaptureClient) pCaptureClient->Release();
        if (pAudioClient) pAudioClient->Release();
        if (pDevice) pDevice->Release();
        if (pEnumerator) pEnumerator->Release();
        if (pwfx) CoTaskMemFree(pwfx);
        CoUninitialize();
        return;
    }

    // 2. Main Audio Loop
    while (!m_stopRequested) {
        
        // --- Manage Sink List ---
        if (m_sinksChanged) {
            QSet<QString> targets;
            {
                QMutexLocker locker(&m_mutex);
                targets = m_targetSinks;
                m_sinksChanged = false;
            }

            QSet<QString> removedTargets = activeTargetIds;
            removedTargets.subtract(targets);
            for (const QString& id : removedTargets) {
                if (id != captureId) {
                    SetDeviceMute(pEnumerator, id, true);
                }
            }
            activeTargetIds = targets;

            // Remove old sinks
            for (auto it = activeSinks.begin(); it != activeSinks.end(); ) {
                if (!targets.contains((*it)->deviceId)) {
                    (*it)->cleanup();
                    delete *it;
                    it = activeSinks.erase(it);
                } else {
                    ++it;
                }
            }

            // Add new sinks
            for (const QString& id : targets) {
                if (id == captureId) {
                    SetDeviceMute(pEnumerator, id, false);
                    qWarning() << "AudioRouter: Unmuting capture device as it is selected as a sink.";
                    continue;
                }

                bool exists = false;
                for (auto* ctx : activeSinks) {
                    if (ctx->deviceId == id) { exists = true; break; }
                }

                if (!exists) {
                    RenderClientContext* ctx = new RenderClientContext();
                    ctx->deviceId = id;

                    IMMDevice* sinkDevice = nullptr;
                    // Note: Using the enumerator we created earlier
                    HRESULT sHr = pEnumerator->GetDevice(reinterpret_cast<LPCWSTR>(id.utf16()), &sinkDevice);
                    
                    if (SUCCEEDED(sHr)) {
                        ctx->device = sinkDevice;
                        sHr = sinkDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&ctx->audioClient);
                    }

                    if (SUCCEEDED(sHr)) {
                        // Try to initialize with the SAME format as the capture.
                        // If the sink doesn't support the mix format of the source, this simple router fails for that sink.
                        // Complex routing requires a resampler (e.g. Media Foundation Transform).
                        sHr = ctx->audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 
                                                          0, // No special flags
                                                          1000000, 0, pwfx, nullptr);
                    }

                    if (SUCCEEDED(sHr)) {
                        sHr = ctx->audioClient->GetBufferSize(&ctx->bufferSize);
                    }

                    if (SUCCEEDED(sHr)) {
                        sHr = ctx->audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&ctx->renderClient);
                    }

                    if (SUCCEEDED(sHr)) {
                        sHr = ctx->audioClient->Start();
                    }

                    if (SUCCEEDED(sHr)) {
                        ctx->valid = true;
                        activeSinks.append(ctx);
                        // Unmute the device being added
                        SetDeviceMute(pEnumerator, id, false);
                        qDebug() << "AudioRouter: Added sink" << id;
                    } else {
                        qWarning() << "AudioRouter: Failed to initialize sink" << id << "hr=" << sHr;
                        ctx->cleanup();
                        delete ctx;
                    }
                }
            }
        }

        // --- Process Audio ---

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        
        if (SUCCEEDED(hr)) {
            if (packetLength == 0) {
                // No data yet, sleep briefly to yield
                QThread::msleep(1);
                continue;
            }

            // Read all available packets
            while (packetLength > 0) {
                hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
                
                if (FAILED(hr)) {
                    if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                        // Default device lost
                        m_stopRequested = true; 
                    }
                    break;
                }

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    // We could write silence, but usually safe to just write the zeros in pData (if valid) or handle specially.
                    // For simplicity, we write whatever payload we got or zero it out if explicit silent flag.
                    // Typically GetBuffer returns a pointer to a buffer. If silent, we should write silence to sinks.
                    // However, pData might be garbage if SILENT flag is set.
                }

                // Push to all sinks
                for (auto* ctx : activeSinks) {
                    if (!ctx->valid) continue;

                    // Check space available in render buffer
                    UINT32 padding = 0;
                    HRESULT sHr = ctx->audioClient->GetCurrentPadding(&padding);
                    
                    if (SUCCEEDED(sHr)) {
                        UINT32 framesAvailable = ctx->bufferSize - padding;
                        if (framesAvailable >= numFramesAvailable) {
                            BYTE* pRenderData = nullptr;
                            sHr = ctx->renderClient->GetBuffer(numFramesAvailable, &pRenderData);
                            if (SUCCEEDED(sHr)) {
                                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                                    memset(pRenderData, 0, numFramesAvailable * pwfx->nBlockAlign);
                                } else {
                                    if (pwfx->wBitsPerSample == 32) {
                                        float* pIn = reinterpret_cast<float*>(pData);
                                        float* pOut = reinterpret_cast<float*>(pRenderData);
                                        int numSamples = numFramesAvailable * pwfx->nChannels;
                                        
                                        // Retrieve current EQ settings for this sink
                                        QList<int> currentEq;
                                        {
                                            QMutexLocker locker(const_cast<QMutex*>(&m_mutex));
                                            currentEq = m_eqSettings.value(ctx->deviceId, {50, 50, 50, 50, 50});
                                        }
                                        
                                        // Simple scalar gain calculation to simulate EQ (Neutral at 50)
                                        // In a full DSP implementation, each band would filter specific frequencies.
                                        float avgEq = (currentEq[0] + currentEq[1] + currentEq[2] + currentEq[3] + currentEq[4]) / 250.0f;
                                        float gain = avgEq * 2.0f;

                                        for (int i = 0; i < numSamples; ++i) {
                                            pOut[i] = pIn[i] * gain;
                                        }
                                    } else {
                                        // Fallback to direct memory copy if format is not 32-bit float
                                        memcpy(pRenderData, pData, numFramesAvailable * pwfx->nBlockAlign);
                                    }
                                }
                                ctx->renderClient->ReleaseBuffer(numFramesAvailable, 0);
                            }
                        } else {
                            // Sink is backing up, drop this frame (glitch) to keep system sync
                            // or we could implement a ring buffer, but "Keep it very simple".
                        }
                    } else if (sHr == AUDCLNT_E_DEVICE_INVALIDATED) {
                        // Device removed
                        ctx->valid = false; // Mark for removal or re-init next cycle
                    }
                }

                hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
                if (FAILED(hr)) break;

                hr = pCaptureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) break;
            }
        } else {
            // GetNextPacketSize failed
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) m_stopRequested = true;
            else QThread::msleep(5);
        }
    }

    // 3. Cleanup
    
    // Cleanup sinks
    for (auto* ctx : activeSinks) {
        ctx->cleanup();
        delete ctx;
    }
    activeSinks.clear();

    // Cleanup capture
    if (pAudioClient) pAudioClient->Stop();
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    if (pwfx) CoTaskMemFree(pwfx);

    CoUninitialize();
}