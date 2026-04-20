#include "AudioController.h"
#include <QDebug>

AudioController::AudioController(QObject *parent)
    : QObject(parent)
    , m_enumerator(nullptr)
{
    HRESULT hr = CoInitialize(nullptr);
    // S_FALSE means already initialized, which is fine
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
            (void**)&m_enumerator);

        if (FAILED(hr)) {
            qWarning() << "AudioController: Failed to create device enumerator. hr=" << hr;
        }
    } else {
        qWarning() << "AudioController: Failed to CoInitialize. hr=" << hr;
    }
}

AudioController::~AudioController()
{
    if (m_enumerator) {
        m_enumerator->Release();
        m_enumerator = nullptr;
    }
    CoUninitialize();
}

IMMDevice* AudioController::getDevice(const QString &sinkId)
{
    if (!m_enumerator) return nullptr;

    IMMDevice *pDevice = nullptr;
    HRESULT hr = m_enumerator->GetDevice(reinterpret_cast<LPCWSTR>(sinkId.utf16()), &pDevice);

    if (FAILED(hr)) {
        qWarning() << "AudioController: Failed to get device" << sinkId << "hr=" << hr;
        return nullptr;
    }
    return pDevice;
}

IAudioEndpointVolume* AudioController::getEndpointVolume(const QString &sinkId)
{
    IMMDevice *pDevice = getDevice(sinkId);
    if (!pDevice) return nullptr;

    IAudioEndpointVolume *pVol = nullptr;
    HRESULT hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&pVol);

    pDevice->Release();

    if (FAILED(hr)) {
        qWarning() << "AudioController: Failed to activate endpoint volume for" << sinkId << "hr=" << hr;
        return nullptr;
    }
    return pVol;
}

bool AudioController::setVolume(const QString &sinkId, float volume)
{
    // Clamp volume
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    IAudioEndpointVolume *pVol = getEndpointVolume(sinkId);
    if (!pVol) return false;

    HRESULT hr = pVol->SetMasterVolumeLevelScalar(volume, nullptr);
    pVol->Release();

    return SUCCEEDED(hr);
}

float AudioController::getVolume(const QString &sinkId)
{
    IAudioEndpointVolume *pVol = getEndpointVolume(sinkId);
    if (!pVol) return -1.0f;

    float currentVol = 0.0f;
    HRESULT hr = pVol->GetMasterVolumeLevelScalar(&currentVol);
    pVol->Release();

    if (FAILED(hr)) return -1.0f;
    return currentVol;
}

bool AudioController::setMute(const QString &sinkId, bool mute)
{
    IAudioEndpointVolume *pVol = getEndpointVolume(sinkId);
    if (!pVol) return false;

    HRESULT hr = pVol->SetMute(mute ? TRUE : FALSE, nullptr);
    pVol->Release();

    return SUCCEEDED(hr);
}

bool AudioController::getMute(const QString &sinkId)
{
    IAudioEndpointVolume *pVol = getEndpointVolume(sinkId);
    if (!pVol) return false;

    BOOL mute = FALSE;
    HRESULT hr = pVol->GetMute(&mute);
    pVol->Release();

    if (FAILED(hr)) return false;
    return (mute != FALSE);
}

bool AudioController::isSinkActive(const QString &sinkId)
{
    IMMDevice *pDevice = getDevice(sinkId);
    if (!pDevice) return false;

    DWORD state = DEVICE_STATE_NOTPRESENT;
    HRESULT hr = pDevice->GetState(&state);
    pDevice->Release();

    if (FAILED(hr)) return false;
    return (state == DEVICE_STATE_ACTIVE);
}
