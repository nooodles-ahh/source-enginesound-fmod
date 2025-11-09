//====================================================================
// Written by Nooodles (nooodlesahh@protonmail.com)
// 
// Purpose: Implementation for the client-side IEngineSound
//====================================================================
#include <tier1.h>
#include <tier2/tier2.h>
#include <tier3/tier3.h>
#include <icliententitylist.h>
#include <cdll_int.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_errors.h>
#include <filesystem.h>
#include <fmod_impl.h>
#include <soundinfo.h>
#include <icliententity.h>
#include "sound_netmessages.h"
#include <soundchars.h>
#include "gain_lut.h"
#include "fmod_overrides.h"
#include <fmodsoundsystem/ifmodenginesound.h>
#include "mouthinfo.h"
#include <utllinkedlist.h>
#include "autodsp.h"

constexpr float SourceUnitsPerMeter = 52.49344f;

ConVar channel_steal_max( "nsnd_channel_steal_max", "1", FCVAR_NONE, "Number of channels that are longer than nsnd_channel_steal_length that we allow." );
ConVar channel_steal_length( "nsnd_channel_steal_length", "0.8", FCVAR_NONE, "Is a sound is longer than this it will be stolen." );

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
	CEngineSoundClient()
	{
	}

	// ISoundMessageHandler
public:
	PROCESS_NET_MESSAGE( SoundMessage )
	{
		SoundInfo_t &soundInfo = msg->m_SoundInfo;

		float soundtime = soundInfo.fDelay + m_pGlobals->curtime;

		EmitSoundInternal( soundInfo.nEntityIndex, soundInfo.nChannel, msg->m_szSampleName,
			soundInfo.fVolume, soundInfo.Soundlevel, soundInfo.nFlags, soundInfo.nPitch,
			soundInfo.nSpecialDSP, &soundInfo.vOrigin, &soundInfo.vDirection, nullptr, false,
			soundtime, soundInfo.nSpeakerEntity, true );
		return true;
	}

	// IFMODEngineSound
