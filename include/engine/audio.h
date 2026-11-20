#pragma once

#include <xaudio2.h>
#include "component.h"
#include "resourceManager.h"

struct XAudioVoiceDeleter
{
	void operator()(IXAudio2Voice* voice) const
	{
		if (voice != nullptr)
		{
			voice->DestroyVoice();
		}
	}
};


class Audio : public Component
{
private:
	static ComPtr<IXAudio2> m_Xaudio;
	static std::unique_ptr<IXAudio2MasteringVoice, XAudioVoiceDeleter> m_MasteringVoice;
	static float m_MasterVolume;

	// 宣言の逆順で破棄されるので、再生中の音声が読むPCMデータ(m_Wave)を先に宣言し、
	// 音声(m_SourceVoice)を先に壊します。Uninitを経由しない破棄でも解放済みのメモリを読みません。
	Engine::WaveHandle m_Wave;
	std::unique_ptr<IXAudio2SourceVoice, XAudioVoiceDeleter> m_SourceVoice{};


public:
	static bool InitMaster();
	static void UninitMaster();
	// NaN・無限大は無視し、直前の音量を保ちます。
	static void SetMasterVolume(float volume);
	static float GetMasterVolume() { return m_MasterVolume; }

	using Component::Component;
	~Audio() override { Uninit(); }

	void Uninit() override;

	void Load(const char *FileName);
	void Play(bool Loop = false);


};

