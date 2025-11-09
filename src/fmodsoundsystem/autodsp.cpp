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

IPhysicsSurfaceProps *physprops = NULL;
IEngineTrace *enginetrace = NULL;
IVDebugOverlay *debugoverlay = NULL;

enum RoomType
{
	Room = 0,
	Duct,
	Hall,
	Tunnel,
	Street,
	Alley,
	Courtyard,
	OpenSpace,
	OpenWall,
	OpenStreet,
	OpenCourtyard,
};

int CAutoDSP::GetRoomType(Vector &size, float &reflectivity, float &skyVisibility )
{
	float width, depth, height;
	width = depth = height = 0.f;

	float skyVisiblity = 0.f;

	float sideA = MAX( 1, width );
	float sideB = MAX( 1, depth );
	if( sideA > sideB )
		V_swap<float>( sideA, sideB );

	float roomRatio = sideA / sideB;

	// outside
	if ( skyVisiblity > 0.75f )
	{
		// street
		// alley
		// courtyard
		// openspace
		// openwall
		// openstreet
		// opencourtyard
	}
	// inside
	else
	{
		// room
		// duct
		// hall
		// tunnel
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
		VectorYawRotate( VecForward, (360.f/12.f) * float( i ), vecDir );
		Ray_t ray;
		ray.Init( vecStart, vecStart + ( vecDir * MAX_TRACE_LENGTH ) );

		trace_t tr;
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &filter, &tr );
		dists[i] = tr.fraction * MAX_TRACE_LENGTH;
		surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
		if ( psurf )
			totalReflectivity = psurf->audio.reflectivity;

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
		( dists[(longestIdx + 13) % 12] + dists[( longestIdx + 5 ) % 12] ) +
		( dists[( longestIdx + 1 ) % 12] + dists[( longestIdx + 7 ) % 12] )
		) / 3.f;

	size.y = (
		( dists[longestIdx + 3] + dists[(longestIdx + 3 + 6) % 12] ) +
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
		ray.Init( startPos, startPos + (vecDir * MAX_TRACE_LENGTH ) );

		trace_t tr;
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &filter, &tr );
		if ( tr.DidHit() && ( tr.surface.flags & SURF_SKY ) )
		{
			skyTotal += 1.f;
			//debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 255, 0, 0, false, 10 );
			//debugoverlay->AddBoxOverlay2( tr.endpos, boxMins, boxMaxs, { 0, 0, 0 }, Color(0, 0, 0, 0), Color( 255, 0, 0, 255 ), 10);
		}
		else
		{
			//debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 0, 255, 255, false, 10 );
			//debugoverlay->AddBoxOverlay2( tr.endpos, boxMins, boxMaxs, { 0, 0, 0 }, Color( 0, 0, 0, 0 ), Color( 0, 255, 255, 255 ), 10 );
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
}

void CAutoDSP::Update( Vector listenerPos )
{
	const float skyVisibility = GetSkyVisibility( listenerPos );
	Vector size = { 0, 0, 0 };
	float reflectivity;
	GetSpaceSize( listenerPos, size, reflectivity );
}