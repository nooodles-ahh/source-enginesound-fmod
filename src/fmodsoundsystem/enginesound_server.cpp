#include "enginesound_server.h"
#include <tier1/tier1.h>
#include "sound_netmessages.h"

CEngineSoundServer g_EngineSoundServer;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineSoundServer, IFMODEngineSound,
	IFMODENGINESOUND_SERVER_INTERFACE_VERSION, g_EngineSoundServer );

CEngineSoundServer::CEngineSoundServer()
{
}

void CEngineSoundServer::Initialize( CreateInterfaceFn appSystemFactory, CreateInterfaceFn gameFactory, CGlobalVarsBase *globals )
{
	MathLib_Init();
	ConnectTier1Libraries( &appSystemFactory, 1 );
	m_engineServer = (IVEngineServer *) appSystemFactory( INTERFACEVERSION_VENGINESERVER, NULL );
	m_oldEngineSound = (IEngineSound *) appSystemFactory( IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL );
	m_pGlobals = globals;

	m_server = m_engineServer->GetIServer();
}

void CEngineSoundServer::Shutdown()
{
	DisconnectTier1Libraries();
}

void CEngineSoundServer::OnClientConnected( edict_t *pEntity )
{
}

void CEngineSoundServer::OnServerActivate()
{
	m_server = m_engineServer->GetIServer();
}

void CEngineSoundServer::EmitAmbientSound( int entindex, const Vector &pos, const char *samp,
	float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay )
{
}

bool CEngineSoundServer::PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound )
{
	return true;
}

bool CEngineSoundServer::IsSoundPrecached( const char *pSample )
{
	return true;
}

void CEngineSoundServer::PrefetchSound( const char *pSample )
{
}

float CEngineSoundServer::GetSoundDuration( const char *pSample )
{
	return 0.0f;
}

void CEngineSoundServer::EmitSound( IRecipientFilter &filter, int iEntIndex, int iChannel, const char *pSample,
	float flVolume, float flAttenuation, int iFlags, int iPitch, int iSpecialDSP,
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector > *pUtlVecOrigins,
	bool bUpdatePositions, float soundtime, int speakerentity )
{
	// soundlevel is sent over the network so use the other function instead
	EmitSound( filter, iEntIndex, iChannel, pSample, flVolume, ATTN_TO_SNDLVL( flAttenuation ), iFlags, iPitch,
		iSpecialDSP, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
}

void CEngineSoundServer::EmitSound( IRecipientFilter &filter, int iEntIndex, int iChannel, const char *pSample,
	float flVolume, soundlevel_t iSoundlevel, int iFlags, int iPitch, int iSpecialDSP,
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector > *pUtlVecOrigins,
	bool bUpdatePositions, float soundtime, int speakerentity )
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

void CEngineSoundServer::EmitSentenceByIndex( IRecipientFilter &filter, int iEntIndex, int iChannel, int iSentenceIndex,
	float flVolume, soundlevel_t iSoundlevel, int iFlags, int iPitch, int iSpecialDSP,
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector > *pUtlVecOrigins,
	bool bUpdatePositions, float soundtime, int speakerentity )
{
	// TODO
}

void CEngineSoundServer::StopSound( int iEntIndex, int iChannel, const char *pSample )
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

void CEngineSoundServer::SetRoomType( IRecipientFilter &filter, int roomType )
{
}

void CEngineSoundServer::SetPlayerDSP( IRecipientFilter &filter, int dspType, bool fastReset )
{
}

float CEngineSoundServer::GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
{
	// TODO reverse engineer me
	return m_oldEngineSound->GetDistGainFromSoundLevel(soundlevel, dist);
}