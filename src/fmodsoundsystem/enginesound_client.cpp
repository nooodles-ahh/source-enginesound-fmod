#include "enginesound_client.h"
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

static void *F_CALL USER_FMOD_ALLOC( unsigned int size, FMOD_MEMORY_TYPE, const char * )
{
	return MemAlloc_AllocAligned( size, 16 );
}

static void *F_CALL USER_FMOD_REALLOC( void *ptr, unsigned int size, FMOD_MEMORY_TYPE, const char * )
{
	return MemAlloc_ReallocAligned( ptr, size, 16 );
}

static void F_CALL USER_FMOD_FREE( void *ptr, FMOD_MEMORY_TYPE, const char * )
{
	MemAlloc_FreeAligned( ptr );
}

static FMOD_RESULT F_CALL USER_FMOD_FILE_OPEN_CALLBACK( const char *name, unsigned int *filesize, void **handle, void *userdata )
{
	FileHandle_t fileHandle = g_pFullFileSystem->Open( name, "rb", nullptr );
	// Same as checking if NULL
	if ( fileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		ConColorMsg( Color( 255, 0, 0, 255 ), "FMOD FILESYSTEM ERROR: \"%s\"\n", FMOD_ErrorString( FMOD_ERR_FILE_NOTFOUND ) );
		return FMOD_ERR_FILE_NOTFOUND;
	}

	*handle = fileHandle;
	*filesize = g_pFullFileSystem->Size( fileHandle );

	return FMOD_OK;
}

static FMOD_RESULT F_CALL USER_FMOD_FILE_CLOSE_CALLBACK( void *handle, void *userdata )
{
	FileHandle_t fileHandle = handle;
	g_pFullFileSystem->Close( fileHandle );
	return FMOD_OK;
}

static FMOD_RESULT F_CALL USER_FMOD_FILE_READ_CALLBACK( void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void *userdata )
{
	// We shouldn't get to the read callback if the file handle is invalid, so we shouldn't worry about checking it
	FileHandle_t fileHandle = handle;
	*bytesread = (unsigned int) g_pFullFileSystem->Read( buffer, sizebytes, fileHandle );
	if ( *bytesread == 0 )
	{
		ConColorMsg( Color( 255, 0, 0, 255 ), "FMOD FILESYSTEM ERROR: \"%s\"\n", FMOD_ErrorString( FMOD_ERR_FILE_EOF ) );
		return FMOD_ERR_FILE_EOF;
	}
	return FMOD_OK;
}

static FMOD_RESULT F_CALL USER_FMOD_FILE_SEEK_CALLBACK( void *handle, unsigned int pos, void *userdata )
{
	// We shouldn't get to the seek callback if the file handle is invalid, so we shouldn't worry about checking it
	FileHandle_t fileHandle = handle;
	g_pFullFileSystem->Seek( fileHandle, pos, FILESYSTEM_SEEK_HEAD );
	return FMOD_OK;
}

ConVar channel_steal_max( "nsnd_channel_steal_max", "1", FCVAR_NONE, "Number of channels that are longer than nsnd_channel_steal_length that we allow." );
ConVar channel_steal_length( "nsnd_channel_steal_length", "0.8", FCVAR_NONE, "Is a sound is longer than this it will be stolen." );

CEngineSoundClient g_EngineSoundClient;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineSoundClient, IEngineSound,
	IFMODENGINESOUND_CLIENT_INTERFACE_VERSION, g_EngineSoundClient );

CEngineSoundClient::CEngineSoundClient()
{
}

bool CEngineSoundClient::ProcessSoundMessage( NET_SoundMessage *msg )
{
	SoundInfo_t &soundInfo = msg->m_SoundInfo;

	float soundtime = soundInfo.fDelay + m_pGlobals->curtime;

	EmitSoundInternal( soundInfo.nEntityIndex, soundInfo.nChannel, msg->m_szSampleName,
		soundInfo.fVolume, soundInfo.Soundlevel, soundInfo.nFlags, soundInfo.nPitch,
		soundInfo.nSpecialDSP, &soundInfo.vOrigin, &soundInfo.vDirection, nullptr, false,
		soundtime, soundInfo.nSpeakerEntity, true );
	return true;
}

