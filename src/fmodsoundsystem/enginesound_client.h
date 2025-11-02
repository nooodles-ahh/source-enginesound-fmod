#pragma once
#include <fmodsoundsystem/ifmodenginesound.h>
#include <tier1.h>
#include <tier2/tier2.h>
#include <tier3/tier3.h>
#include <icliententitylist.h>
#include <cdll_int.h>
#include "mouthinfo.h"
#include <utllinkedlist.h>
#include "sound_netmessages.h"

struct SoundChannel
{
	// Channel ID returned by FMOD system
	int id;
	// sound attributes we have to hold onto
	int entityIndex;
	int speakerEntityIndex;
	// internal channel name like CHAN_VOICE or CHAN_WEAPON
	int sourceChannelType;
	// entities from the server won't immediately find something client side
	int fromServer;
};

class CEngineSoundClient : public IFMODEngineSound, public ISoundMessageHandler
{
public:
	CEngineSoundClient();
	
	// ISoundMessageHandler
public:
	PROCESS_NET_MESSAGE( SoundMessage );

	// IFMODEngineSound
public:
	virtual void Initialize( CreateInterfaceFn appSystemFactory, CreateInterfaceFn gameFactory, CGlobalVarsBase *globals );
	virtual void Shutdown();

	// Client only
	virtual void Update( float frametime );
	virtual void OnConnectedToServer();
	virtual void OnDisconnectedFromServer();
	virtual void SetAudioState( const AudioState_t &state );
	virtual CSentence *GetSentence( CAudioSource *audioSource );
	virtual float GetSentenceLength( CAudioSource *audioSource );

	// Server only
	virtual void OnClientConnected( edict_t *pEntity ) { Assert( "IFMODEngineSound::OnClientConnected not available on client\n" ); }
	virtual void OnServerActivate() { Assert( "IFMODEngineSound::OnServerActivate not available on client\n" ); }
	// server version of EmitAmbientSound
	virtual void EmitAmbientSound( int entindex, const Vector &pos, const char *samp,
		float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay ) 
	{
		Assert( "IFMODEngineSound::EmitAmbientSound not available on client\n" );
	}

private:
	void EmitSoundInternal( int iEntIndex, int iChannel, const char *pSample,
		float flVolume, soundlevel_t iSoundlevel, int iFlags = 0, int iPitch = PITCH_NORM, int iSpecialDSP = 0,
		const Vector *pOrigin = NULL, const Vector *pDirection = NULL, CUtlVector< Vector > *pUtlVecOrigins = NULL,
		bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1, bool fromServer = false );
	bool StompChannels( int iEntIdx, int iChannel, const char *pSample );
	void UpdateChannelPosition( SoundChannel& channel, const Vector *pOrigin );

	// IEngineSound
public:
	virtual bool PrecacheSound( const char *pSample, bool bPreload = false, bool bIsUISound = false );
	virtual bool IsSoundPrecached( const char *pSample );
	virtual void PrefetchSound( const char *pSample );
	virtual float GetSoundDuration( const char *pSample );  

	// NOTE: setting iEntIndex to -1 will cause the sound to be emitted from the local
	// player (client-side only)
	virtual void EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSample, 
		float flVolume, float flAttenuation, int iFlags = 0, int iPitch = PITCH_NORM, int iSpecialDSP = 0, 
		const Vector *pOrigin = NULL, const Vector *pDirection = NULL, CUtlVector< Vector >* pUtlVecOrigins = NULL, bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 );

	virtual void EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSample, 
		float flVolume, soundlevel_t iSoundlevel, int iFlags = 0, int iPitch = PITCH_NORM, int iSpecialDSP = 0, 
		const Vector *pOrigin = NULL, const Vector *pDirection = NULL, CUtlVector< Vector >* pUtlVecOrigins = NULL, bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 );

	virtual void EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, int iSentenceIndex, 
		float flVolume, soundlevel_t iSoundlevel, int iFlags = 0, int iPitch = PITCH_NORM,int iSpecialDSP = 0, 
		const Vector *pOrigin = NULL, const Vector *pDirection = NULL, CUtlVector< Vector >* pUtlVecOrigins = NULL, bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 );

	virtual void StopSound( int iEntIndex, int iChannel, const char *pSample );

	// stop all active sounds (client only)
	virtual void StopAllSounds(bool bClearBuffers);

	// Set the room type for a player (client only)
	virtual void SetRoomType( IRecipientFilter& filter, int roomType );

	// Set the dsp preset for a player (client only)
	virtual void SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset );
	
	// emit an "ambient" sound that isn't spatialized
	// only available on the client, assert on server
	virtual void EmitAmbientSound( const char *pSample, float flVolume, int iPitch = PITCH_NORM, int flags = 0, float soundtime = 0.0f );

	virtual float GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist );

	// Client .dll only functions
	virtual int		GetGuidForLastSoundEmitted();
	virtual bool	IsSoundStillPlaying( int guid );
	virtual void	StopSoundByGuid( int guid );
	// Set's master volume (0.0->1.0)
	virtual void	SetVolumeByGuid( int guid, float fvol );

	// Retrieves list of all active sounds
	virtual void	GetActiveSounds( CUtlVector< SndInfo_t >& sndlist );

	virtual void	PrecacheSentenceGroup( const char *pGroupName );
	virtual void	NotifyBeginMoviePlayback();
	virtual void	NotifyEndMoviePlayback();

	// create/destroy an audio stream
	virtual IAudioOutputStream *CreateOutputStream( uint nSampleRate, uint nChannels, uint nBits );
	virtual void DestroyOutputStream( IAudioOutputStream *pStream );

	// Force an update, for instances where we are otherwise deadlocked from the main loop.
	virtual void	ManualUpdate( const AudioState_t *pListenerState );
	// Force an extra update.
	virtual void	ExtraUpdate();

private:
	IVEngineClient *m_engineClient;
	IEngineSound *m_oldEngineSound;
	IClientEntityList *m_entitylist;
	CGlobalVarsBase *m_pGlobals;
	CUtlLinkedList< SoundChannel > m_activeChannels;
};