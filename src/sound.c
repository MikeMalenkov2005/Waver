#include <Windows.h>

#define SOUND_RIFF_SIGNATURE  0x46464952
#define SOUND_WAVE_SIGNATURE  0x45564157
#define SOUND_FMT_SIGNATURE   0x20746D66
#define SOUND_DATA_SIGNATURE  0x61746164

typedef struct HSOUNDPLAYER__ *HSOUNDPLAYER;
typedef struct HSOUND__ *HSOUND;

struct HSOUNDPLAYER__ {
  HANDLE hMutex;
  HANDLE hThread;
  HSOUND hSoundList;
  LPWAVEHDR lpWaveHeaders;
  DWORD dwSampleRate;
  DWORD dwQueueSize;
  DWORD dwThreadId;
};

struct HSOUND__ {
  HSOUND hNextSound;
  HSOUND hPrevSound;
  HSOUNDPLAYER hPlayer;
  LPCWAVEFORMATEX lpWaveFormat;
  LPCVOID lpSoundBuffer;
  DWORD dwBufferLength;
  DWORD dwSampleIndex;
  DWORD dwSubsample;
  BOOL bPaused;
  BOOL bLoop;
};

DWORD TakeSample(HSOUND hSound, DWORD dwSampleRate) {
  if (!hSound || !dwSampleRate || hSound->bPaused || !hSound->lpSoundBuffer) return 0;
  DWORD dwSample = 0;
  DWORD dwSampleIncrement = hSound->lpWaveFormat->nSamplesPerSec / dwSampleRate;
  DWORD dwSubsampleIncrement = ((DWORD64)1 << 32) / (hSound->lpWaveFormat->nSamplesPerSec % dwSampleRate);
  DWORD dwNewSubsample = hSound->dwSubsample + dwSubsampleIncrement;
  if (dwNewSubsample < hSound->dwSubsample) ++dwSampleIncrement;
  hSound->dwSubsample = dwNewSubsample;
  switch (hSound->lpWaveFormat->wFormatTag) {
  case WAVE_FORMAT_PCM:
    if (hSound->lpWaveFormat->wBitsPerSample == 8) {
      LPCBYTE lpSamples = hSound->lpSoundBuffer;
      dwSample |= (DWORD)(lpSamples[hSound->dwSampleIndex * hSound->lpWaveFormat->nBlockAlign] - 0x80) << 24;
      dwSample |= (DWORD)(lpSamples[hSound->dwSampleIndex * hSound->lpWaveFormat->nBlockAlign + hSound->lpWaveFormat->nChannels - 1] - 0x80) << 8;
    }
    else if (hSound->lpWaveFormat->wBitsPerSample == 16) {
      CONST WORD* lpSamples = hSound->lpSoundBuffer;
      dwSample |= (DWORD)lpSamples[hSound->dwSampleIndex * hSound->lpWaveFormat->nBlockAlign] << 16;
      dwSample |= (DWORD)lpSamples[hSound->dwSampleIndex * hSound->lpWaveFormat->nBlockAlign + hSound->lpWaveFormat->nChannels - 1];
    }
    break;
  }
  hSound->dwSampleIndex += dwSampleIncrement;
  if (hSound->dwSampleIndex * hSound->lpWaveFormat->nBlockAlign >= hSound->dwBufferLength) {
    InterlockedOr((volatile LONG *)&hSound->bPaused, !hSound->bLoop); /* IDK: SHOULD WORK FINE */
    hSound->dwSampleIndex = 0;
  }
  return dwSample;
}

