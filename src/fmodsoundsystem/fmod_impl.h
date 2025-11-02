#pragma once

#include <DirectXMath.h>
#include <fmod.hpp>

typedef void        ( _stdcall *LOG_FUNCTION )      ( const char *fmt, ... );

struct SoundVector
{
	float x;
	float y;
	float z;
};

class IFMODAudioEngine
{
public:
	virtual bool Init( FMOD_MEMORY_ALLOC_CALLBACK useralloc = nullptr, 
		FMOD_MEMORY_REALLOC_CALLBACK userrealloc = nullptr, FMOD_MEMORY_FREE_CALLBACK userfree = nullptr,
		FMOD_FILE_OPEN_CALLBACK useropen = nullptr, FMOD_FILE_CLOSE_CALLBACK userclose = nullptr, 
		FMOD_FILE_READ_CALLBACK userread = nullptr, FMOD_FILE_SEEK_CALLBACK userseek = nullptr,
		LOG_FUNCTION logfunc = nullptr) = 0;
	virtual void Shutdown() = 0;
	virtual void Update(float dt) = 0;

	virtual void LoadSound( const char *soundName, bool isStream, bool is3d ) = 0;
	virtual void UnloadSound( const char *soundName ) = 0;
	virtual void SetVolume(float volume) = 0;
	virtual void StopAllChannels() = 0;
	virtual int GetLastGUID() const = 0;

	virtual void UpdateListenerPosition( const SoundVector &position, const SoundVector &forward, const SoundVector &up ) = 0;
	virtual int PlaySound( const char *soundName, float volume, const SoundVector &position, const SoundVector &angle, bool startPaused ) = 0;

	virtual void StartChannel( int channelId ) = 0;
	virtual void StopChannel( int channelId ) = 0;
	virtual void SetChannelPosition( int channelId, const SoundVector &position ) = 0;
	virtual void SetChannelVolume( int channelId, float volume ) = 0;
	virtual void SetChannelMuted( int channelId, bool muted ) = 0;
	virtual void SetChannelPitch( int channelId, float pitch) = 0;
	virtual bool IsChannelPlaying( int channelId ) = 0;
	virtual bool MatchesChannelName( int channelId, const char* name ) = 0;
};

extern IFMODAudioEngine *g_pFMODAudioEngine;