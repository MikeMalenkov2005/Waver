#ifndef SOUND_H
#define SOUND_H

#include <Windows.h>

DECLARE_HANDLE(HSOUNDPLAYER);
DECLARE_HANDLE(HSOUND);

HSOUNDPLAYER CreateSoundPlayer(_In_ DWORD dwSampleRate, _In_ DWORD dwChunkSize, _In_ DWORD dwQueueSize);
BOOL DeleteSoundPlayer(_In_ HSOUNDPLAYER hPlayer);

HSOUND CreateSound(_In_ HSOUNDPLAYER hPlayer, _In_ LPCVOID lpSound, _In_ BOOL bLoop, _In_ BOOL bPause);
BOOL DeleteSound(_In_ HSOUND hSound);

BOOL PauseSound(_In_ HSOUND hSound);
BOOL ResumeSound(_In_ HSOUND hSound);

#endif
