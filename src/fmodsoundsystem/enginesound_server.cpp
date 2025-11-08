#include <fmodsoundsystem/ifmodenginesound.h>
#include <tier1/tier1.h>
#include "sound_netmessages.h"
#include <iserver.h>

class CEngineSoundServer : public IFMODEngineSound
{
public:
	CEngineSoundServer()
	{
	}

	// IFMODEngineSound
public:
	virtual void Initialize( CreateInterfaceFn appSystemFactory, CreateInterfaceFn gameFactory, CGlobalVarsBase *globals )
	{
		MathLib_Init();
		ConnectTier1Libraries( &appSystemFactory, 1 );
		m_engineServer = (IVEngineServer *) appSystemFactory( INTERFACEVERSION_VENGINESERVER, NULL );
		m_oldEngineSound = (IEngineSound *) appSystemFactory( IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL );
		m_pGlobals = globals;

		m_server = m_engineServer->GetIServer();
	}

	virtual void Shutdown()
	{
		DisconnectTier1Libraries();
	}

	// Client only
	virtual void Update( float frametime ) {}
	virtual void OnConnectedToServer() {}
	virtual void OnDisconnectedFromServer() {}
	virtual void SetAudioState( const AudioState_t &state ) {}
	virtual CSentence *GetSentence( CAudioSource *audioSource ) { return nullptr; }
	virtual float GetSentenceLength( CAudioSource *audioSource ) { return 0.f; }

	// Server only
	virtual void OnClientConnected( edict_t *pEntity )
	{
	}

	virtual void OnServerActivate()
	{
	}

	// server version of EmitAmbientSound
	virtual void EmitAmbientSound( int entindex, const Vector &pos, const char *samp,
		float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay )
	{
	}

public:
	virtual bool PrecacheSound( const char *pSample, bool bPreload = false, bool bIsUISound = false )
	{
		return true;
	}

	virtual bool IsSoundPrecached( const char *pSample )
	{
		return true;
	}

	virtual void PrefetchSound( const char *pSample )
	{
	}

	virtual float GetSoundDuration( const char *pSample )
	{
		return 0.f;
	}

