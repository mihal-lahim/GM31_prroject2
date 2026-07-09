
#include "main.h"
#include "audio.h"





ComPtr<IXAudio2> Audio::m_Xaudio = nullptr;
std::unique_ptr<IXAudio2MasteringVoice, XAudioVoiceDeleter> Audio::m_MasteringVoice = nullptr;
float Audio::m_MasterVolume = 0.8f;


void Audio::InitMaster()
{
	// COM初期化
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// XAudio生成
	XAudio2Create(m_Xaudio.GetAddressOf(), 0);

	// マスタリングボイス生成
	IXAudio2MasteringVoice* masteringVoice = nullptr;
	m_Xaudio->CreateMasteringVoice(&masteringVoice);
	m_MasteringVoice.reset(masteringVoice);
	SetMasterVolume(m_MasterVolume);
}


void Audio::UninitMaster()
{
	m_MasteringVoice.reset();
	m_Xaudio.Reset();
	CoUninitialize();
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









void Audio::Load(const char *FileName)
{

	// サウンドデータ読込
	WAVEFORMATEX wfx = { 0 };

	{
		HMMIO hmmio = NULL;
		MMIOINFO mmioinfo = { 0 };
		MMCKINFO riffchunkinfo = { 0 };
		MMCKINFO datachunkinfo = { 0 };
		MMCKINFO mmckinfo = { 0 };
		UINT32 buflen;
		LONG readlen;


		hmmio = mmioOpen((LPSTR)FileName, &mmioinfo, MMIO_READ);
		assert(hmmio);

		riffchunkinfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');
		mmioDescend(hmmio, &riffchunkinfo, NULL, MMIO_FINDRIFF);

		mmckinfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
		mmioDescend(hmmio, &mmckinfo, &riffchunkinfo, MMIO_FINDCHUNK);

		if (mmckinfo.cksize >= sizeof(WAVEFORMATEX))
		{
			mmioRead(hmmio, (HPSTR)&wfx, sizeof(wfx));
		}
		else
		{
			PCMWAVEFORMAT pcmwf = { 0 };
			mmioRead(hmmio, (HPSTR)&pcmwf, sizeof(pcmwf));
			memset(&wfx, 0x00, sizeof(wfx));
			memcpy(&wfx, &pcmwf, sizeof(pcmwf));
			wfx.cbSize = 0;
		}
		mmioAscend(hmmio, &mmckinfo, 0);

		datachunkinfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
		mmioDescend(hmmio, &datachunkinfo, &riffchunkinfo, MMIO_FINDCHUNK);



		buflen = datachunkinfo.cksize;
		m_SoundData.resize(buflen);
		readlen = mmioRead(hmmio, (HPSTR)m_SoundData.data(), buflen);


		m_Length = readlen;
		m_PlayLength = readlen / wfx.nBlockAlign;


		mmioClose(hmmio, 0);
	}


	// サウンドソース生成
	IXAudio2SourceVoice* sourceVoice = nullptr;
	m_Xaudio->CreateSourceVoice(&sourceVoice, &wfx);
	m_SourceVoice.reset(sourceVoice);
	assert(m_SourceVoice);
}


void Audio::Uninit()
{
	if (m_SourceVoice)
	{
		m_SourceVoice->Stop();
		m_SourceVoice.reset();
	}

	m_SoundData.clear();
}





void Audio::Play(bool Loop)
{
	m_SourceVoice->Stop();
	m_SourceVoice->FlushSourceBuffers();


	// バッファ設定
	XAUDIO2_BUFFER bufinfo;

	memset(&bufinfo, 0x00, sizeof(bufinfo));
	bufinfo.AudioBytes = m_Length;
	bufinfo.pAudioData = m_SoundData.data();
	bufinfo.PlayBegin = 0;
	bufinfo.PlayLength = m_PlayLength;

	// ループ設定
	if (Loop)
	{
		bufinfo.LoopBegin = 0;
		bufinfo.LoopLength = m_PlayLength;
		bufinfo.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	m_SourceVoice->SubmitSourceBuffer(&bufinfo, NULL);

/*
	float outputMatrix[4] = { 0.0f , 0.0f, 1.0f , 0.0f };
	m_SourceVoice->SetOutputMatrix(m_MasteringVoice, 2, 2, outputMatrix);
	//m_SourceVoice->SetVolume(0.1f);
*/


	// 再生
	m_SourceVoice->Start();

}