DWORD CALLBACK SoundPlayerThread(LPVOID lpParam) {
  HSOUNDPLAYER hPlayer = lpParam;
  HWAVEOUT hWaveOut = NULL;
  DWORD nFreeHeaders = hPlayer->dwQueueSize;
  DWORD dwNextHeader = 0;
  BOOL bActive = TRUE;
  MSG msg;
  while (bActive || !hWaveOut) {
    if (bActive) {
      if (hWaveOut && nFreeHeaders && WaitForSingleObject(hPlayer->hMutex, 0) == WAIT_OBJECT_0) {
        DWORD nSamples = hPlayer->lpWaveHeaders[dwNextHeader].dwBufferLength >> 2;
        LPWORD lpSampleBuffer = (LPVOID)hPlayer->lpWaveHeaders[dwNextHeader].lpData;
        ZeroMemory(lpSampleBuffer, hPlayer->lpWaveHeaders[dwNextHeader].dwBufferLength);
        for (HSOUND hSound = hPlayer->hSoundList; hSound; hSound = hSound->hNextSound) {
          for (DWORD dwSampleIndex = 0; dwSampleIndex < nSamples; ++dwSampleIndex) {
            DWORD dwSample = TakeSample(hSound, hPlayer->dwSampleRate);
            lpSampleBuffer[dwSampleIndex << 1] += (WORD)(dwSample >> 16);
            lpSampleBuffer[(dwSampleIndex << 1) | 1] += (WORD)dwSample;
          }
        }
        /* TODO: REMOVE */
        for (DWORD dwSampleIndex = 0; dwSampleIndex < nSamples; ++dwSampleIndex) {
          lpSampleBuffer[dwSampleIndex << 1] += rand() >> 4;
          lpSampleBuffer[(dwSampleIndex << 1) | 1] += rand() >> 4;
        }
        waveOutPrepareHeader(hWaveOut, &hPlayer->lpWaveHeaders[dwNextHeader], sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &hPlayer->lpWaveHeaders[dwNextHeader], sizeof(WAVEHDR));
        dwNextHeader = (dwNextHeader + 1) % hPlayer->dwQueueSize;
        nFreeHeaders--;
        ReleaseMutex(hPlayer->hMutex);
      }
    }
    else if (nFreeHeaders == hPlayer->dwQueueSize) {
      nFreeHeaders = 0;
      waveOutClose(hWaveOut);
    }
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      switch (msg.message) {
      case MM_WOM_OPEN:
        hWaveOut = (HWAVEOUT)msg.wParam;
        break;
      case MM_WOM_DONE:
        waveOutUnprepareHeader((HWAVEOUT)msg.wParam, (LPWAVEHDR)msg.lParam, sizeof(WAVEHDR));
        nFreeHeaders++;
        break;
      case MM_WOM_CLOSE:
        hWaveOut = NULL;
        break;
      case WM_QUIT:
        bActive = FALSE;
        break;
      }
    }
  }
  ExitThread(0);
  return 0;
}

HSOUNDPLAYER CreateSoundPlayer(_In_ DWORD dwSampleRate, _In_ DWORD dwChunkSize, _In_ DWORD dwQueueSize) {
  if (!dwChunkSize || !dwQueueSize) return NULL;
  HWAVEOUT hWaveOut;
  WAVEFORMATEX WaveFormat;
  HSOUNDPLAYER hPlayer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct HSOUNDPLAYER__) + dwQueueSize * (sizeof(WAVEHDR) + dwChunkSize));
  if (hPlayer) {
    if (!(hPlayer->hMutex = CreateMutex(NULL, FALSE, NULL))) {
      HeapFree(GetProcessHeap(), 0, hPlayer);
      return NULL;
    }
    if (!(hPlayer->hThread = CreateThread(NULL, 0, SoundPlayerThread, hPlayer, 0, &hPlayer->dwThreadId))) {
      CloseHandle(hPlayer->hMutex);
      HeapFree(GetProcessHeap(), 0, hPlayer);
      return NULL;
    }
    hPlayer->lpWaveHeaders = (LPVOID)(hPlayer + 1);
    LPVOID lpSampleBuffers = (LPVOID)(hPlayer->lpWaveHeaders + dwQueueSize);
    for (DWORD i = 0; i < dwQueueSize; ++i) {
      hPlayer->lpWaveHeaders[i].lpData = lpSampleBuffers + i * dwChunkSize;
      hPlayer->lpWaveHeaders[i].dwBufferLength = dwChunkSize;
    }
    hPlayer->dwSampleRate = dwSampleRate;
    hPlayer->dwQueueSize = dwQueueSize;

    WaveFormat.nChannels = 2;
    WaveFormat.wBitsPerSample = 16;
    WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    WaveFormat.nSamplesPerSec = dwSampleRate;
    WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
    WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;

    if (dwChunkSize % WaveFormat.nBlockAlign || waveOutOpen(&hWaveOut, WAVE_MAPPER, &WaveFormat, (DWORD_PTR)(hPlayer->dwThreadId), 0, CALLBACK_THREAD)) {
      PostThreadMessage(hPlayer->dwThreadId, WM_QUIT, 0, 0);
      WaitForSingleObject(hPlayer->hThread, INFINITE);
      CloseHandle(hPlayer->hMutex);
      CloseHandle(hPlayer->hThread);
      HeapFree(GetProcessHeap(), 0, hPlayer);
      return NULL;
    }
  }
  return hPlayer;
}