void CEngineSoundClient::Initialize( CreateInterfaceFn appSystemFactory, CreateInterfaceFn gameFactory, CGlobalVarsBase *globals )
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

void CEngineSoundClient::Shutdown()
{
	g_pFMODAudioEngine->Shutdown();

	ConVar_Unregister();
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

void CEngineSoundClient::Update( float frametime )
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
			g_pFMODAudioEngine->SetChannelPosition( channel.id, { origin.x, origin.z, -origin.y });
			g_pFMODAudioEngine->SetChannelMuted( channel.id, !bAudible );
		}
		else if ( !channel.fromServer )
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

	g_pFMODAudioEngine->Update(frametime);
}

void CEngineSoundClient::OnConnectedToServer()
{
	INetChannelInfo *ni = m_engineClient->GetNetChannelInfo();
	INetChannel *chan = (INetChannel *) ni;
	REGISTER_NET_MSG( SoundMessage );
}

void CEngineSoundClient::OnDisconnectedFromServer()
{
	StopAllSounds(false);
}

void CEngineSoundClient::SetAudioState( const AudioState_t &state )
{
	Vector vecForward, vecRight, vecUp;
	AngleVectors( state.m_Angles, &vecForward, &vecRight, &vecUp );
	g_pFMODAudioEngine->UpdateListenerPosition(
		{ state.m_Origin.x, state.m_Origin.z, -state.m_Origin.y },
		{ vecForward.x, vecForward.z, -vecForward.y},
		{ vecUp.x, vecUp.z, -vecUp.y }
	);
}

CSentence *CEngineSoundClient::GetSentence( CAudioSource *audioSource )
{
	return nullptr;
}

float CEngineSoundClient::GetSentenceLength( CAudioSource *audioSource )
{
	return 0.f;
}

struct SoundChannels
{
	int			channel;
	const char *name;
};

static SoundChannels g_pChannelNames[] =
{
	{ CHAN_AUTO, "CHAN_AUTO" },
	{ CHAN_WEAPON, "CHAN_WEAPON" },
	{ CHAN_VOICE, "CHAN_VOICE" },
	{ CHAN_ITEM, "CHAN_ITEM" },
	{ CHAN_BODY, "CHAN_BODY" },
	{ CHAN_STREAM, "CHAN_STREAM" },
	{ CHAN_STATIC, "CHAN_STATIC" },
	{ CHAN_VOICE2, "CHAN_VOICE2" },
};

static SoundChannels g_pFlagNames[] =
{
	{ SND_NOFLAGS, "SND_NOFLAGS" },
	{ SND_CHANGE_VOL, "SND_CHANGE_VOL" },
	{ SND_CHANGE_PITCH, "SND_CHANGE_PITCH" },
	{ SND_STOP, "SND_STOP" },
	{ SND_SPAWNING, "SND_SPAWNING" },
	{ SND_DELAY, "SND_DELAY" },
	{ SND_STOP_LOOPING, "SND_STOP_LOOPING" },
	{ SND_SPEAKER, "SND_SPEAKER" },
	{ SND_SHOULDPAUSE, "SND_SHOULDPAUSE" },
	{ SND_IGNORE_PHONEMES, "SND_IGNORE_PHONEMES" },
	{ SND_IGNORE_NAME, "SND_IGNORE_NAME" },
	{ SND_DO_NOT_OVERWRITE_EXISTING_ON_CHANNEL, "SND_DO_NOT_OVERWRITE_EXISTING_ON_CHANNEL" }
};

