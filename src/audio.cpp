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

bool Audio::InitMaster()
{
	if (m_Xaudio != nullptr)
	{
		return m_MasteringVoice != nullptr;
	}

	// COM初期化はアプリ全体の責務としてmain.cppで行う。
	HRESULT hr = XAudio2Create(m_Xaudio.GetAddressOf(), 0);
	if (FAILED(hr))
	{
		DebugLog("Audio::InitMaster: XAudio2Create failed.\n");
		m_Xaudio.Reset();
		return false;
	}

	IXAudio2MasteringVoice* masteringVoice = nullptr;
	hr = m_Xaudio->CreateMasteringVoice(&masteringVoice);
	if (FAILED(hr))
	{
		DebugLog("Audio::InitMaster: CreateMasteringVoice failed.\n");
		m_Xaudio.Reset();
		return false;
	}

	m_MasteringVoice.reset(masteringVoice);
	SetMasterVolume(m_MasterVolume);
	return true;
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

	m_Wave = ResourceManager::LoadWave(FileName);
	if (m_Wave == nullptr || m_Wave->GetFormat() == nullptr)
	{
		DebugLog("Audio::Load: shared wave resource load failed.\n");
		return;
	}

	IXAudio2SourceVoice* sourceVoice = nullptr;
	HRESULT hr = m_Xaudio->CreateSourceVoice(&sourceVoice, m_Wave->GetFormat());
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

	m_Wave.reset();
}

void Audio::Play(bool Loop)
{
	if (m_SourceVoice == nullptr || m_Wave == nullptr || m_Wave->AudioData.empty() || m_Wave->PlayLength == 0)
	{
		return;
	}

	m_SourceVoice->Stop();
	m_SourceVoice->FlushSourceBuffers();

	// 読み込んだWAVバッファをXAudio2へ渡す。
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(m_Wave->AudioData.size());
	buffer.pAudioData = m_Wave->AudioData.data();
	buffer.PlayBegin = 0;
	buffer.PlayLength = m_Wave->PlayLength;

	if (Loop)
	{
		buffer.LoopBegin = 0;
		buffer.LoopLength = m_Wave->PlayLength;
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	if (FAILED(m_SourceVoice->SubmitSourceBuffer(&buffer, nullptr)))
	{
		DebugLog("Audio::Play: SubmitSourceBuffer failed.\n");
		return;
	}

	m_SourceVoice->Start();
}
