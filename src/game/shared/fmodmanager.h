#ifndef FMODMANAGER_H
#define FMODMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"
#include "fmodsoundsystem/ifmodenginesound.h"

class CFMODManager : public CAutoGameSystemPerFrame
{
public:
	CFMODManager();
	virtual ~CFMODManager();

	virtual bool Init();
	virtual void Shutdown();

	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPreEntity();

	virtual void OnSave();
	virtual void OnRestore();

#ifdef CLIENT_DLL
	virtual void Update(float frametime);
	void SetAudioState(AudioState_t state);
	CSentence* GetSentence(CAudioSource* audioSource);
	float GetSentenceLength(CAudioSource* audioSource);
#else
	void OnClientConnect(edict_t* pEntity);
	virtual void EmitAmbientSound(int entindex, const Vector& pos, const char* samp,
		float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay);
#endif

private:
	IFMODEngineSound* m_pFMODSystem;
	CSysModule* m_hFMODModule;
};

extern CFMODManager *g_pFMODManager;
#endif