void CEngineSoundClient::EmitSoundInternal( int iEntIndex, int iChannel, const char *pSample,
	float flVolume, soundlevel_t iSoundlevel, int iFlags, int iPitch, int iSpecialDSP,
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector > *pUtlVecOrigins,
	bool bUpdatePositions, float soundtime, int speakerentity, bool fromServer )
{
	// if we don't have a sample, we can't do anything
	if ( !pSample || !pSample[0] )
		return;

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

	// some channels have limits and this is here to enforce that.
	// In the future this may be redundant if we stop doing retarded shit on the client.

	// if we find that a sound exists we should update it
	if( iFlags & ( SND_CHANGE_PITCH | SND_CHANGE_VOL) || !StompChannels( iEntity, iChannel, szSampleFull ) )
	{
		// if we successfully found and modified a sound/s, we're done
		bool modified = false;
		FOR_EACH_LL( m_activeChannels, i )
		{
			SoundChannel &channel = m_activeChannels[i];
			if ( channel.entityIndex == iEntIndex && channel.sourceChannelType == iChannel )
			{
				if ( g_pFMODAudioEngine->MatchesChannelName( channel.id, szSampleFull ) )
				{
					if( iFlags & SND_CHANGE_PITCH )
						g_pFMODAudioEngine->SetChannelPitch( channel.id, iPitch/100.f );
					if ( iFlags & SND_CHANGE_VOL )
						g_pFMODAudioEngine->SetChannelVolume( channel.id, flVolume );

					UpdateChannelPosition( channel, pOrigin );
					modified = true;
				}
			}
		}
		if ( modified )
		{
			return;
		}
	}

	// delay sounds that shouldn't be played yet
	/*if ( soundtime > m_pGlobals->curtime )
	{
		SoundInfo_t sndInfo;
		sndInfo.SetDefault();
		sndInfo.nEntityIndex = iEntIndex;
		sndInfo.nChannel = iChannel;
		sndInfo.pszName = _strdup( pSample );
		sndInfo.fVolume = flVolume;
		sndInfo.Soundlevel = iSoundlevel;
		sndInfo.nFlags = iFlags | ( fromServer ? SND_SPAWNING : 0 );
		sndInfo.nPitch = iPitch;
		sndInfo.nSpecialDSP = iSpecialDSP;
		sndInfo.fDelay = soundtime;
		sndInfo.nSpeakerEntity = speakerentity;
		if ( pOrigin )
		{
			sndInfo.vOrigin = *pOrigin;
		}
		if ( pDirection )
		{
			sndInfo.vDirection = *pDirection;
		}

		m_delayedSounds.AddToTail( sndInfo );
		return;
	}*/

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
	
	UpdateChannelPosition( channel, nullptr );

	g_pFMODAudioEngine->StartChannel( channelId );
}

bool CEngineSoundClient::StompChannels( int iEntIndex, int iChannel, const char *pSample )
{
	// We only stomp out non-world sounds with the following channels
	if ( iEntIndex == SOUND_FROM_WORLD && iChannel != CHAN_WEAPON )
	{
		return false;
	}

	if ( iChannel == CHAN_STATIC )
		return false;

	CUtlVector<int> oldSounds;
	FOR_EACH_LL( m_activeChannels, i )
	{
		SoundChannel &channel = m_activeChannels[i];
		if ( channel.entityIndex == iEntIndex && channel.sourceChannelType == iChannel )
		{
			// if the sound is playing and is longer than the steal length, remove it
			if ( g_pFMODAudioEngine->IsChannelPlaying( channel.id )
				/*&& source.m_soundChannel->GetLength() > channel_steal_length.GetFloat() */ )
			{
				// remove
				oldSounds.AddToTail( channel.id );
			}
		}
	}

	bool stomped = oldSounds.Count() > 0;
	// if we have too many sounds, remove the oldest ones. We should only ever be 1 over the limit
	// but better safe than sorry
	while ( oldSounds.Count() >= channel_steal_max.GetInt() )
	{
		// find the oldest and remove it
		int *oldest = &oldSounds[0];
		for ( int i = 1; i < oldSounds.Count(); i++ )
		{
			oldest = &oldSounds[i];
		}
		g_pFMODAudioEngine->StopChannel( *oldest );
		oldSounds.FindAndRemove( *oldest );
	}

	return stomped;
}

void CEngineSoundClient::UpdateChannelPosition( SoundChannel &channel, const Vector *pOrigin )
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
		g_pFMODAudioEngine->SetChannelMuted( channel.id, false);

	}
}

bool CEngineSoundClient::PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound )
{
	return true;
}

