#ifndef SOUND_NETMESSAGES_H
#define SOUND_NETMESSAGES_H
#pragma once

#include "inetchannel.h"
#include "inetmessage.h"
#include "inetmsghandler.h" 
#include "soundinfo.h"
#include <tier1/strtools.h>

class NET_SoundMessage;
class ISoundMessageHandler
{
public:
	PROCESS_NET_MESSAGE(SoundMessage) = 0;
};

class NET_SoundMessage : public INetMessage
{
public:
	NET_SoundMessage()
	{
		m_SoundInfo.SetDefault();
		m_bReliable = true;
		m_pNetChannel = nullptr;
		m_pMessageHandler = nullptr;
		m_szSampleName[0] = '\0';
	}

	NET_SoundMessage(const SoundInfo_t& soundInfo)
	{
		m_SoundInfo = soundInfo;
		m_bReliable = true;
		m_pNetChannel = nullptr;
		m_pMessageHandler = nullptr;
		V_sprintf_safe(m_szSampleName, "%s", soundInfo.pszName);
	}

	// INetMessage implementation
public:
	virtual void			SetNetChannel(INetChannel* netchan) { m_pNetChannel = netchan; }
	virtual void			SetReliable(bool state) { m_bReliable = state; }

	virtual bool Process(void) { return m_pMessageHandler->ProcessSoundMessage(this); }

	virtual bool ReadFromBuffer(bf_read& buffer)
	{
		SoundInfo_t delta;
		delta.SetDefault();
		buffer.ReadString(m_szSampleName, sizeof(m_szSampleName));
		m_SoundInfo.ReadDelta(&delta, buffer, 22 /*proto version*/);
		return !buffer.IsOverflowed();
	}

	virtual bool WriteToBuffer(bf_write& buffer)
	{
		SoundInfo_t delta;
		delta.SetDefault();
		buffer.WriteUBitLong(GetType(), 6); // assuming this is unsigned for now
		buffer.WriteString(m_szSampleName);
		m_SoundInfo.WriteDelta(&delta, buffer);
		return !buffer.IsOverflowed();
	}

	virtual bool			IsReliable(void) const { return m_bReliable; }
	virtual int				GetType(void) const { return 34; } // first available type id
	virtual int				GetGroup(void) const { return INetChannelInfo::SOUNDS; } // this is probably for profiling
	virtual const char* GetName(void) const { return "NET_SoundMessage"; }
	virtual INetChannel* GetNetChannel(void) const { return m_pNetChannel; }
	virtual const char* ToString(void) const { return ""; } // TODO

	virtual bool	BIncomingMessageForProcessing( double dblNetTime, int numBytes ) { return true; }
	virtual size_t			GetSize() const { return sizeof( SoundInfo_t ) + sizeof( m_szSampleName ); }

public:
	ISoundMessageHandler* m_pMessageHandler;
private:
	bool m_bReliable;
	INetChannel* m_pNetChannel;
public:
	SoundInfo_t m_SoundInfo;
	char m_szSampleName[256];
};

#endif