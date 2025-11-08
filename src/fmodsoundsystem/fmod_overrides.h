#pragma once
#include <fmod/fmod.hpp>

void *F_CALL USER_FMOD_ALLOC( unsigned int size, FMOD_MEMORY_TYPE, const char * );
void *F_CALL USER_FMOD_REALLOC( void *ptr, unsigned int size, FMOD_MEMORY_TYPE, const char * );
void F_CALL USER_FMOD_FREE( void *ptr, FMOD_MEMORY_TYPE, const char * );
FMOD_RESULT F_CALL USER_FMOD_FILE_OPEN_CALLBACK( const char *name, unsigned int *filesize, void **handle, void *userdata );
FMOD_RESULT F_CALL USER_FMOD_FILE_CLOSE_CALLBACK( void *handle, void *userdata );
FMOD_RESULT F_CALL USER_FMOD_FILE_READ_CALLBACK( void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void *userdata );
FMOD_RESULT F_CALL USER_FMOD_FILE_SEEK_CALLBACK( void *handle, unsigned int pos, void *userdata );