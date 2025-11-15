#pragma once
#include <mathlib/vector.h>
#include "fmod_impl.h"

class CAutoDSP
{
public:
	void Init( CreateInterfaceFn appSystemFactory, CreateInterfaceFn physicsFactory );
	void CategoriseSpace( Vector playerPos, float &reflectivity, float &spaceSize, DynamicReverbSpace &roomtype );

private:
	DynamicReverbSpace GetRoomType( Vector &size, float &skyVisibility );
	void GetSpaceSize( Vector &startPos, Vector &size, float &reflectivity );
	float GetSkyVisibility( Vector &startPos );
};