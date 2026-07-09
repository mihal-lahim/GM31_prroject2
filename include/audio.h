#pragma once

#include <xaudio2.h>
#include "component.h"

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
	std::vector<BYTE>		m_SoundData{};

	int						m_Length{};
	int						m_PlayLength{};


public:
	static void InitMaster();
	static void UninitMaster();
	static void SetMasterVolume(float volume);
	static float GetMasterVolume() { return m_MasterVolume; }

	using Component::Component;

	void Uninit() override;

	void Load(const char *FileName);
	void Play(bool Loop = false);


};

