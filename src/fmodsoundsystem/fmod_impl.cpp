//====================================================================
// Written by Nooodles (nooodlesahh@protonmail.com)
// 
// Purpose: FMOD engine
//====================================================================
#include "fmod_impl.h"
#include <map>
#include <vector>
#include <string>
#include <assert.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_errors.h>
#include <fmod_studio/fmod_studio.hpp>
#include <stdarg.h>

using namespace FMOD;

constexpr bool IsSoundSDK = true;
constexpr float SourceUnitsPerMeter = 52.49344f;

void DefaultLogFunction( const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	vprintf( fmt, args );
	va_end( args );
}
static LOG_FUNCTION Log = DefaultLogFunction;

class CFMODAudioEngine : public IFMODAudioEngine
{
	FMOD::System *m_pSystem;
	FMOD::Studio::System *m_pStudioSystem;
	FMOD::ChannelGroup *m_pMasterChannelGroup;

	std::map<std::string, FMOD::Sound *> m_loadedSounds;
	std::map<int, FMOD::Channel *> m_channels;

	int m_lastGUID;

	FMOD_3D_ATTRIBUTES m_listenerAttribs;

public:
	CFMODAudioEngine()
	{

	}

	virtual bool Init( FMOD_MEMORY_ALLOC_CALLBACK useralloc,
		FMOD_MEMORY_REALLOC_CALLBACK userrealloc, FMOD_MEMORY_FREE_CALLBACK userfree,
		FMOD_FILE_OPEN_CALLBACK useropen, FMOD_FILE_CLOSE_CALLBACK userclose,
		FMOD_FILE_READ_CALLBACK userread, FMOD_FILE_SEEK_CALLBACK userseek,
		LOG_FUNCTION logfunc )
	{
		if ( logfunc )
			Log = logfunc;

		if ( FMOD_RESULT result = FMOD::Memory_Initialize( nullptr, 0, useralloc, userrealloc, userfree ) )
		{
			Log( "FMOD Error: Memory_Initialize failed: %s\n", FMOD_ErrorString(result) );
			return false;
		}
		
		if ( FMOD_RESULT result = FMOD::Studio::System::create( &m_pStudioSystem ) )
		{
			Log( "FMOD Error: Studio::System::create failed: %s\n", FMOD_ErrorString( result ) );
			return false;
		}

		if ( FMOD_RESULT result = m_pStudioSystem->initialize( 1024, FMOD_STUDIO_INIT_LIVEUPDATE | FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE,
			FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED, 0 ) )
		{
			Log( "FMOD Error: Studio::System::initialize failed: %s\n", FMOD_ErrorString( result ) );
			return false;
		}

		if ( FMOD_RESULT result = m_pStudioSystem->getCoreSystem( &m_pSystem ) )
		{
			Log( "FMOD Error: Studio::System::getCoreSystem failed: %s\n", FMOD_ErrorString( result ) );
			return false;
		}

		if ( FMOD_RESULT result = m_pSystem->setFileSystem( useropen, userclose, userread, userseek, nullptr, nullptr, -1 ) )
		{
			Log( "FMOD Error: System::setFileSystem failed: %s\n", FMOD_ErrorString( result ) );
			return false;
		}

		m_pSystem->getMasterChannelGroup( &m_pMasterChannelGroup );
		m_pSystem->set3DNumListeners( 1 );

		if ( IsSoundSDK )
		{
			// A unit in source is about an inch. This will affect the doppler and mindist for sounds
			m_pSystem->set3DSettings( 1.f, SourceUnitsPerMeter, 1.f );
		}

		return true;
	}

	virtual void Shutdown()
	{
	}

	virtual void Update( float dt )
	{
		std::vector< std::map<int, FMOD::Channel *>::iterator > vecStoppedChannels;
		for ( auto it = m_channels.begin(), itEnd = m_channels.end(); it != itEnd; ++it )
		{
			bool isPlaying = false;
			it->second->isPlaying( &isPlaying );
			if ( !isPlaying )
				vecStoppedChannels.push_back( it );
		}

		for ( auto &it : vecStoppedChannels )
		{
			m_channels.erase( it );
		}

		m_pStudioSystem->update();
	}

	virtual void LoadSound( const char *soundName, bool isStream, bool is3d )
	{
		auto soundIt = m_loadedSounds.find( soundName );
		if ( soundIt != m_loadedSounds.end() )
			return;

		FMOD_MODE mode = FMOD_IGNORETAGS;
		mode |= ( isStream ? FMOD_CREATESTREAM : FMOD_CREATESAMPLE );
		mode |= is3d * ( FMOD_3D | FMOD_3D_INVERSEROLLOFF );

		FMOD::Sound *pSound = nullptr;
		if ( FMOD_RESULT result = m_pSystem->createSound( soundName, mode, nullptr, &pSound ) )
		{
			// insert an empty sound so we don't try to load it again
			Log( "FMOD Error: System::createSound failed: %s %s\n", FMOD_ErrorString( result ), soundName );
			m_loadedSounds[soundName] = nullptr;
			return;
		}

		m_loadedSounds[soundName] = pSound;

		if ( IsSoundSDK )
		{
			// Source uses markers to indicate if a sound is loopable
			int syncPointCount;
			pSound->getNumSyncPoints( &syncPointCount );
			if ( syncPointCount > 0 )
			{
				assert( syncPointCount == 1 ); // expecting only one marker
				FMOD_SYNCPOINT *pSyncPoint;
				pSound->getSyncPoint( 0, &pSyncPoint );
				unsigned int syncPointOffset;
				pSound->getSyncPointInfo( pSyncPoint, nullptr, 0, &syncPointOffset, FMOD_TIMEUNIT_MS );

				unsigned int uLength;
				pSound->getLength( &uLength, FMOD_TIMEUNIT_MS );

				// mark sound as loopable
				mode &= ~FMOD_LOOP_OFF;
				pSound->setMode( mode | FMOD_LOOP_NORMAL );
				pSound->setLoopPoints( syncPointOffset, FMOD_TIMEUNIT_MS, uLength, FMOD_TIMEUNIT_MS );
				pSound->setLoopCount( -1 );
			}
		}
	}

