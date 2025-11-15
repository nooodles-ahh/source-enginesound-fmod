#pragma once

#include <fmod.hpp>

typedef void( _stdcall *LOG_FUNCTION )      ( const char *fmt, ... );

struct SoundVector
{
	float x;
	float y;
	float z;
};

enum DynamicReverbSpace
{
	Room,
	Hall,
	Tunnel,
	Street,
	Alley,
	Courtyard,
	OpenSpace,

	Count,
};

class IFMODAudioEngine
{
public:
	virtual bool Init( FMOD_MEMORY_ALLOC_CALLBACK useralloc = nullptr,
		FMOD_MEMORY_REALLOC_CALLBACK userrealloc = nullptr, FMOD_MEMORY_FREE_CALLBACK userfree = nullptr,
		FMOD_FILE_OPEN_CALLBACK useropen = nullptr, FMOD_FILE_CLOSE_CALLBACK userclose = nullptr,
		FMOD_FILE_READ_CALLBACK userread = nullptr, FMOD_FILE_SEEK_CALLBACK userseek = nullptr,
		LOG_FUNCTION logfunc = nullptr ) = 0;
	virtual void Shutdown() = 0;
	virtual void Update( float dt ) = 0;

	virtual void LoadSound( const char *soundName, bool isStream, bool is3d ) = 0;
	virtual void UnloadSound( const char *soundName ) = 0;
	virtual void SetVolume( float volume ) = 0;
	virtual void StopAllChannels() = 0;
	virtual int GetLastGUID() const = 0;

	virtual void UpdateListenerPosition( const SoundVector &position, const SoundVector &forward, const SoundVector &up ) = 0;
	virtual int PlaySound( const char *soundName, float volume, const SoundVector &position, const SoundVector &angle, bool startPaused ) = 0;

	virtual int GetSnapshotGUID( const char *snapshotName ) = 0;
	virtual void StartSnapshot( int guid ) = 0;
	virtual void StopSnapshot( int guid ) = 0;

	virtual void UpdateDynamicReverb( DynamicReverbSpace spaceType, float reflectivity, float size ) = 0;

	virtual void StartChannel( int channelId ) = 0;
	virtual void StopChannel( int channelId ) = 0;
	virtual void SetChannelPosition( int channelId, const SoundVector &position ) = 0;
	virtual void SetChannelVolume( int channelId, float volume ) = 0;
	virtual void SetChannelMuted( int channelId, bool muted ) = 0;
	virtual void SetChannelPitch( int channelId, float pitch ) = 0;
	virtual bool IsChannelPlaying( int channelId ) = 0;
	virtual bool MatchesChannelName( int channelId, const char *name ) = 0;
	virtual float GetChannelDuration( int channelId ) = 0;
	virtual float GetChannelPlaybackPosition( int channelId ) = 0;
	virtual void SetChannelPlaybackPosition( int channelId, float flTime ) = 0;
	virtual void SetChannelMinMaxDist( int channelId, float min, float max ) = 0;
};

extern IFMODAudioEngine *g_pFMODAudioEngine;