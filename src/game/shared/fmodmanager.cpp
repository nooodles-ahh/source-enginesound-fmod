#include "cbase.h"
#include "fmodmanager.h"
#include <icommandline.h>
#ifdef CLIENT_DLL
#include "client_factorylist.h"
#include "isaverestore.h"
#else
#include "init_factory.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CFMODManager g_FMODManager;
CFMODManager*g_pFMODManager = &g_FMODManager;

CFMODManager::CFMODManager() : CAutoGameSystemPerFrame("CFMODManager")
{
}

CFMODManager::~CFMODManager()
{
}

bool CFMODManager::Init()
{
	char szPath[MAX_PATH];
	V_sprintf_safe(szPath, "%s\\bin" PLATFORM_DIR "\\fmodsoundsystem.dll", CommandLine()->ParmValue("-game", "hl2"));
	m_hFMODModule = Sys_LoadModule(szPath);
	if (!m_hFMODModule)
		return false;

	CreateInterfaceFn factory = Sys_GetFactory(m_hFMODModule);
#ifdef CLIENT_DLL
	m_pFMODSystem = (IFMODEngineSound *) factory( IFMODENGINESOUND_CLIENT_INTERFACE_VERSION, nullptr );
#else
	m_pFMODSystem = (IFMODEngineSound *) factory( IFMODENGINESOUND_SERVER_INTERFACE_VERSION, nullptr );
#endif

	if (!m_pFMODSystem)
		return false;

	factorylist_t factories;
	FactoryList_Retrieve(factories);
#ifdef CLIENT_DLL
	m_pFMODSystem->Initialize( factories.appSystemFactory, Sys_GetFactoryThis(), gpGlobals );
#else
	m_pFMODSystem->Initialize(factories.engineFactory, Sys_GetFactoryThis(), gpGlobals);
#endif
	enginesound = m_pFMODSystem;
	return true;
}

void CFMODManager::Shutdown()
{
	m_pFMODSystem->Shutdown();
	Sys_UnloadModule(m_hFMODModule);
}

void CFMODManager::LevelInitPreEntity()
{
#ifdef CLIENT_DLL
	m_pFMODSystem->OnConnectedToServer();
#else
	m_pFMODSystem->OnServerActivate();
#endif
}

void CFMODManager::LevelShutdownPreEntity()
{
#ifdef CLIENT_DLL
	m_pFMODSystem->OnDisconnectedFromServer();
#endif
}

void CFMODManager::OnSave()
{
}

void CFMODManager::OnRestore()
{
}

#ifdef CLIENT_DLL
void CFMODManager::Update(float frametime)
{
	m_pFMODSystem->Update(frametime);
}

void CFMODManager::SetAudioState(AudioState_t state)
{
	m_pFMODSystem->SetAudioState(state);
}

CSentence* CFMODManager::GetSentence(CAudioSource* audioSource)
{
	return m_pFMODSystem->GetSentence(audioSource);
}

float CFMODManager::GetSentenceLength(CAudioSource* audioSource)
{
	return m_pFMODSystem->GetSentenceLength(audioSource);
}

#else
void CFMODManager::OnClientConnect(edict_t* pEntity)
{
	m_pFMODSystem->OnClientConnected(pEntity);
}

void CFMODManager::EmitAmbientSound(int entindex, const Vector& pos, const char* samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay)
{
	m_pFMODSystem->EmitAmbientSound(entindex, pos, samp, vol, soundlevel, fFlags, pitch, delay);
}
#endif