	// NOTE: setting iEntIndex to -1 will cause the sound to be emitted from the local
	// player (client-side only)
	virtual void EmitSound( IRecipientFilter &filter, int iEntIndex, int iChannel, const char *pSample,
		float flVolume, float flAttenuation, int iFlags = 0, int iPitch = PITCH_NORM, int iSpecialDSP = 0,
		const Vector *pOrigin = NULL, const Vector *pDirection = NULL, CUtlVector< Vector > *pUtlVecOrigins = NULL,
		bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 )
	{
		// soundlevel is sent over the network so use the other function instead
		EmitSound( filter, iEntIndex, iChannel, pSample, flVolume, ATTN_TO_SNDLVL( flAttenuation ), iFlags, iPitch,
			iSpecialDSP, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}

	virtual void EmitSound( IRecipientFilter &filter, int iEntIndex, int iChannel, const char *pSample,
		float flVolume, soundlevel_t iSoundlevel, int iFlags = 0, int iPitch = PITCH_NORM, int iSpecialDSP = 0,
		const Vector *pOrigin = NULL, const Vector *pDirection = NULL, CUtlVector< Vector > *pUtlVecOrigins = NULL,
		bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 )
	{
		SoundInfo_t soundInfo;
		soundInfo.SetDefault();

		soundInfo.nEntityIndex = iEntIndex;
		soundInfo.nChannel = iChannel;
		soundInfo.pszName = pSample;
		soundInfo.fVolume = flVolume;
		soundInfo.Soundlevel = iSoundlevel;
		soundInfo.nFlags = iFlags;
		soundInfo.nPitch = iPitch;
		soundInfo.nSpecialDSP = iSpecialDSP;
		soundInfo.fDelay = soundtime - m_pGlobals->curtime;
		soundInfo.nSpeakerEntity = speakerentity;

		if ( pOrigin )
		{
			soundInfo.vOrigin = *pOrigin;
		}
		else
		{
			edict_t *edict = m_engineServer->PEntityOfEntIndex( iEntIndex );
			if ( edict )
			{
				ICollideable *collideable = edict->GetCollideable();
				if ( collideable )
					soundInfo.vOrigin = collideable->GetCollisionOrigin();
			}
		}

		if ( pDirection )
		{
			soundInfo.vDirection = *pDirection;
		}

		// Broadcast the sound message to whatever the filter specified
		NET_SoundMessage playSound( soundInfo );
		m_server->BroadcastMessage( playSound, filter );
	}

	virtual void EmitSentenceByIndex( IRecipientFilter &filter, int iEntIndex, int iChannel, int iSentenceIndex,
		float flVolume, soundlevel_t iSoundlevel, int iFlags = 0, int iPitch = PITCH_NORM, int iSpecialDSP = 0,
		const Vector *pOrigin = NULL, const Vector *pDirection = NULL, CUtlVector< Vector > *pUtlVecOrigins = NULL,
		bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 )
	{
		// TODO
	}

	virtual void StopSound( int iEntIndex, int iChannel, const char *pSample )
	{
		SoundInfo_t soundInfo;
		soundInfo.SetDefault();

		soundInfo.nEntityIndex = iEntIndex;
		soundInfo.nChannel = iChannel;
		soundInfo.pszName = pSample;
		soundInfo.nFlags = SND_STOP;

		// Broadcast message to all clients
		NET_SoundMessage stopSound( soundInfo );
		m_server->BroadcastMessage( stopSound );
	}

	// stop all active sounds (client only)
	virtual void StopAllSounds( bool bClearBuffers ) {}

	// Set the room type for a player (client only)
	virtual void SetRoomType( IRecipientFilter &filter, int roomType )
	{
	}

	// Set the dsp preset for a player (client only)
	virtual void SetPlayerDSP( IRecipientFilter &filter, int dspType, bool fastReset )
	{
	}

	// emit an "ambient" sound that isn't spatialized
	// only available on the client, assert on server
	virtual void EmitAmbientSound( const char *pSample, float flVolume, int iPitch = PITCH_NORM, int flags = 0, float soundtime = 0.0f ) {}

	virtual float GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
	{
		// TODO reverse engineer me
		return m_oldEngineSound->GetDistGainFromSoundLevel(soundlevel, dist);
	}

	// Client .dll only functions
	virtual int		GetGuidForLastSoundEmitted() { return 0; }
	virtual bool	IsSoundStillPlaying( int guid ) { return false; }
	virtual void	StopSoundByGuid( int guid ) {}
	// Set's master volume (0.0->1.0)
	virtual void	SetVolumeByGuid( int guid, float fvol ) {}

	// Retrieves list of all active sounds
	virtual void	GetActiveSounds( CUtlVector< SndInfo_t > &sndlist ) {}

	virtual void	PrecacheSentenceGroup( const char *pGroupName ) {}
	virtual void	NotifyBeginMoviePlayback() {}
	virtual void	NotifyEndMoviePlayback() {}

	// create/destroy an audio stream
	virtual IAudioOutputStream *CreateOutputStream( uint nSampleRate, uint nChannels, uint nBits ) { return nullptr; }
	virtual void DestroyOutputStream( IAudioOutputStream *pStream ) {}

	// Force an update, for instances where we are otherwise deadlocked from the main loop.
	virtual void	ManualUpdate( const AudioState_t *pListenerState ) {}
	// Force an extra update.
	virtual void	ExtraUpdate() {}

private:
	IVEngineServer *m_engineServer;
	IServer *m_server;
	IEngineSound *m_oldEngineSound;
	CGlobalVarsBase *m_pGlobals;
};

CEngineSoundServer g_EngineSoundServer;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineSoundServer, IFMODEngineSound,
	IFMODENGINESOUND_SERVER_INTERFACE_VERSION, g_EngineSoundServer );