public:
	virtual void Initialize( CreateInterfaceFn appSystemFactory, CreateInterfaceFn physicsFactory, CreateInterfaceFn gameFactory, CGlobalVarsBase *globals )
	{
		MathLib_Init();

		ConnectTier1Libraries( &appSystemFactory, 1 );
		ConnectTier2Libraries( &appSystemFactory, 1 );
		ConnectTier3Libraries( &appSystemFactory, 1 );

		ConVar_Register( FCVAR_CLIENTDLL );
		m_engineClient = (IVEngineClient *) appSystemFactory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );
		m_oldEngineSound = (IEngineSound *) appSystemFactory( IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL );
		m_entitylist = (IClientEntityList *) gameFactory( VCLIENTENTITYLIST_INTERFACE_VERSION, NULL );
		m_pGlobals = globals;
		m_autoDSP.Init( appSystemFactory, physicsFactory );

		g_pFMODAudioEngine->Init(
			USER_FMOD_ALLOC,
			USER_FMOD_REALLOC,
			USER_FMOD_FREE,
			USER_FMOD_FILE_OPEN_CALLBACK,
			USER_FMOD_FILE_CLOSE_CALLBACK,
			USER_FMOD_FILE_READ_CALLBACK,
			USER_FMOD_FILE_SEEK_CALLBACK,
			ConMsg
		);
	}
	virtual void Shutdown()
	{
		g_pFMODAudioEngine->Shutdown();

		ConVar_Unregister();
		DisconnectTier3Libraries();
		DisconnectTier2Libraries();
		DisconnectTier1Libraries();
	}

	// Client only
	virtual void Update( float frametime )
	{
		CUtlVector<int> vecRemoveChannels;
		FOR_EACH_LL( m_activeChannels, i )
		{
			SoundChannel &channel = m_activeChannels.Element( i );
			// if we have a speaker make sure it still exists
			if ( channel.speakerEntityIndex > 0 )
			{
				IClientEntity *pSpeaker = m_entitylist->GetClientEntity( channel.speakerEntityIndex );
				if ( !pSpeaker )
				{
					g_pFMODAudioEngine->StopChannel( channel.id );
					vecRemoveChannels.AddToTail( i );
					continue;
				}
			}

			if ( !g_pFMODAudioEngine->IsChannelPlaying( channel.id ) )
			{
				vecRemoveChannels.AddToTail( i );
				continue;
			}

			// don't spatialize world sounds
			if ( channel.entityIndex == SOUND_FROM_WORLD )
				continue;

			// Spatialize
			IClientEntity *pEntity = m_entitylist->GetClientEntity( channel.entityIndex );
			if ( pEntity )
			{
				// get the spatialization info from the entity
				SpatializationInfo_t spatInfo;
				Vector origin = pEntity->GetAbsOrigin();
				QAngle angles = pEntity->GetAbsAngles();
				float radius = 0.f;
				spatInfo.pflRadius = &radius;
				spatInfo.pAngles = &angles;
				spatInfo.pOrigin = &origin;
				spatInfo.info.nChannel = channel.sourceChannelType;

				bool bAudible = pEntity->GetSoundSpatialization( spatInfo );
				//ConMsg("SetAttributes for %s: %f %f %f\n", pSource.m_soundChannel->GetName(), origin.x, origin.y, origin.z);
				g_pFMODAudioEngine->SetChannelPosition( channel.id, { origin.x, origin.z, -origin.y } );
				g_pFMODAudioEngine->SetChannelMuted( channel.id, !bAudible );
			}
			// Static channels are manually dealt with
			else if ( channel.sourceChannelType != CHAN_STATIC )
			{
				g_pFMODAudioEngine->StopChannel( channel.id );
				vecRemoveChannels.AddToTail( i );
			}
		}

		FOR_EACH_VEC( vecRemoveChannels, i )
		{
			// TODO perform clean-up
			m_activeChannels.Remove( vecRemoveChannels[i] );
			continue;
		}

		if ( m_engineClient->IsConnected() )
		{
			static float updateDSPTime = 0.f;
			updateDSPTime -= frametime;
			if ( m_needADSPUpdate && updateDSPTime <= 0.f )
			{
				m_autoDSP.Update( m_oldAudioState.m_Origin );
				m_needADSPUpdate = false;
				updateDSPTime = 1.f / 10.f;
			}
		}

		g_pFMODAudioEngine->Update( frametime );
	}

	virtual void OnConnectedToServer()
	{
		INetChannelInfo *ni = m_engineClient->GetNetChannelInfo();
		INetChannel *chan = (INetChannel *) ni;
		REGISTER_NET_MSG( SoundMessage );
	}

	virtual void OnDisconnectedFromServer()
	{
		StopAllSounds( false );
	}

	virtual void SetAudioState( const AudioState_t &state )
	{
		Vector vecForward, vecRight, vecUp;
		AngleVectors( state.m_Angles, &vecForward, &vecRight, &vecUp );
		g_pFMODAudioEngine->UpdateListenerPosition(
			{ state.m_Origin.x, state.m_Origin.z, -state.m_Origin.y },
			{ vecForward.x, vecForward.z, -vecForward.y },
			{ vecUp.x, vecUp.z, -vecUp.y }
		);

		if ( m_oldAudioState.m_Origin != state.m_Origin ||
			m_oldAudioState.m_bIsUnderwater != state.m_bIsUnderwater )
		{
			m_needADSPUpdate = true;
		}
		m_oldAudioState = state;
	}

	virtual CSentence *GetSentence( CAudioSource *audioSource )
	{
		return nullptr;
	}

	virtual float GetSentenceLength( CAudioSource *audioSource )
	{
		return 0.f;
	}

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
		bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1, bool fromServer = false )
	{
		// if we don't have a sample, we can't do anything
		if ( !pSample || !pSample[0] )
			return;

		bool scrape = false;
		if ( V_strstr( pSample, "scrape" ) )
		{
			scrape = true;
		}

		if ( TestSoundChar( pSample, CHAR_SENTENCE ) )
		{
			ConMsg( "Attempted to play sentence %s\n", pSample );
			return;
		}

		char szSampleFull[MAX_PATH];
		V_sprintf_safe( szSampleFull, "sound\\%s", PSkipSoundChars( pSample ) );
		V_FixSlashes( szSampleFull );

		// TODO handle UI panels?
		int iEntity = iEntIndex;
		if ( iEntity == SOUND_FROM_LOCAL_PLAYER )
			iEntity = m_engineClient->IsConnected() ? m_engineClient->GetLocalPlayer() : SOUND_FROM_UI_PANEL;

		if ( iFlags & SND_STOP )
		{
			StopSound( iEntity, iChannel, pSample );
			return;
		}

		{
			// Do we need to steal from this channel?
			CUtlVector<int> vecStompChannels;
			const bool performSteal = iEntity != SOUND_FROM_WORLD && ( iChannel == CHAN_WEAPON || iChannel == CHAN_VOICE || iChannel == CHAN_VOICE2 );
			bool foundChannel = false;
			FOR_EACH_LL( m_activeChannels, i )
			{
				SoundChannel &channel = m_activeChannels[i];
				if ( channel.entityIndex == iEntIndex && channel.sourceChannelType == iChannel )
				{
					// if we're stealing 
					if ( performSteal && ( iChannel != CHAN_WEAPON || g_pFMODAudioEngine->GetChannelDuration( channel.id ) > channel_steal_length.GetFloat() ) )
						vecStompChannels.AddToTail( channel.id );

					if ( g_pFMODAudioEngine->MatchesChannelName( channel.id, szSampleFull ) )
					{
						if ( iFlags & SND_CHANGE_PITCH )
							g_pFMODAudioEngine->SetChannelPitch( channel.id, iPitch / 100.f );
						if ( iFlags & SND_CHANGE_VOL )
							g_pFMODAudioEngine->SetChannelVolume( channel.id, flVolume );
						foundChannel = iFlags & ( SND_CHANGE_PITCH | SND_CHANGE_VOL );

						UpdateChannelPosition( channel, nullptr );
					}
				}
			}

			// if we weren't stealing and found a channel to update exit out now
			if ( foundChannel )
				return;

			FOR_EACH_VEC( vecStompChannels, i )
				g_pFMODAudioEngine->StopChannel( vecStompChannels[i] );
		}

		float flSoundPos = fabs( fminf( 0, soundtime - m_pGlobals->curtime ) );
		// temp until I figure out why
		float fVol = iChannel == CHAN_WEAPON ? flVolume * 2.f : flVolume;

		// Grab a new sound channel from the manager
		SoundVector pos{ 0, 0, 0 };
		if ( pOrigin )
			pos = { pOrigin->x, pOrigin->z, -( pOrigin->y ) };
		SoundVector ang;
		if ( pDirection )
			ang = { pDirection->x, pDirection->z, -( pDirection->y ) };

		int channelId = g_pFMODAudioEngine->PlaySound( szSampleFull, fVol, pos, ang, true );
		if ( channelId == -1 )
			return;

		// create a new sound source
		unsigned short i = m_activeChannels.AddToTail();
		SoundChannel &channel = m_activeChannels[i];
		channel.id = channelId;
		channel.entityIndex = iEntity;
		channel.sourceChannelType = iChannel;
		channel.speakerEntityIndex = speakerentity > 0 ? speakerentity : -1;
		channel.fromServer = fromServer;

		float maxDist = dbToGainDist( iSoundlevel );
		g_pFMODAudioEngine->SetChannelMinMaxDist( channel.id, SourceUnitsPerMeter, maxDist );

		if ( channel.entityIndex != SOUND_FROM_WORLD )
			UpdateChannelPosition( channel, nullptr );

		g_pFMODAudioEngine->StartChannel( channelId );
	}

	void UpdateChannelPosition( SoundChannel &channel, const Vector *pOrigin )
	{
		if ( channel.entityIndex == SOUND_FROM_WORLD )
			return;

		IClientEntity *pEntity = m_entitylist->GetClientEntity( channel.entityIndex );
		if ( pEntity )
		{
			// if we previously didn't have an valid client entity, now we do
			if ( channel.fromServer )
				channel.fromServer = false;

			// get the spatialization info from the entity
			SpatializationInfo_t spatInfo;
			Vector origin = pEntity->GetAbsOrigin();
			QAngle angles = pEntity->GetAbsAngles();
			float radius = 0.f;
			spatInfo.pflRadius = &radius;
			spatInfo.pAngles = &angles;
			spatInfo.pOrigin = &origin;
			spatInfo.info.nChannel = channel.sourceChannelType;

			bool bAudible = pEntity->GetSoundSpatialization( spatInfo );
			g_pFMODAudioEngine->SetChannelPosition( channel.id, { origin.x, origin.z, -origin.y } );
			g_pFMODAudioEngine->SetChannelMuted( channel.id, !bAudible );
		}
		else if ( pOrigin )
		{
			g_pFMODAudioEngine->SetChannelPosition( channel.id, { pOrigin->x, pOrigin->z, -pOrigin->y } );
			g_pFMODAudioEngine->SetChannelMuted( channel.id, false );

		}
	}

	// IEngineSound
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
		EmitSoundInternal( iEntIndex, iChannel, pSample,
			flVolume, ATTN_TO_SNDLVL( flAttenuation ), iFlags, iPitch, iSpecialDSP,
			pOrigin, pDirection, pUtlVecOrigins,
			bUpdatePositions, soundtime, speakerentity );
	}

	virtual void EmitSound( IRecipientFilter &filter, int iEntIndex, int iChannel, const char *pSample,
		float flVolume, soundlevel_t iSoundlevel, int iFlags = 0, int iPitch = PITCH_NORM, int iSpecialDSP = 0,
		const Vector *pOrigin = NULL, const Vector *pDirection = NULL, CUtlVector< Vector > *pUtlVecOrigins = NULL, 
		bool bUpdatePositions = true, float soundtime = 0.0f, int speakerentity = -1 )
	{
		EmitSoundInternal( iEntIndex, iChannel, pSample,
			flVolume, iSoundlevel, iFlags, iPitch, iSpecialDSP,
			pOrigin, pDirection, pUtlVecOrigins,
			bUpdatePositions, soundtime, speakerentity );
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
		char szSampleFull[MAX_PATH];
		V_sprintf_safe( szSampleFull, "sound\\%s", PSkipSoundChars( pSample ) );
		V_FixSlashes( szSampleFull );

		CUtlVector<int> vecRemove;
		FOR_EACH_LL( m_activeChannels, i )
		{
			SoundChannel &channel = m_activeChannels[i];
			if ( channel.entityIndex == iEntIndex && channel.sourceChannelType == iChannel )
			{
				if ( g_pFMODAudioEngine->MatchesChannelName( channel.id, szSampleFull ) )
				{
					// let update handle clean-up
					g_pFMODAudioEngine->StopChannel( channel.id );
				}
			}
		}
	}

	// stop all active sounds (client only)
	virtual void StopAllSounds( bool bClearBuffers )
	{
		// update will handle clean-up
		g_pFMODAudioEngine->StopAllChannels();
	}

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
	virtual void EmitAmbientSound( const char *pSample, float flVolume, int iPitch = PITCH_NORM, int flags = 0, float soundtime = 0.0f )
	{
	}

	virtual float GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
	{
		// TODO reverse engineer me
		return m_oldEngineSound->GetDistGainFromSoundLevel( soundlevel, dist );
	}

	// Client .dll only functions
	virtual int		GetGuidForLastSoundEmitted()
	{
		return g_pFMODAudioEngine->GetLastGUID();
	}

	virtual bool	IsSoundStillPlaying( int guid )
	{
		return g_pFMODAudioEngine->IsChannelPlaying( guid );
	}

	virtual void	StopSoundByGuid( int guid )
	{
		g_pFMODAudioEngine->StopChannel( guid );
	}

	// Set's master volume (0.0->1.0)
	virtual void	SetVolumeByGuid( int guid, float fvol )
	{
		g_pFMODAudioEngine->SetChannelVolume( guid, fvol );
	}

	// Retrieves list of all active sounds
	virtual void	GetActiveSounds( CUtlVector< SndInfo_t > &sndlist )
	{
	}

	virtual void	PrecacheSentenceGroup( const char *pGroupName )
	{
	}

	virtual void	NotifyBeginMoviePlayback()
	{
	}

	virtual void	NotifyEndMoviePlayback()
	{
	}

	// create/destroy an audio stream
	virtual IAudioOutputStream *CreateOutputStream( uint nSampleRate, uint nChannels, uint nBits )
	{
		return nullptr;
	}

	virtual void DestroyOutputStream( IAudioOutputStream *pStream )
	{
	}

	// Force an update, for instances where we are otherwise deadlocked from the main loop.
	virtual void	ManualUpdate( const AudioState_t *pListenerState )
	{
	}

	// Force an extra update.
	virtual void	ExtraUpdate()
	{
	}