bool CEngineSoundClient::IsSoundPrecached( const char *pSample )
{
	return true;
}

void CEngineSoundClient::PrefetchSound( const char *pSample )
{
}

float CEngineSoundClient::GetSoundDuration( const char *pSample )
{
	return 0.0f;
}

void CEngineSoundClient::EmitSound( IRecipientFilter &filter, int iEntIndex, int iChannel, const char *pSample,
	float flVolume, float flAttenuation, int iFlags, int iPitch, int iSpecialDSP,
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector > *pUtlVecOrigins,
	bool bUpdatePositions, float soundtime, int speakerentity )
{
	EmitSoundInternal( iEntIndex, iChannel, pSample,
		flVolume, ATTN_TO_SNDLVL( flAttenuation ), iFlags, iPitch, iSpecialDSP,
		pOrigin, pDirection, pUtlVecOrigins,
		bUpdatePositions, soundtime, speakerentity );
}

void CEngineSoundClient::EmitSound( IRecipientFilter &filter, int iEntIndex, int iChannel, const char *pSample,
	float flVolume, soundlevel_t iSoundlevel, int iFlags, int iPitch, int iSpecialDSP,
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector > *pUtlVecOrigins,
	bool bUpdatePositions, float soundtime, int speakerentity )
{
	EmitSoundInternal( iEntIndex, iChannel, pSample,
		flVolume, iSoundlevel, iFlags, iPitch, iSpecialDSP,
		pOrigin, pDirection, pUtlVecOrigins,
		bUpdatePositions, soundtime, speakerentity );
}

void CEngineSoundClient::EmitSentenceByIndex( IRecipientFilter &filter, int iEntIndex, int iChannel, int iSentenceIndex,
	float flVolume, soundlevel_t iSoundlevel, int iFlags, int iPitch, int iSpecialDSP,
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector > *pUtlVecOrigins,
	bool bUpdatePositions, float soundtime, int speakerentity )
{
}

void CEngineSoundClient::StopSound( int iEntIndex, int iChannel, const char *pSample )
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

void CEngineSoundClient::StopAllSounds( bool bClearBuffers )
{
	// update will handle clean-up
	g_pFMODAudioEngine->StopAllChannels();
}

void CEngineSoundClient::SetRoomType( IRecipientFilter &filter, int roomType )
{
}

void CEngineSoundClient::SetPlayerDSP( IRecipientFilter &filter, int dspType, bool fastReset )
{
}

void CEngineSoundClient::EmitAmbientSound( const char *pSample, float flVolume, int iPitch, int flags, float soundtime )
{
}

float CEngineSoundClient::GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
{
	// TODO reverse engineer me
	return m_oldEngineSound->GetDistGainFromSoundLevel( soundlevel, dist );
}

int	CEngineSoundClient::GetGuidForLastSoundEmitted()
{
	return g_pFMODAudioEngine->GetLastGUID();
}

bool CEngineSoundClient::IsSoundStillPlaying( int guid )
{
	return g_pFMODAudioEngine->IsChannelPlaying( guid );
}

void CEngineSoundClient::StopSoundByGuid( int guid )
{
	g_pFMODAudioEngine->StopChannel( guid );
}

void CEngineSoundClient::SetVolumeByGuid( int guid, float fvol )
{
	g_pFMODAudioEngine->SetChannelVolume( guid, fvol );
}

void CEngineSoundClient::GetActiveSounds( CUtlVector< SndInfo_t > &sndlist )
{
}

void CEngineSoundClient::PrecacheSentenceGroup( const char *pGroupName )
{
}

void CEngineSoundClient::NotifyBeginMoviePlayback()
{
}

void CEngineSoundClient::NotifyEndMoviePlayback()
{
}

IAudioOutputStream *CEngineSoundClient::CreateOutputStream( uint nSampleRate, uint nChannels, uint nBits )
{
	return nullptr;
}

void CEngineSoundClient::DestroyOutputStream( IAudioOutputStream *pStream )
{
}

void CEngineSoundClient::ManualUpdate( const AudioState_t *pListenerState )
{
}

void CEngineSoundClient::ExtraUpdate()
{
}
