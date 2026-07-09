#include "main.h"
#include "audio.h"

namespace
{
	void DebugLog(const char* message)
	{
		OutputDebugStringA(message);
	}
}

ComPtr<IXAudio2> Audio::m_Xaudio = nullptr;
std::unique_ptr<IXAudio2MasteringVoice, XAudioVoiceDeleter> Audio::m_MasteringVoice = nullptr;
float Audio::m_MasterVolume = 0.8f;

void Audio::InitMaster()
{
	if (m_Xaudio != nullptr)
	{
		return;
	}

	// COM初期化はアプリ全体の責務としてmain.cppで行う。
	HRESULT hr = XAudio2Create(m_Xaudio.GetAddressOf(), 0);
	if (FAILED(hr))
	{
		DebugLog("Audio::InitMaster: XAudio2Create failed.\n");
		m_Xaudio.Reset();
		return;
	}

	IXAudio2MasteringVoice* masteringVoice = nullptr;
	hr = m_Xaudio->CreateMasteringVoice(&masteringVoice);
	if (FAILED(hr))
	{
		DebugLog("Audio::InitMaster: CreateMasteringVoice failed.\n");
		m_Xaudio.Reset();
		return;
	}

	m_MasteringVoice.reset(masteringVoice);
	SetMasterVolume(m_MasterVolume);
}

void Audio::UninitMaster()
{
	m_MasteringVoice.reset();
	m_Xaudio.Reset();
}

void Audio::SetMasterVolume(float volume)
{
	if (volume < 0.0f)
	{
		volume = 0.0f;
	}
	if (volume > 1.0f)
	{
		volume = 1.0f;
	}

	m_MasterVolume = volume;
	if (m_MasteringVoice)
	{
		m_MasteringVoice->SetVolume(m_MasterVolume);
	}
}

void Audio::Load(const char* FileName)
{
	Uninit();

	if (FileName == nullptr || FileName[0] == '\0')
	{
		DebugLog("Audio::Load: invalid file name.\n");
		return;
	}

	if (m_Xaudio == nullptr)
	{
		DebugLog("Audio::Load: XAudio2 is not initialized.\n");
		return;
	}

	// WAVファイルを読み込み、fmtチャンクとdataチャンクから再生用データを作る。
	WAVEFORMATEX wfx{};
	HMMIO hmmio = mmioOpenA(const_cast<LPSTR>(FileName), nullptr, MMIO_READ);
	if (hmmio == nullptr)
	{
		DebugLog("Audio::Load: failed to open wave file.\n");
		return;
	}

	auto closeFile = [&hmmio]()
		{
			if (hmmio != nullptr)
			{
				mmioClose(hmmio, 0);
				hmmio = nullptr;
			}
		};

	MMCKINFO riffChunk{};
	riffChunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	if (mmioDescend(hmmio, &riffChunk, nullptr, MMIO_FINDRIFF) != MMSYSERR_NOERROR)
	{
		DebugLog("Audio::Load: RIFF WAVE chunk was not found.\n");
		closeFile();
		return;
	}

	MMCKINFO formatChunk{};
	formatChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (mmioDescend(hmmio, &formatChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
	{
		DebugLog("Audio::Load: fmt chunk was not found.\n");
		closeFile();
		return;
	}

	if (formatChunk.cksize >= sizeof(WAVEFORMATEX))
	{
		if (mmioRead(hmmio, reinterpret_cast<HPSTR>(&wfx), sizeof(wfx)) != sizeof(wfx))
		{
			DebugLog("Audio::Load: failed to read WAVEFORMATEX.\n");
			closeFile();
			return;
		}
	}
	else
	{
		PCMWAVEFORMAT pcmwf{};
		if (mmioRead(hmmio, reinterpret_cast<HPSTR>(&pcmwf), sizeof(pcmwf)) != sizeof(pcmwf))
		{
			DebugLog("Audio::Load: failed to read PCMWAVEFORMAT.\n");
			closeFile();
			return;
		}

		memcpy(&wfx, &pcmwf, sizeof(pcmwf));
		wfx.cbSize = 0;
	}

	mmioAscend(hmmio, &formatChunk, 0);

	if (wfx.nBlockAlign == 0)
	{
		DebugLog("Audio::Load: invalid block alignment.\n");
		closeFile();
		return;
	}

	MMCKINFO dataChunk{};
	dataChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(hmmio, &dataChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR || dataChunk.cksize == 0)
	{
		DebugLog("Audio::Load: data chunk was not found.\n");
		closeFile();
		return;
	}

	m_SoundData.resize(dataChunk.cksize);
	const LONG readLength = mmioRead(hmmio, reinterpret_cast<HPSTR>(m_SoundData.data()), dataChunk.cksize);
	closeFile();

	if (readLength <= 0)
	{
		DebugLog("Audio::Load: failed to read audio data.\n");
		m_SoundData.clear();
		return;
	}

	m_SoundData.resize(static_cast<size_t>(readLength));
	m_Length = static_cast<int>(readLength);
	m_PlayLength = m_Length / wfx.nBlockAlign;
	if (m_PlayLength <= 0)
	{
		DebugLog("Audio::Load: audio data is too short.\n");
		Uninit();
		return;
	}

	IXAudio2SourceVoice* sourceVoice = nullptr;
	HRESULT hr = m_Xaudio->CreateSourceVoice(&sourceVoice, &wfx);
	if (FAILED(hr))
	{
		DebugLog("Audio::Load: CreateSourceVoice failed.\n");
		Uninit();
		return;
	}

	m_SourceVoice.reset(sourceVoice);
}

void Audio::Uninit()
{
	if (m_SourceVoice)
	{
		m_SourceVoice->Stop();
		m_SourceVoice.reset();
	}

	m_SoundData.clear();
	m_Length = 0;
	m_PlayLength = 0;
}

void Audio::Play(bool Loop)
{
	if (m_SourceVoice == nullptr || m_SoundData.empty() || m_Length <= 0 || m_PlayLength <= 0)
	{
		return;
	}

	m_SourceVoice->Stop();
	m_SourceVoice->FlushSourceBuffers();

	// 読み込んだWAVバッファをXAudio2へ渡す。
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(m_Length);
	buffer.pAudioData = m_SoundData.data();
	buffer.PlayBegin = 0;
	buffer.PlayLength = m_PlayLength;

	if (Loop)
	{
		buffer.LoopBegin = 0;
		buffer.LoopLength = m_PlayLength;
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	if (FAILED(m_SourceVoice->SubmitSourceBuffer(&buffer, nullptr)))
	{
		DebugLog("Audio::Play: SubmitSourceBuffer failed.\n");
		return;
	}

	m_SourceVoice->Start();
}
