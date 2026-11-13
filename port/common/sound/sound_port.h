// CSound's port plumbing shared by both games' sound.cpp (docs/CONSOLIDATION_PLAN.md, stage 4 rank 7):
// the DirectSound buffer registry, the historical .blp wave header and its PCM load into the port mixer
// (CreateBufferFromWaveFile), the MCI end-of-song notification (PortSoundPoll) and the midiOutSetVolume
// table (InitMidiVolume). Until 2026-11-13 each game had an identical copy.
//
// Included once by games/sb1/game/sound.cpp and games/sb2/game/sound.cpp after their includes, so it
// is compiled in each game's own context (its def.h and sound.h, whose CSound members it defines).
// CSound's policy (the constructor, PlayMusic, CD audio, volumes) stays in the game's file.
#ifndef BLUPI_SOUND_PORT_H
#define BLUPI_SOUND_PORT_H

#include <vector>

struct IDirectSound
{
};

struct IDirectSoundBuffer
{
	int id;
};

#define DS_OK ((HRESULT)0)

/////////////////////////////////////////////////////////////////////////////

struct WaveHeader
{
    BYTE        RIFF[4];          // "RIFF"
    DWORD       dwSize;           // Size of data to follow
    BYTE        WAVE[4];          // "WAVE"
    BYTE        fmt_[4];          // "fmt "
    DWORD       dw16;             // 16
    WORD        wOne_0;           // 1
    WORD        wChnls;           // Number of Channels
    DWORD       dwSRate;          // Sample Rate
    DWORD       BytesPerSec;      // Sample Rate
    WORD        wBlkAlign;        // 1
    WORD        BitsPerSample;    // Sample size
    BYTE        DATA[4];          // "DATA"
    DWORD       dwDSize;          // Number of Samples
};

static_assert(sizeof(WaveHeader) == 44, "historical wave header layout");

// Port: the original created an empty DirectSound buffer, then ReadData
// filled it. The mixer needs the samples up front, so both steps happen in
// CreateBufferFromWaveFile; these keep their historical signatures.

BOOL CSound::CreateSoundBuffer(int dwBuf, DWORD dwBufSize, DWORD dwFreq, DWORD dwBitsPerSample, DWORD dwBlkAlign, BOOL bStereo)
{
	return FALSE;
}

BOOL CSound::ReadData(LPDIRECTSOUNDBUFFER lpDSB, FILE* pFile, DWORD dwSize, DWORD dwPos)
{
	return FALSE;
}

// Creates a DirectSound buffer from a wave file.

BOOL CSound::CreateBufferFromWaveFile(int dwBuf, char *pFileName)
{
	// Open the wave file
	FILE* pFile = fopen(pFileName, "rb");
	if ( pFile == NULL ) return FALSE;

	// Read in the wave header
	WaveHeader wavHdr;
	if ( fread(&wavHdr, sizeof(wavHdr), 1, pFile) != 1 )
	{
		fclose(pFile);
		return FALSE;
	}

	// Figure out the size of the data region
	DWORD dwSize = wavHdr.dwDSize;

	// Is this a stereo or mono file?
	BOOL bStereo = wavHdr.wChnls > 1 ? TRUE : FALSE;

	// CreateSoundBuffer rejected invalid secondary-buffer sizes and PCM block
	// alignment. These header values must be checked before allocating samples.
	if ( dwSize < 4 || dwSize > 0x0fffffff ||
	     wavHdr.wBlkAlign != (bStereo ? 2 : 1)*wavHdr.BitsPerSample/8 )
	{
		fclose(pFile);
		return FALSE;
	}

	// Read the data for the wave file (the historical ReadData read exactly
	// dwSize bytes after the 44-byte header and failed on a short read).
	std::vector<uint8_t> data(dwSize);
	if ( dwSize == 0 || fread(data.data(), dwSize, 1, pFile) != 1 )
	{
		fclose(pFile);
		return FALSE;
	}
	fclose(pFile);

	int id = blupi::audio::create_buffer(data.data(), dwSize, (int)wavHdr.dwSRate,
										 wavHdr.BitsPerSample, bStereo ? 2 : 1);
	if ( id < 0 )  return FALSE;
	m_lpDSB[dwBuf] = new IDirectSoundBuffer{id};
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

// MCI state of the sequencer device: the song whose MCI_PLAY asked for
// MCI_NOTIFY and has not been answered yet, and the window to notify.
static unsigned	s_notifyDevice = 0;
static HWND		s_notifyWnd = NULL;

// Posts MM_MCINOTIFY(MCI_NOTIFY_SUCCESSFUL) when the song with a pending
// notification reached its end. Called by the host loop and before a close.

void PortSoundPoll()
{
	unsigned	finished = blupi::music::take_finished();

	if ( finished != 0 && finished == s_notifyDevice )
	{
		char	text[100];
		sprintf(text, "MIDI end of song (device %u): MCI_NOTIFY_SUCCESSFUL\n", finished);
		OutputDebug(text);
		PostMessage(s_notifyWnd, MM_MCINOTIFY, MCI_NOTIFY_SUCCESSFUL, (LPARAM)finished);
		s_notifyDevice = 0;
	}
}

// Modifie le volume midi.
// Le volume est compris entre 0 et 20 !
// Port: the historical table of midiOutSetVolume levels (low word = left
// channel) drives the synthesizer output level linearly.

void InitMidiVolume(int volume)
{
	static unsigned int table[21] =
	{
		0x00000000,
		0x11111111,
		0x22222222,
		0x33333333,
		0x44444444,
		0x55555555,
		0x66666666,
		0x77777777,
		0x88888888,
		0x99999999,
		0xAAAAAAAA,
		0xBBBBBBBB,
		0xCCCCCCCC,
		0xDDDDDDDD,
		0xEEEEEEEE,
		0xF222F222,
		0xF555F555,
		0xF777F777,
		0xFAAAFAAA,
		0xFDDDFDDD,
		0xFFFFFFFF,
	};

	if ( volume < 0         )  volume = 0;
	if ( volume > MAXVOLUME )  volume = MAXVOLUME;

	blupi::music::set_level((float)(table[volume] & 0xFFFF) / 65535.0f);
}

/////////////////////////////////////////////////////////////////////////////

// Constructeur.

#endif
