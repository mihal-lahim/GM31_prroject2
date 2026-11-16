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

	std::unique_ptr<IXAudio2SourceVoice, XAudioVoiceDeleter> m_SourceVoice{};
	Engine::WaveHandle m_Wave;


public:
	static bool InitMaster();
	static void UninitMaster();
	static void SetMasterVolume(float volume);
	static float GetMasterVolume() { return m_MasterVolume; }

	using Component::Component;

	void Uninit() override;

	void Load(const char *FileName);
	void Play(bool Loop = false);


};

