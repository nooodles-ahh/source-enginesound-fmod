#pragma once
#include <mathlib/vector.h>

class CAutoDSP
{
public:
	void Init( CreateInterfaceFn appSystemFactory, CreateInterfaceFn physicsFactory );
	void Update( Vector playerPos );

private:
	int GetRoomType( Vector &size, float &reflectivity, float &skyVisibility );
	void GetSpaceSize( Vector &startPos, Vector &size, float &reflectivity );
	float GetSkyVisibility( Vector &startPos );
};