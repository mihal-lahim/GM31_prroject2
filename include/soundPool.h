#pragma once

#include "audio.h"
#include "component.h"

#include <memory>
#include <string>
#include <vector>

// 同じ効果音を重ねて鳴らすための声の組です。鳴らすたびに次の声を使うので、
// 連射しても前の音の尾を途中で切りません。
//   m_Shot = AddComponent<SoundPool>("assets\\audio\\shot.wav", 8);
//   m_Shot->Play();
class SoundPool final : public Component
{
public:
	SoundPool(GameObject* owner, std::string path, int voices)
		: Component(owner), m_Path(std::move(path)), m_VoiceCount(voices < 1 ? 1 : voices)
	{
	}

	void Init() override
	{
		m_Voices.clear();
		for (int i = 0; i < m_VoiceCount; ++i)
		{
			auto voice = std::make_unique<Audio>(m_GameObject);
			voice->Load(m_Path.c_str());
			m_Voices.push_back(std::move(voice));
		}
		m_Next = 0;
	}

	void Uninit() override
	{
		for (auto& voice : m_Voices)
		{
			voice->Uninit();
		}
		m_Voices.clear();
	}

	void Play()
	{
		if (m_Voices.empty())
		{
			return;
		}
		m_Voices[static_cast<size_t>(m_Next)]->Play();
		m_Next = (m_Next + 1) % static_cast<int>(m_Voices.size());
	}

	int GetVoiceCount() const { return static_cast<int>(m_Voices.size()); }

private:
	std::string m_Path;
	int m_VoiceCount = 1;
	int m_Next = 0;
	std::vector<std::unique_ptr<Audio>> m_Voices;
};