	virtual void UnloadSound( const char *soundName ) 
	{
	}

	virtual void SetVolume( float volume ) 
	{
		m_pMasterChannelGroup->setVolume( volume );
	}

	virtual void StopAllChannels() 
	{
	}

	virtual int GetLastGUID() const
	{
		return m_lastGUID;
	}

	virtual void UpdateListenerPosition( const SoundVector &position, const SoundVector &forward, const SoundVector &up )
	{
		m_listenerAttribs.position = *( static_cast<FMOD_VECTOR *>( (void *) &position ) );
		m_listenerAttribs.forward = *( static_cast<FMOD_VECTOR *>( (void *) &forward ) );
		m_listenerAttribs.up = *( static_cast<FMOD_VECTOR *>( (void *) &up ) );
		m_listenerAttribs.velocity = { 0, 0, 0 };
		m_pStudioSystem->setListenerAttributes( 0, &m_listenerAttribs );
	}

	virtual int PlaySound( const char *soundName, float volume, const SoundVector &position, const SoundVector &angle, bool startPaused )
	{
		auto soundIt = m_loadedSounds.find( soundName );
		if ( soundIt == m_loadedSounds.end() )
		{
			Log( "Late load of \"%s\". Sound may not have correct attributes\n", soundName );
			LoadSound( soundName, false, true );
			soundIt = m_loadedSounds.find( soundName );
			if ( soundIt == m_loadedSounds.end() )
			{
				Log( "Unable to play sound \"%s\". Sound not loaded", soundName );
				return -1;
			}
		}

		FMOD::Channel *channel = nullptr;
		if ( FMOD_RESULT result = m_pSystem->playSound( soundIt->second, m_pMasterChannelGroup, true, &channel ) )
		{
			Log( "FMOD Error: System::playSound failed: %s\n", FMOD_ErrorString( result ) );
			return -1;
		}

		const int channelId = ++m_lastGUID;
		channel->setVolume( volume );
		FMOD_VECTOR vec = *( static_cast<FMOD_VECTOR *>( (void *) &position ) );
		channel->set3DAttributes( &vec, nullptr );
		channel->setPaused( startPaused );

		m_channels[channelId] = channel;

		return channelId;
	}

	virtual void StartChannel( int channelId ) 
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
			channelIt->second->setPaused( false );
	}

	virtual void StopChannel( int channelId ) 
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			channelIt->second->stop();
		}
	}

	virtual void SetChannelPosition( int channelId, const SoundVector &position ) 
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			FMOD_VECTOR vec = *( static_cast<FMOD_VECTOR *>( (void *) &position ) );
			channelIt->second->set3DAttributes( &vec, nullptr );
		}
	}

	virtual void SetChannelVolume( int channelId, float volume ) 
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			channelIt->second->setVolume( volume );
		}
	}

	virtual void SetChannelMuted( int channelId, bool muted )
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			channelIt->second->setMute( muted );
		}
	}

	virtual void SetChannelPitch( int channelId, float pitch ) 
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			channelIt->second->setPitch( pitch );
		}
	}

	virtual bool IsChannelPlaying( int channelId ) 
	{
		bool isPlaying = false;
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			channelIt->second->isPlaying( &isPlaying );
		}
		return isPlaying;
	}

	virtual bool MatchesChannelName( int channelId, const char *name )
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			FMOD::Sound *pSound = nullptr;
			channelIt->second->getCurrentSound( &pSound );
			if ( pSound )
			{
				auto it = m_loadedSounds.find( name );
				if( it != m_loadedSounds.end() && it->second == pSound )
					return true;
			}
		}
		return false;
	}

	virtual float GetChannelDuration( int channelId )
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			FMOD::Sound *sound = nullptr;
			channelIt->second->getCurrentSound( &sound );
			if ( sound )
			{
				unsigned int length;
				sound->getLength( &length, FMOD_TIMEUNIT_MS );
				return length / 1000.f;
			}
		}
		return 0.f;
	}

	virtual float GetChannelPlaybackPosition( int channelId )
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			unsigned position = 0;
			channelIt->second->getPosition( &position, FMOD_TIMEUNIT_MS );
			return position/1000.f;
		}
		return 0.f;
	}

	virtual void SetChannelPlaybackPosition( int channelId, float flTime )
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			unsigned position = (unsigned) ( flTime * 1000.f );
			channelIt->second->setPosition( position, FMOD_TIMEUNIT_MS );
		}
	}

	virtual void SetChannelMinMaxDist( int channelId, float min, float max )
	{
		auto channelIt = m_channels.find( channelId );
		if ( channelIt != m_channels.end() )
		{
			channelIt->second->set3DMinMaxDistance( min, max );
		}
	}
};

CFMODAudioEngine g_FMODAudioEngine;
IFMODAudioEngine *g_pFMODAudioEngine = &g_FMODAudioEngine;