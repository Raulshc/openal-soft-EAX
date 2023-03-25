#ifndef EAXMAIN_H
#define EAXMAIN_H

#include "alstring.h"
#include "alMain.h"

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct {
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID;
#endif /* GUID_DEFINED */

typedef void (AL_APIENTRY *LPEAXSET)(const GUID*, ALuint, ALuint, ALvoid*, ALuint);
typedef void (AL_APIENTRY *LPEAXGET)(const GUID*, ALuint, ALuint, ALvoid*, ALuint);

typedef void (AL_APIENTRY *LPEAXSETBUFFERMODE)(ALsizei, ALuint*, ALint);
typedef void (AL_APIENTRY *LPEAXGETBUFFERMODE)(ALuint, ALint*);

#ifdef AL_ALEXT_PROTOTYPES
AL_API ALenum AL_APIENTRY EAXSet(const GUID *propertySetID, ALuint property, ALuint source, ALvoid *value, ALuint size);
AL_API ALenum AL_APIENTRY EAXGet(const GUID *propertySetID, ALuint property, ALuint source, ALvoid *value, ALuint size);

AL_API ALboolean AL_APIENTRY EAXSetBufferMode(ALsizei n, ALuint *buffers, ALint value);
AL_API ALenum AL_APIENTRY EAXGetBufferMode(ALuint buffer, ALint *pReserved);
#endif

#endif /* EAXMAIN_H */