private:
	IVEngineClient *m_engineClient;
	IEngineSound *m_oldEngineSound;
	IClientEntityList *m_entitylist;
	CGlobalVarsBase *m_pGlobals;
	CUtlLinkedList< SoundChannel > m_activeChannels;

	AudioState_t m_oldAudioState;
	bool m_needADSPUpdate;
	CAutoDSP m_autoDSP;
};

CEngineSoundClient g_EngineSoundClient;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineSoundClient, IEngineSound,
	IFMODENGINESOUND_CLIENT_INTERFACE_VERSION, g_EngineSoundClient );

// figure out when the gain is basically 0
CON_COMMAND( nsnd_get_min_dist, "" )
{
	if ( args.ArgC() != 3 )
		return;

	const char* db = args.Arg( 1 );
	int iDBStart = atoi( db );

	db = args.Arg( 2 );
	int iDBStop = atoi( db );

	for ( int iDB = iDBStart; iDB <= iDBStop; iDB += 5 )
	{
		double minDist = 0.0f;
		double gain = g_EngineSoundClient.GetDistGainFromSoundLevel( (soundlevel_t) iDB, minDist );
		while ( gain > 0.0001 )
		{
			if ( iDB < 50 )
				minDist += 0.01;
			else if ( iDB < 100 )
				minDist += 0.05;
			else
				minDist += 0.1;
			gain = g_EngineSoundClient.GetDistGainFromSoundLevel( (soundlevel_t) iDB, minDist );
		}

		ConMsg( "Min distance for soundlevel %d is %f (%f meters)\n", iDB, minDist, minDist / SourceUnitsPerMeter );
	}
}