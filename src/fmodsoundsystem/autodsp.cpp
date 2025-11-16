//====================================================================
// Written by Nooodles (nooodlesahh@protonmail.com)
// 
// Purpose: Re-implementation of Automatic DSP. Determines the 
// characteristics of a space and uses that to select and modify an
// existing DSP/reverb preset.
//====================================================================
#include <tier1.h>
#include "autodsp.h"
#include "vphysics_interface.h"
#include "engine/IEngineTrace.h"
#include "engine/ivdebugoverlay.h"
#include "cmodel.h"
#include "worldsize.h"
#include "Color.h"
#include "tier2/tier2.h"
#include "tier2/renderutils.h"

ConVar *adsp_debug = nullptr;

IPhysicsSurfaceProps *physprops = NULL;
IEngineTrace *enginetrace = NULL;
IVDebugOverlay *debugoverlay = NULL;

DynamicReverbSpace CAutoDSP::GetRoomType( Vector &size, float &skyVisibility )
{
	float width = size.x;
	float depth = size.y;
	float height = size.z;

	float skyVisiblity = skyVisibility;

	float sideA = MAX( 1, width );
	float sideB = MAX( 1, depth );
	if ( sideA < sideB )
		V_swap<float>( sideA, sideB );

	constexpr float RoomRatio = 2.5f;
	constexpr float TunnelRatio = 4.0f;
	constexpr float SkyFactor = 0.7f;

	float spaceRatio = sideA / sideB;

	// outside
	if ( skyVisiblity > SkyFactor )
	{
		// TODO
		return DynamicReverbSpace::ReverbOpenSpace;
	}
	// inside
	else
	{
		// TODO what is considered a duct?

		// is this boxy?
		if ( spaceRatio > RoomRatio )
		{
			if ( sideB <= 96 )
			{
				return DynamicReverbSpace::ReverbHall;
			}
			else if ( spaceRatio >= TunnelRatio )
			{
				return DynamicReverbSpace::ReverbTunnel;
			}
		}

		// it's just a fucking room
		return DynamicReverbSpace::ReverbRoom;
	}
}

const Vector VecUp( 0.f, 0.f, 1.f );
const Vector VecDown( 0.f, 0.f, -1.f );
const Vector VecForward( 0.f, 1.f, 0.f );
const Vector VecBack( 0.f, -1.f, 0.f );
const Vector VecLeft( -1.f, 0.f, 0.f );
const Vector VecRight( 1.f, 0.f, 0.f );

void CAutoDSP::GetSpaceSize( Vector &startPos, Vector &size, float &reflectivity )
{
	CTraceFilterWorldOnly filter;
	float totalReflectivity = 0.f;

	Vector vecStart = startPos;

	// trace to the floor so we can then trace from so many units above the ground
	{
		Ray_t ray;
		ray.Init( startPos, startPos + ( VecDown * MAX_TRACE_LENGTH ) );

		trace_t tr;
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &filter, &tr );
		if ( tr.DidHit() )
		{
			vecStart = tr.endpos + Vector( 0, 0, 128 );
		}
	}

	float dists[12] = { 0.f };
	for ( int i = 0; i < 12; ++i )
	{
		Vector vecDir;
		VectorYawRotate( VecForward, ( 360.f / 12.f ) * float( i ), vecDir );
		Ray_t ray;
		ray.Init( vecStart, vecStart + ( vecDir * MAX_TRACE_LENGTH ) );

		trace_t tr;
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &filter, &tr );
		dists[i] = tr.fraction * MAX_TRACE_LENGTH;
		surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
		if ( psurf )
			totalReflectivity = psurf->audio.reflectivity;

		if ( adsp_debug->GetBool() )
			debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 0, 255, 255, false, 10 );
	}

	// find the longest set
	float longest = 0.f;
	int longestIdx = 0;
	for ( int i = 0; i < 6; ++i )
	{
		float dist = dists[i] + dists[i + 6];
		if ( dist > longest )
		{
			longest = dist;
			longestIdx = i;
		}
	}
	// Average the neighbouring traces
	size.x = (
		( dists[longestIdx] + dists[longestIdx + 6] ) +
		( dists[( longestIdx + 13 ) % 12] + dists[( longestIdx + 5 ) % 12] ) +
		( dists[( longestIdx + 1 ) % 12] + dists[( longestIdx + 7 ) % 12] )
		) / 3.f;

	size.y = (
		( dists[longestIdx + 3] + dists[( longestIdx + 3 + 6 ) % 12] ) +
		( dists[( longestIdx + 3 + 13 ) % 12] + dists[( longestIdx + 3 + 5 ) % 12] ) +
		( dists[( longestIdx + 3 + 1 ) % 12] + dists[( longestIdx + 3 + 7 ) % 12] )
		) / 3.f;
	// 

	size.z = 0.f;

	reflectivity = totalReflectivity / 12.f;
}

