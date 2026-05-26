#ifndef AUDIOLINK_H
#define AUDIOLINK_H

#pragma once

#include <audioclient.h>
#include <mmdeviceapi.h>


#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

class AudioSink
{
public:
    HRESULT CopyData(BYTE* Data, UINT32 DataSize, bool Status);

    HRESULT SetFormat(WAVEFORMATEX* format);

    HRESULT hr = S_OK;

};

class AudioLink
{
public:
    HRESULT RecordAudioStream(AudioSink* pMySink);

    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
    const IID IID_IAudioClient = __uuidof(IAudioClient);
    const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
};



#endif