BOOL DeleteSoundPlayer(_In_ HSOUNDPLAYER hPlayer) {
  if (!hPlayer) return FALSE;
  PostThreadMessage(hPlayer->dwThreadId, WM_QUIT, 0, 0);
  if (WaitForSingleObject(hPlayer->hThread, INFINITE) != WAIT_OBJECT_0) return FALSE;
  CloseHandle(hPlayer->hMutex);
  CloseHandle(hPlayer->hThread);
  for (HSOUND hSound = hPlayer->hSoundList; hSound; hSound = hSound->hNextSound) HeapFree(GetProcessHeap(), 0, hSound);
  HeapFree(GetProcessHeap(), 0, hPlayer);
  return TRUE;
}

HSOUND CreateSound(_In_ HSOUNDPLAYER hPlayer, _In_ LPCVOID lpSound, _In_ BOOL bLoop, _In_ BOOL bPause) {
  if (!hPlayer || !lpSound) return NULL;
  CONST DWORD* lpdw = lpSound;
  if (*lpdw++ != SOUND_RIFF_SIGNATURE) return NULL;
  DWORD dwBytesLeft = *lpdw++;
  if (dwBytesLeft < 4 || *lpdw++ != SOUND_WAVE_SIGNATURE) return NULL;
  dwBytesLeft -= 4;
  DWORD dwBufferLength = 0;
  LPCVOID lpSoundBuffer = NULL;
  LPCWAVEFORMATEX lpWaveFormat = NULL;
  while (dwBytesLeft > 8) {
    DWORD dwChunkID = *lpdw++;
    DWORD dwChunkSize = *lpdw++;
    dwBytesLeft -= 8;
    lpSound = lpdw;
    if (dwBytesLeft < dwChunkSize) return NULL;
    if (dwChunkID == SOUND_DATA_SIGNATURE) {
      dwBufferLength = dwChunkSize;
      lpSoundBuffer = lpSound;
    }
    if (dwChunkID == SOUND_FMT_SIGNATURE) lpWaveFormat = lpSound;
    dwBytesLeft -= dwChunkSize;
    lpSound += dwChunkSize;
    lpdw = lpSound;
  }
  if (!dwBufferLength || !lpSoundBuffer || !lpWaveFormat) return NULL;
  HSOUND hSound = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct HSOUND__));
  if (hSound) {
    hSound->lpWaveFormat = lpWaveFormat;
    hSound->lpSoundBuffer = lpSoundBuffer;
    hSound->dwBufferLength = dwBufferLength;
    hSound->bLoop = bLoop;
    hSound->bPaused = bPause;
    hSound->hPlayer = hPlayer;
    if (WaitForSingleObject(hPlayer->hMutex, INFINITE) != WAIT_OBJECT_0) {
      HeapFree(GetProcessHeap(), 0, hSound);
      return NULL;
    }
    if (hSound->hNextSound = hPlayer->hSoundList) {
      hSound->hNextSound->hPrevSound = hSound;
    }
    hPlayer->hSoundList = hSound;
    ReleaseMutex(hPlayer->hMutex);
  }
  return hSound;
}

BOOL DeleteSound(_In_ HSOUND hSound) {
  if (!hSound) return FALSE;
  if (WaitForSingleObject(hSound->hPlayer->hMutex, INFINITE) != WAIT_OBJECT_0) return FALSE;
  if (hSound->hPlayer->hSoundList == hSound) hSound->hPlayer->hSoundList = hSound->hNextSound;
  else if (hSound->hPrevSound) hSound->hPrevSound->hNextSound = hSound->hNextSound;
  if (hSound->hNextSound) hSound->hNextSound->hPrevSound = hSound->hPrevSound;
  ReleaseMutex(hSound->hPlayer->hMutex);
  HeapFree(GetProcessHeap(), 0, hSound);
  return TRUE;
}

BOOL PauseSound(_In_ HSOUND hSound) {
  hSound->bPaused = TRUE;
  return TRUE;
}

BOOL ResumeSound(_In_ HSOUND hSound) {
  hSound->bPaused = FALSE;
  return TRUE;
}