const Vector skyDirections[11] =
{
	VecUp,
	VecUp + VecUp + VecForward,
	VecUp + VecUp + VecBack,
	VecUp + VecUp + VecLeft,
	VecUp + VecUp + VecRight,
	VecUp + VecForward + VecLeft,
	VecUp + VecForward + VecRight,
	VecUp + VecForward + VecLeft + VecForward + VecLeft,
	VecUp + VecForward + VecRight + VecForward + VecRight,
	VecUp + VecUp + VecBack + VecLeft,
	VecUp + VecUp + VecBack + VecRight,
};

float CAutoDSP::GetSkyVisibility( Vector &startPos )
{
	const Vector boxMins( -1, -1, -1 );
	const Vector boxMaxs( 1, 1, 1 );
	CTraceFilterWorldOnly filter;
	float skyTotal = 0.f;
	for ( int i = 0; i < ARRAYSIZE( skyDirections ); ++i )
	{
		const Vector vecDir = skyDirections[i].Normalized();
		Ray_t ray;
		ray.Init( startPos, startPos + ( vecDir * MAX_TRACE_LENGTH ) );

		trace_t tr;
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &filter, &tr );
		if ( tr.DidHit() && ( tr.surface.flags & SURF_SKY ) )
		{
			skyTotal += 1.f;
			if ( adsp_debug->GetBool() )
			{
				debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 255, 0, 0, false, 10 );
				debugoverlay->AddBoxOverlay2( tr.endpos, boxMins, boxMaxs, { 0, 0, 0 }, Color( 0, 0, 0, 0 ), Color( 255, 0, 0, 255 ), 10 );
			}
		}
		else
		{
			if ( adsp_debug->GetBool() )
			{
				debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 0, 255, 255, false, 10 );
				debugoverlay->AddBoxOverlay2( tr.endpos, boxMins, boxMaxs, { 0, 0, 0 }, Color( 0, 0, 0, 0 ), Color( 0, 255, 255, 255 ), 10 );
			}
		}
	}

	const float skyVisibility = skyTotal / ARRAYSIZE( skyDirections );
	return skyVisibility;
}

void CAutoDSP::Init( CreateInterfaceFn appSystemFactory, CreateInterfaceFn physicsFactory )
{
	enginetrace = (IEngineTrace *) appSystemFactory( INTERFACEVERSION_ENGINETRACE_CLIENT, NULL );
	debugoverlay = (IVDebugOverlay *) appSystemFactory( VDEBUG_OVERLAY_INTERFACE_VERSION, NULL );
	physprops = (IPhysicsSurfaceProps *) physicsFactory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL );

	adsp_debug = g_pCVar->FindVar( "adsp_debug" );
}

void CAutoDSP::CategoriseSpace( Vector listenerPos, float &reflectivity, float &spaceSize, DynamicReverbSpace &roomType )
{
	float skyVisibility = GetSkyVisibility( listenerPos );
	Vector size = { 0, 0, 0 };
	GetSpaceSize( listenerPos, size, reflectivity );
	spaceSize = ( size.x / 12.f ) * ( size.y / 12.f ); // feet cubed
	roomType = GetRoomType( size, skyVisibility );
}