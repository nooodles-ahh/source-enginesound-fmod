#pragma once

#include <engine/IEngineSound.h>
#include "interface.h"
#include "eiface.h"
#include <sentence.h>

#define IFMODENGINESOUND_CLIENT_INTERFACE_VERSION	"IFMODEngineSoundClient001"
#define IFMODENGINESOUND_SERVER_INTERFACE_VERSION	"IFMODEngineSoundServer001"

// extended IEngineSound interface required for to handle engine tasks and additional functionality
class CAudioSource;

abstract_class IFMODEngineSound : public IEngineSound
{
public:
	virtual void Initialize( CreateInterfaceFn appSystemFactory, CreateInterfaceFn gameFactory, CGlobalVarsBase *globals ) = 0;
	virtual void Shutdown() = 0;

	// Client only
	virtual void Update( float frametime ) = 0;
	virtual void OnConnectedToServer() = 0;
	virtual void OnDisconnectedFromServer() = 0;
	virtual void SetAudioState( const AudioState_t &state ) = 0;
	virtual CSentence *GetSentence( CAudioSource *audioSource ) = 0;
	virtual float GetSentenceLength( CAudioSource *audioSource ) = 0;

	// Server only
	virtual void OnClientConnected( edict_t *pEntity ) = 0;
	virtual void OnServerActivate() = 0;
	// server version of EmitAmbientSound
	virtual void EmitAmbientSound( int entindex, const Vector &pos, const char *samp,
		float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay ) = 0;
};