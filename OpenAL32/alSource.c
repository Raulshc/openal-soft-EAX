/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <float.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "alMain.h"
#include "alError.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"
#include "ringbuffer.h"

#include "backends/base.h"

#include "threads.h"
#include "almalloc.h"


static ALsource *AllocSource(ALCcontext *context);
static void FreeSource(ALCcontext *context, ALsource *source);
static void InitSourceParams(ALsource *Source, ALsizei num_sends, LPGUID list);
static void DeinitSource(ALsource *source, ALsizei num_sends);
static void UpdateSourceProps(ALsource *source, ALvoice *voice, ALsizei num_sends, ALCcontext *context);
static ALint64 GetSourceSampleOffset(ALsource *Source, ALCcontext *context, ALuint64 *clocktime);
static ALdouble GetSourceSecOffset(ALsource *Source, ALCcontext *context, ALuint64 *clocktime);
static ALdouble GetSourceOffset(ALsource *Source, ALenum name, ALCcontext *context);
static ALboolean GetSampleOffset(ALsource *Source, ALuint *offset, ALsizei *frac);
static ALboolean ApplyOffset(ALsource *Source, ALvoice *voice);
static ALboolean GenSourceSendPrimaryEAX(ALCcontext *Context, ALsource *Source);

static inline void LockSourceList(ALCcontext *context)
{ almtx_lock(&context->SourceLock); }
static inline void UnlockSourceList(ALCcontext *context)
{ almtx_unlock(&context->SourceLock); }

static inline ALsource *LookupSource(ALCcontext *context, ALuint id)
{
    SourceSubList *sublist;
    ALuint lidx = (id-1) >> 6;
    ALsizei slidx = (id-1) & 0x3f;

    if(UNLIKELY(lidx >= VECTOR_SIZE(context->SourceList)))
        return NULL;
    sublist = &VECTOR_ELEM(context->SourceList, lidx);
    if(UNLIKELY(sublist->FreeMask & (U64(1)<<slidx)))
        return NULL;
    return sublist->Sources + slidx;
}

static inline ALbuffer *LookupBuffer(ALCdevice *device, ALuint id)
{
    BufferSubList *sublist;
    ALuint lidx = (id-1) >> 6;
    ALsizei slidx = (id-1) & 0x3f;

    if(UNLIKELY(lidx >= VECTOR_SIZE(device->BufferList)))
        return NULL;
    sublist = &VECTOR_ELEM(device->BufferList, lidx);
    if(UNLIKELY(sublist->FreeMask & (U64(1)<<slidx)))
        return NULL;
    return sublist->Buffers + slidx;
}

static inline ALfilter *LookupFilter(ALCdevice *device, ALuint id)
{
    FilterSubList *sublist;
    ALuint lidx = (id-1) >> 6;
    ALsizei slidx = (id-1) & 0x3f;

    if(UNLIKELY(lidx >= VECTOR_SIZE(device->FilterList)))
        return NULL;
    sublist = &VECTOR_ELEM(device->FilterList, lidx);
    if(UNLIKELY(sublist->FreeMask & (U64(1)<<slidx)))
        return NULL;
    return sublist->Filters + slidx;
}

static inline ALeffectslot *LookupEffectSlot(ALCcontext *context, ALuint id)
{
    id--;
    if(UNLIKELY(id >= VECTOR_SIZE(context->EffectSlotList)))
        return NULL;
    return VECTOR_ELEM(context->EffectSlotList, id);
}


typedef enum SourceProp {
    srcPitch = AL_PITCH,
    srcGain = AL_GAIN,
    srcMinGain = AL_MIN_GAIN,
    srcMaxGain = AL_MAX_GAIN,
    srcMaxDistance = AL_MAX_DISTANCE,
    srcRolloffFactor = AL_ROLLOFF_FACTOR,
    srcDopplerFactor = AL_DOPPLER_FACTOR,
    srcConeOuterGain = AL_CONE_OUTER_GAIN,
    srcSecOffset = AL_SEC_OFFSET,
    srcSampleOffset = AL_SAMPLE_OFFSET,
    srcByteOffset = AL_BYTE_OFFSET,
    srcConeInnerAngle = AL_CONE_INNER_ANGLE,
    srcConeOuterAngle = AL_CONE_OUTER_ANGLE,
    srcRefDistance = AL_REFERENCE_DISTANCE,

    srcPosition = AL_POSITION,
    srcVelocity = AL_VELOCITY,
    srcDirection = AL_DIRECTION,

    srcSourceRelative = AL_SOURCE_RELATIVE,
    srcLooping = AL_LOOPING,
    srcBuffer = AL_BUFFER,
    srcSourceState = AL_SOURCE_STATE,
    srcBuffersQueued = AL_BUFFERS_QUEUED,
    srcBuffersProcessed = AL_BUFFERS_PROCESSED,
    srcSourceType = AL_SOURCE_TYPE,

    /* ALC_EXT_EFX */
    srcConeOuterGainHF = AL_CONE_OUTER_GAINHF,
    srcAirAbsorptionFactor = AL_AIR_ABSORPTION_FACTOR,
    srcRoomRolloffFactor =  AL_ROOM_ROLLOFF_FACTOR,
    srcDirectFilterGainHFAuto = AL_DIRECT_FILTER_GAINHF_AUTO,
    srcAuxSendFilterGainAuto = AL_AUXILIARY_SEND_FILTER_GAIN_AUTO,
    srcAuxSendFilterGainHFAuto = AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO,
    srcDirectFilter = AL_DIRECT_FILTER,
    srcAuxSendFilter = AL_AUXILIARY_SEND_FILTER,

    /* AL_SOFT_direct_channels */
    srcDirectChannelsSOFT = AL_DIRECT_CHANNELS_SOFT,

    /* AL_EXT_source_distance_model */
    srcDistanceModel = AL_DISTANCE_MODEL,

    /* AL_SOFT_source_latency */
    srcSampleOffsetLatencySOFT = AL_SAMPLE_OFFSET_LATENCY_SOFT,
    srcSecOffsetLatencySOFT = AL_SEC_OFFSET_LATENCY_SOFT,

    /* AL_EXT_STEREO_ANGLES */
    srcAngles = AL_STEREO_ANGLES,

    /* AL_EXT_SOURCE_RADIUS */
    srcRadius = AL_SOURCE_RADIUS,

    /* AL_EXT_BFORMAT */
    srcOrientation = AL_ORIENTATION,

    /* AL_SOFT_source_resampler */
    srcResampler = AL_SOURCE_RESAMPLER_SOFT,

    /* AL_SOFT_source_spatialize */
    srcSpatialize = AL_SOURCE_SPATIALIZE_SOFT,

    /* ALC_SOFT_device_clock */
    srcSampleOffsetClockSOFT = AL_SAMPLE_OFFSET_CLOCK_SOFT,
    srcSecOffsetClockSOFT = AL_SEC_OFFSET_CLOCK_SOFT,
} SourceProp;

static ALboolean SetSourcefv(ALsource *Source, ALCcontext *Context, SourceProp prop, const ALfloat *values);
static ALboolean SetSourceiv(ALsource *Source, ALCcontext *Context, SourceProp prop, const ALint *values);
static ALboolean SetSourcei64v(ALsource *Source, ALCcontext *Context, SourceProp prop, const ALint64SOFT *values);

static ALboolean GetSourcedv(ALsource *Source, ALCcontext *Context, SourceProp prop, ALdouble *values);
static ALboolean GetSourceiv(ALsource *Source, ALCcontext *Context, SourceProp prop, ALint *values);
static ALboolean GetSourcei64v(ALsource *Source, ALCcontext *Context, SourceProp prop, ALint64 *values);

static inline ALvoice *GetSourceVoice(ALsource *source, ALCcontext *context)
{
    ALint idx = source->VoiceIdx;
    if(idx >= 0 && idx < context->VoiceCount)
    {
        ALvoice *voice = context->Voices[idx];
        if(ATOMIC_LOAD(&voice->Source, almemory_order_acquire) == source)
            return voice;
    }
    source->VoiceIdx = -1;
    return NULL;
}

/**
 * Returns if the last known state for the source was playing or paused. Does
 * not sync with the mixer voice.
 */
static inline bool IsPlayingOrPaused(ALsource *source)
{ return source->state == AL_PLAYING || source->state == AL_PAUSED; }

/**
 * Returns an updated source state using the matching voice's status (or lack
 * thereof).
 */
static inline ALenum GetSourceState(ALsource *source, ALvoice *voice)
{
    if(!voice && source->state == AL_PLAYING)
        source->state = AL_STOPPED;
    return source->state;
}

/**
 * Returns if the source should specify an update, given the context's
 * deferring state and the source's last known state.
 */
static inline bool SourceShouldUpdate(ALsource *source, ALCcontext *context)
{
    return !ATOMIC_LOAD(&context->DeferUpdates, almemory_order_acquire) &&
           IsPlayingOrPaused(source);
}


/** Can only be called while the mixer is locked! */
static void SendStateChangeEvent(ALCcontext *context, ALuint id, ALenum state)
{
    AsyncEvent evt = ASYNC_EVENT(EventType_SourceStateChange);
    ALbitfieldSOFT enabledevt;

    enabledevt = ATOMIC_LOAD(&context->EnabledEvts, almemory_order_acquire);
    if(!(enabledevt&EventType_SourceStateChange)) return;

    evt.u.user.type = AL_EVENT_TYPE_SOURCE_STATE_CHANGED_SOFT;
    evt.u.user.id = id;
    evt.u.user.param = state;
    snprintf(evt.u.user.msg, sizeof(evt.u.user.msg), "Source ID %u state changed to %s", id,
        (state==AL_INITIAL) ? "AL_INITIAL" :
        (state==AL_PLAYING) ? "AL_PLAYING" :
        (state==AL_PAUSED) ? "AL_PAUSED" :
        (state==AL_STOPPED) ? "AL_STOPPED" : "<unknown>"
    );
    /* The mixer may have queued a state change that's not yet been processed,
     * and we don't want state change messages to occur out of order, so send
     * it through the async queue to ensure proper ordering.
     */
    if(ll_ringbuffer_write(context->AsyncEvents, (const char*)&evt, 1) == 1)
        alsem_post(&context->EventSem);
}


static ALint FloatValsByProp(ALenum prop)
{
    if(prop != (ALenum)((SourceProp)prop))
        return 0;
    switch((SourceProp)prop)
    {
        case AL_PITCH:
        case AL_GAIN:
        case AL_MIN_GAIN:
        case AL_MAX_GAIN:
        case AL_MAX_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_DOPPLER_FACTOR:
        case AL_CONE_OUTER_GAIN:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_REFERENCE_DISTANCE:
        case AL_CONE_OUTER_GAINHF:
        case AL_AIR_ABSORPTION_FACTOR:
        case AL_ROOM_ROLLOFF_FACTOR:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_SOURCE_STATE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_TYPE:
        case AL_SOURCE_RADIUS:
        case AL_SOURCE_RESAMPLER_SOFT:
        case AL_SOURCE_SPATIALIZE_SOFT:
            return 1;

        case AL_STEREO_ANGLES:
            return 2;

        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            return 3;

        case AL_ORIENTATION:
            return 6;

        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
            break; /* Double only */

        case AL_BUFFER:
        case AL_DIRECT_FILTER:
        case AL_AUXILIARY_SEND_FILTER:
            break; /* i/i64 only */
        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            break; /* i64 only */
    }
    return 0;
}
static ALint DoubleValsByProp(ALenum prop)
{
    if(prop != (ALenum)((SourceProp)prop))
        return 0;
    switch((SourceProp)prop)
    {
        case AL_PITCH:
        case AL_GAIN:
        case AL_MIN_GAIN:
        case AL_MAX_GAIN:
        case AL_MAX_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_DOPPLER_FACTOR:
        case AL_CONE_OUTER_GAIN:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_REFERENCE_DISTANCE:
        case AL_CONE_OUTER_GAINHF:
        case AL_AIR_ABSORPTION_FACTOR:
        case AL_ROOM_ROLLOFF_FACTOR:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_SOURCE_STATE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_TYPE:
        case AL_SOURCE_RADIUS:
        case AL_SOURCE_RESAMPLER_SOFT:
        case AL_SOURCE_SPATIALIZE_SOFT:
            return 1;

        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
        case AL_STEREO_ANGLES:
            return 2;

        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            return 3;

        case AL_ORIENTATION:
            return 6;

        case AL_BUFFER:
        case AL_DIRECT_FILTER:
        case AL_AUXILIARY_SEND_FILTER:
            break; /* i/i64 only */
        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            break; /* i64 only */
    }
    return 0;
}

static ALint IntValsByProp(ALenum prop)
{
    if(prop != (ALenum)((SourceProp)prop))
        return 0;
    switch((SourceProp)prop)
    {
        case AL_PITCH:
        case AL_GAIN:
        case AL_MIN_GAIN:
        case AL_MAX_GAIN:
        case AL_MAX_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_DOPPLER_FACTOR:
        case AL_CONE_OUTER_GAIN:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_REFERENCE_DISTANCE:
        case AL_CONE_OUTER_GAINHF:
        case AL_AIR_ABSORPTION_FACTOR:
        case AL_ROOM_ROLLOFF_FACTOR:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_BUFFER:
        case AL_SOURCE_STATE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_TYPE:
        case AL_DIRECT_FILTER:
        case AL_SOURCE_RADIUS:
        case AL_SOURCE_RESAMPLER_SOFT:
        case AL_SOURCE_SPATIALIZE_SOFT:
            return 1;

        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
        case AL_AUXILIARY_SEND_FILTER:
            return 3;

        case AL_ORIENTATION:
            return 6;

        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            break; /* i64 only */
        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
            break; /* Double only */
        case AL_STEREO_ANGLES:
            break; /* Float/double only */
    }
    return 0;
}
static ALint Int64ValsByProp(ALenum prop)
{
    if(prop != (ALenum)((SourceProp)prop))
        return 0;
    switch((SourceProp)prop)
    {
        case AL_PITCH:
        case AL_GAIN:
        case AL_MIN_GAIN:
        case AL_MAX_GAIN:
        case AL_MAX_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_DOPPLER_FACTOR:
        case AL_CONE_OUTER_GAIN:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_REFERENCE_DISTANCE:
        case AL_CONE_OUTER_GAINHF:
        case AL_AIR_ABSORPTION_FACTOR:
        case AL_ROOM_ROLLOFF_FACTOR:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_BUFFER:
        case AL_SOURCE_STATE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_TYPE:
        case AL_DIRECT_FILTER:
        case AL_SOURCE_RADIUS:
        case AL_SOURCE_RESAMPLER_SOFT:
        case AL_SOURCE_SPATIALIZE_SOFT:
            return 1;

        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            return 2;

        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
        case AL_AUXILIARY_SEND_FILTER:
            return 3;

        case AL_ORIENTATION:
            return 6;

        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
            break; /* Double only */
        case AL_STEREO_ANGLES:
            break; /* Float/double only */
    }
    return 0;
}


#define CHECKVAL(x) do {                                                      \
    if(!(x))                                                                  \
    {                                                                         \
        alSetError(Context, AL_INVALID_VALUE, "Value out of range");          \
        return AL_FALSE;                                                      \
    }                                                                         \
} while(0)

#define DO_UPDATEPROPS() do {                                                 \
    ALvoice *voice;                                                           \
    if(SourceShouldUpdate(Source, Context) &&                                 \
       (voice=GetSourceVoice(Source, Context)) != NULL)                       \
        UpdateSourceProps(Source, voice, device->NumAuxSends, Context);       \
    else                                                                      \
        ATOMIC_FLAG_CLEAR(&Source->PropsClean, almemory_order_release);       \
} while(0)

#define DO_UPDATE_EAXPROPS() do {                                             \
    Context->EAXIsDefer = AL_FALSE;                                           \
    CalcFilterGainsEAX(Context, Source);                                      \
    ApplyFilterGainsEAX(Context);                                             \
    UpdateSourceEAX(Context, Source);                                         \
} while(0)

static ALboolean SetSourcefv(ALsource *Source, ALCcontext *Context, SourceProp prop, const ALfloat *values)
{
    ALCdevice *device = Context->Device;
    ALint ival;

    switch(prop)
    {
        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
            /* Query only */
            SETERR_RETURN(Context, AL_INVALID_OPERATION, AL_FALSE,
                          "Setting read-only source property 0x%04x", prop);

        case AL_PITCH:
            CHECKVAL(*values >= 0.0f);

            Source->Pitch = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_CONE_INNER_ANGLE:
            CHECKVAL(*values >= 0.0f && *values <= 360.0f);

            Source->InnerAngle = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_CONE_OUTER_ANGLE:
            CHECKVAL(*values >= 0.0f && *values <= 360.0f);

            Source->OuterAngle = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_GAIN:
            CHECKVAL(*values >= 0.0f);

            Source->Gain = *values;
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_MAX_DISTANCE:
            CHECKVAL(*values >= 0.0f);

            Source->MaxDistance = *values;
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_ROLLOFF_FACTOR:
            CHECKVAL(*values >= 0.0f);

            Source->RolloffFactor = *values;
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_REFERENCE_DISTANCE:
            CHECKVAL(*values >= 0.0f);

            Source->RefDistance = *values;
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_MIN_GAIN:
            CHECKVAL(*values >= 0.0f);

            Source->MinGain = *values;
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_MAX_GAIN:
            CHECKVAL(*values >= 0.0f);

            Source->MaxGain = *values;
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_CONE_OUTER_GAIN:
            CHECKVAL(*values >= 0.0f && *values <= 1.0f);

            Source->OuterGain = *values;
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_CONE_OUTER_GAINHF:
            CHECKVAL(*values >= 0.0f && *values <= 1.0f);

            Source->OuterGainHF = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_AIR_ABSORPTION_FACTOR:
            CHECKVAL(*values >= 0.0f && *values <= 10.0f);

            Source->AirAbsorptionFactor = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_ROOM_ROLLOFF_FACTOR:
            CHECKVAL(*values >= 0.0f && *values <= 10.0f);

            Source->RoomRolloffFactor = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_DOPPLER_FACTOR:
            CHECKVAL(*values >= 0.0f && *values <= 1.0f);

            Source->DopplerFactor = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
            CHECKVAL(*values >= 0.0f);

            Source->OffsetType = prop;
            Source->Offset = *values;

            if(IsPlayingOrPaused(Source))
            {
                ALvoice *voice;

                ALCdevice_Lock(Context->Device);
                /* Double-check that the source is still playing while we have
                 * the lock.
                 */
                voice = GetSourceVoice(Source, Context);
                if(voice)
                {
                    if(ApplyOffset(Source, voice) == AL_FALSE)
                    {
                        ALCdevice_Unlock(Context->Device);
                        SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid offset");
                    }
                }
                ALCdevice_Unlock(Context->Device);
            }
            return AL_TRUE;

        case AL_SOURCE_RADIUS:
            CHECKVAL(*values >= 0.0f && isfinite(*values));

            Source->Radius = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_STEREO_ANGLES:
            CHECKVAL(isfinite(values[0]) && isfinite(values[1]));

            Source->StereoPan[0] = values[0];
            Source->StereoPan[1] = values[1];
            DO_UPDATEPROPS();
            return AL_TRUE;


        case AL_POSITION:
            CHECKVAL(isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]));

            Source->Position[0] = values[0];
            Source->Position[1] = values[1];
            Source->Position[2] = values[2];
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_VELOCITY:
            CHECKVAL(isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]));

            Source->Velocity[0] = values[0];
            Source->Velocity[1] = values[1];
            Source->Velocity[2] = values[2];
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_DIRECTION:
            CHECKVAL(isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]));

            Source->Direction[0] = values[0];
            Source->Direction[1] = values[1];
            Source->Direction[2] = values[2];
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_ORIENTATION:
            CHECKVAL(isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]) &&
                     isfinite(values[3]) && isfinite(values[4]) && isfinite(values[5]));

            Source->Orientation[0][0] = values[0];
            Source->Orientation[0][1] = values[1];
            Source->Orientation[0][2] = values[2];
            Source->Orientation[1][0] = values[3];
            Source->Orientation[1][1] = values[4];
            Source->Orientation[1][2] = values[5];
            DO_UPDATEPROPS();
            return AL_TRUE;


        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_SOURCE_STATE:
        case AL_SOURCE_TYPE:
        case AL_DISTANCE_MODEL:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_SOURCE_RESAMPLER_SOFT:
        case AL_SOURCE_SPATIALIZE_SOFT:
            ival = (ALint)values[0];
            return SetSourceiv(Source, Context, prop, &ival);

        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
            ival = (ALint)((ALuint)values[0]);
            return SetSourceiv(Source, Context, prop, &ival);

        case AL_BUFFER:
        case AL_DIRECT_FILTER:
        case AL_AUXILIARY_SEND_FILTER:
        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            break;
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    SETERR_RETURN(Context, AL_INVALID_ENUM, AL_FALSE, "Invalid source float property 0x%04x", prop);
}

static ALboolean SetSourceiv(ALsource *Source, ALCcontext *Context, SourceProp prop, const ALint *values)
{
    ALCdevice *device = Context->Device;
    ALbuffer  *buffer = NULL;
    ALfilter  *filter = NULL;
    ALeffectslot *slot = NULL;
    ALbufferlistitem *oldlist;
    ALfloat fvals[6];
    ALuint oldchannels;

    switch(prop)
    {
        case AL_SOURCE_STATE:
        case AL_SOURCE_TYPE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
            /* Query only */
            SETERR_RETURN(Context, AL_INVALID_OPERATION, AL_FALSE,
                          "Setting read-only source property 0x%04x", prop);

        case AL_SOURCE_RELATIVE:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->HeadRelative = (ALboolean)*values;
            if (device->EAXIsActive && Context->EAXIsDefer)
                DO_UPDATE_EAXPROPS();
            else
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_LOOPING:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->Looping = (ALboolean)*values;
            if(IsPlayingOrPaused(Source))
            {
                ALvoice *voice = GetSourceVoice(Source, Context);
                if(voice)
                {
                    if(Source->Looping)
                        ATOMIC_STORE(&voice->loop_buffer, Source->queue, almemory_order_release);
                    else
                        ATOMIC_STORE(&voice->loop_buffer, NULL, almemory_order_release);

                    /* If the source is playing, wait for the current mix to finish
                     * to ensure it isn't currently looping back or reaching the
                     * end.
                     */
                    while((ATOMIC_LOAD(&device->MixCount, almemory_order_acquire)&1))
                        althrd_yield();
                }
            }
            return AL_TRUE;

        case AL_BUFFER:
            LockBufferList(device);
            if(!(*values == 0 || (buffer=LookupBuffer(device, *values)) != NULL))
            {
                UnlockBufferList(device);
                SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid buffer ID %u",
                              *values);
            }

            if(buffer && buffer->MappedAccess != 0 &&
               !(buffer->MappedAccess&AL_MAP_PERSISTENT_BIT_SOFT))
            {
                UnlockBufferList(device);
                SETERR_RETURN(Context, AL_INVALID_OPERATION, AL_FALSE,
                              "Setting non-persistently mapped buffer %u", buffer->id);
            }
            else
            {
                ALenum state = GetSourceState(Source, GetSourceVoice(Source, Context));
                if(state == AL_PLAYING || state == AL_PAUSED)
                {
                    UnlockBufferList(device);
                    SETERR_RETURN(Context, AL_INVALID_OPERATION, AL_FALSE,
                                  "Setting buffer on playing or paused source %u", Source->id);
                }
            }

            oldlist = Source->queue;
            if(buffer != NULL)
            {
                /* Add the selected buffer to a one-item queue */
                ALbufferlistitem *newlist = al_calloc(DEF_ALIGN,
                    FAM_SIZE(ALbufferlistitem, buffers, 1));
                ATOMIC_INIT(&newlist->next, NULL);
                newlist->max_samples = buffer->SampleLen;
                newlist->num_buffers = 1;
                newlist->buffers[0] = buffer;
                IncrementRef(&buffer->ref);

                /* Source is now Static */
                Source->SourceType = AL_STATIC;
                Source->queue = newlist;

                oldchannels = Source->SourceChannels;
                Source->SourceChannels = ChannelsFromUserFmt(newlist->buffers[0]->FmtChannels);

                if(oldchannels != Source->SourceChannels)
                {
                    if(Source->SourceChannels == 1)
                        Source->EAXSrcProps.lRoom = EAXSOURCE_DEFAULTROOM;
                    else if (Source->SourceChannels >= 2)
                        Source->EAXSrcProps.lRoom = EAXSOURCE_MINROOM;

                    if(Context->Device->EAXIsActive)
                    {
                        CalcFilterGainsEAX(Context, Source);
                        ApplyFilterGainsEAX(Context);
                        UpdateSourceEAX(Context, Source);
                    }
                }
            }
            else
            {
                /* Source is now Undetermined */
                Source->SourceType = AL_UNDETERMINED;
                Source->queue = NULL;

                Source->SourceChannels = AL_NONE;
            }
            UnlockBufferList(device);

            /* Delete all elements in the previous queue */
            while(oldlist != NULL)
            {
                ALsizei i;
                ALbufferlistitem *temp = oldlist;
                oldlist = ATOMIC_LOAD(&temp->next, almemory_order_relaxed);

                for(i = 0;i < temp->num_buffers;i++)
                {
                    if(temp->buffers[i])
                        DecrementRef(&temp->buffers[i]->ref);
                }
                al_free(temp);
            }
            return AL_TRUE;

        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
            CHECKVAL(*values >= 0);

            Source->OffsetType = prop;
            Source->Offset = *values;

            if(IsPlayingOrPaused(Source))
            {
                ALvoice *voice;

                ALCdevice_Lock(Context->Device);
                voice = GetSourceVoice(Source, Context);
                if(voice)
                {
                    if(ApplyOffset(Source, voice) == AL_FALSE)
                    {
                        ALCdevice_Unlock(Context->Device);
                        SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE,
                                      "Invalid source offset");
                    }
                }
                ALCdevice_Unlock(Context->Device);
            }
            return AL_TRUE;

        case AL_DIRECT_FILTER:
            LockFilterList(device);
            if(!(*values == 0 || (filter=LookupFilter(device, *values)) != NULL))
            {
                UnlockFilterList(device);
                SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid filter ID %u",
                              *values);
            }

            if(!filter)
            {
                Source->Direct.Gain = 1.0f;
                Source->Direct.GainHF = 1.0f;
                Source->Direct.HFReference = LOWPASSFREQREF;
                Source->Direct.GainLF = 1.0f;
                Source->Direct.LFReference = HIGHPASSFREQREF;
            }
            else
            {
                Source->Direct.Gain = filter->Gain;
                Source->Direct.GainHF = filter->GainHF;
                Source->Direct.HFReference = filter->HFReference;
                Source->Direct.GainLF = filter->GainLF;
                Source->Direct.LFReference = filter->LFReference;
            }
            UnlockFilterList(device);
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_DIRECT_FILTER_GAINHF_AUTO:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->DryGainHFAuto = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->WetGainAuto = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->WetGainHFAuto = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_DIRECT_CHANNELS_SOFT:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->DirectChannels = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_DISTANCE_MODEL:
            CHECKVAL(*values == AL_NONE ||
                     *values == AL_INVERSE_DISTANCE ||
                     *values == AL_INVERSE_DISTANCE_CLAMPED ||
                     *values == AL_LINEAR_DISTANCE ||
                     *values == AL_LINEAR_DISTANCE_CLAMPED ||
                     *values == AL_EXPONENT_DISTANCE ||
                     *values == AL_EXPONENT_DISTANCE_CLAMPED);

            Source->DistanceModel = *values;
            if(Context->SourceDistanceModel)
                DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_SOURCE_RESAMPLER_SOFT:
            CHECKVAL(*values >= 0 && *values <= ResamplerMax);

            Source->Resampler = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;

        case AL_SOURCE_SPATIALIZE_SOFT:
            CHECKVAL(*values >= AL_FALSE && *values <= AL_AUTO_SOFT);

            Source->Spatialize = *values;
            DO_UPDATEPROPS();
            return AL_TRUE;


        case AL_AUXILIARY_SEND_FILTER:
            LockEffectSlotList(Context);
            if(!(values[0] == 0 || (slot=LookupEffectSlot(Context, values[0])) != NULL))
            {
                UnlockEffectSlotList(Context);
                SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid effect ID %u",
                              values[0]);
            }
            if(!((ALuint)values[1] < (ALuint)device->NumAuxSends))
            {
                UnlockEffectSlotList(Context);
                SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid send %u", values[1]);
            }
            LockFilterList(device);
            if(!(values[2] == 0 || (filter=LookupFilter(device, values[2])) != NULL))
            {
                UnlockFilterList(device);
                UnlockEffectSlotList(Context);
                SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid filter ID %u",
                              values[2]);
            }

            if(!filter)
            {
                /* Disable filter */
                Source->Send[values[1]].Gain = 1.0f;
                Source->Send[values[1]].GainHF = 1.0f;
                Source->Send[values[1]].HFReference = LOWPASSFREQREF;
                Source->Send[values[1]].GainLF = 1.0f;
                Source->Send[values[1]].LFReference = HIGHPASSFREQREF;
            }
            else
            {
                Source->Send[values[1]].Gain = filter->Gain;
                Source->Send[values[1]].GainHF = filter->GainHF;
                Source->Send[values[1]].HFReference = filter->HFReference;
                Source->Send[values[1]].GainLF = filter->GainLF;
                Source->Send[values[1]].LFReference = filter->LFReference;
            }
            UnlockFilterList(device);

            if(slot != Source->Send[values[1]].Slot && IsPlayingOrPaused(Source))
            {
                ALvoice *voice;
                /* Add refcount on the new slot, and release the previous slot */
                if(slot) IncrementRef(&slot->ref);
                if(Source->Send[values[1]].Slot)
                    DecrementRef(&Source->Send[values[1]].Slot->ref);
                Source->Send[values[1]].Slot = slot;

                /* We must force an update if the auxiliary slot changed on an
                 * active source, in case the slot is about to be deleted.
                 */
                if((voice=GetSourceVoice(Source, Context)) != NULL)
                    UpdateSourceProps(Source, voice, device->NumAuxSends, Context);
                else
                    ATOMIC_FLAG_CLEAR(&Source->PropsClean, almemory_order_release);
            }
            else
            {
                if(slot) IncrementRef(&slot->ref);
                if(Source->Send[values[1]].Slot)
                    DecrementRef(&Source->Send[values[1]].Slot->ref);
                Source->Send[values[1]].Slot = slot;
                DO_UPDATEPROPS();
            }
            UnlockEffectSlotList(Context);

            return AL_TRUE;


        /* 1x float */
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_PITCH:
        case AL_GAIN:
        case AL_MIN_GAIN:
        case AL_MAX_GAIN:
        case AL_REFERENCE_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_CONE_OUTER_GAIN:
        case AL_MAX_DISTANCE:
        case AL_DOPPLER_FACTOR:
        case AL_CONE_OUTER_GAINHF:
        case AL_AIR_ABSORPTION_FACTOR:
        case AL_ROOM_ROLLOFF_FACTOR:
        case AL_SOURCE_RADIUS:
            fvals[0] = (ALfloat)*values;
            return SetSourcefv(Source, Context, (int)prop, fvals);

        /* 3x float */
        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            fvals[0] = (ALfloat)values[0];
            fvals[1] = (ALfloat)values[1];
            fvals[2] = (ALfloat)values[2];
            return SetSourcefv(Source, Context, (int)prop, fvals);

        /* 6x float */
        case AL_ORIENTATION:
            fvals[0] = (ALfloat)values[0];
            fvals[1] = (ALfloat)values[1];
            fvals[2] = (ALfloat)values[2];
            fvals[3] = (ALfloat)values[3];
            fvals[4] = (ALfloat)values[4];
            fvals[5] = (ALfloat)values[5];
            return SetSourcefv(Source, Context, (int)prop, fvals);

        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
        case AL_STEREO_ANGLES:
            break;
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    SETERR_RETURN(Context, AL_INVALID_ENUM, AL_FALSE, "Invalid source integer property 0x%04x",
                  prop);
}

static ALboolean SetSourcei64v(ALsource *Source, ALCcontext *Context, SourceProp prop, const ALint64SOFT *values)
{
    ALfloat fvals[6];
    ALint   ivals[3];

    switch(prop)
    {
        case AL_SOURCE_TYPE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_STATE:
        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            /* Query only */
            SETERR_RETURN(Context, AL_INVALID_OPERATION, AL_FALSE,
                          "Setting read-only source property 0x%04x", prop);

        /* 1x int */
        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
        case AL_SOURCE_RESAMPLER_SOFT:
        case AL_SOURCE_SPATIALIZE_SOFT:
            CHECKVAL(*values <= INT_MAX && *values >= INT_MIN);

            ivals[0] = (ALint)*values;
            return SetSourceiv(Source, Context, (int)prop, ivals);

        /* 1x uint */
        case AL_BUFFER:
        case AL_DIRECT_FILTER:
            CHECKVAL(*values <= UINT_MAX && *values >= 0);

            ivals[0] = (ALuint)*values;
            return SetSourceiv(Source, Context, (int)prop, ivals);

        /* 3x uint */
        case AL_AUXILIARY_SEND_FILTER:
            CHECKVAL(values[0] <= UINT_MAX && values[0] >= 0 &&
                     values[1] <= UINT_MAX && values[1] >= 0 &&
                     values[2] <= UINT_MAX && values[2] >= 0);

            ivals[0] = (ALuint)values[0];
            ivals[1] = (ALuint)values[1];
            ivals[2] = (ALuint)values[2];
            return SetSourceiv(Source, Context, (int)prop, ivals);

        /* 1x float */
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_PITCH:
        case AL_GAIN:
        case AL_MIN_GAIN:
        case AL_MAX_GAIN:
        case AL_REFERENCE_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_CONE_OUTER_GAIN:
        case AL_MAX_DISTANCE:
        case AL_DOPPLER_FACTOR:
        case AL_CONE_OUTER_GAINHF:
        case AL_AIR_ABSORPTION_FACTOR:
        case AL_ROOM_ROLLOFF_FACTOR:
        case AL_SOURCE_RADIUS:
            fvals[0] = (ALfloat)*values;
            return SetSourcefv(Source, Context, (int)prop, fvals);

        /* 3x float */
        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            fvals[0] = (ALfloat)values[0];
            fvals[1] = (ALfloat)values[1];
            fvals[2] = (ALfloat)values[2];
            return SetSourcefv(Source, Context, (int)prop, fvals);

        /* 6x float */
        case AL_ORIENTATION:
            fvals[0] = (ALfloat)values[0];
            fvals[1] = (ALfloat)values[1];
            fvals[2] = (ALfloat)values[2];
            fvals[3] = (ALfloat)values[3];
            fvals[4] = (ALfloat)values[4];
            fvals[5] = (ALfloat)values[5];
            return SetSourcefv(Source, Context, (int)prop, fvals);

        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
        case AL_STEREO_ANGLES:
            break;
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    SETERR_RETURN(Context, AL_INVALID_ENUM, AL_FALSE, "Invalid source integer64 property 0x%04x",
                  prop);
}

#undef CHECKVAL


static ALboolean GetSourcedv(ALsource *Source, ALCcontext *Context, SourceProp prop, ALdouble *values)
{
    ALCdevice *device = Context->Device;
    ClockLatency clocktime;
    ALuint64 srcclock;
    ALint ivals[3];
    ALboolean err;

    switch(prop)
    {
        case AL_GAIN:
            *values = Source->Gain;
            return AL_TRUE;

        case AL_PITCH:
            *values = Source->Pitch;
            return AL_TRUE;

        case AL_MAX_DISTANCE:
            *values = Source->MaxDistance;
            return AL_TRUE;

        case AL_ROLLOFF_FACTOR:
            *values = Source->RolloffFactor;
            return AL_TRUE;

        case AL_REFERENCE_DISTANCE:
            *values = Source->RefDistance;
            return AL_TRUE;

        case AL_CONE_INNER_ANGLE:
            *values = Source->InnerAngle;
            return AL_TRUE;

        case AL_CONE_OUTER_ANGLE:
            *values = Source->OuterAngle;
            return AL_TRUE;

        case AL_MIN_GAIN:
            *values = Source->MinGain;
            return AL_TRUE;

        case AL_MAX_GAIN:
            *values = Source->MaxGain;
            return AL_TRUE;

        case AL_CONE_OUTER_GAIN:
            *values = Source->OuterGain;
            return AL_TRUE;

        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
            *values = GetSourceOffset(Source, prop, Context);
            return AL_TRUE;

        case AL_CONE_OUTER_GAINHF:
            *values = Source->OuterGainHF;
            return AL_TRUE;

        case AL_AIR_ABSORPTION_FACTOR:
            *values = Source->AirAbsorptionFactor;
            return AL_TRUE;

        case AL_ROOM_ROLLOFF_FACTOR:
            *values = Source->RoomRolloffFactor;
            return AL_TRUE;

        case AL_DOPPLER_FACTOR:
            *values = Source->DopplerFactor;
            return AL_TRUE;

        case AL_SOURCE_RADIUS:
            *values = Source->Radius;
            return AL_TRUE;

        case AL_STEREO_ANGLES:
            values[0] = Source->StereoPan[0];
            values[1] = Source->StereoPan[1];
            return AL_TRUE;

        case AL_SEC_OFFSET_LATENCY_SOFT:
            /* Get the source offset with the clock time first. Then get the
             * clock time with the device latency. Order is important.
             */
            values[0] = GetSourceSecOffset(Source, Context, &srcclock);
            almtx_lock(&device->BackendLock);
            clocktime = GetClockLatency(device);
            almtx_unlock(&device->BackendLock);
            if(srcclock == (ALuint64)clocktime.ClockTime)
                values[1] = (ALdouble)clocktime.Latency / 1000000000.0;
            else
            {
                /* If the clock time incremented, reduce the latency by that
                 * much since it's that much closer to the source offset it got
                 * earlier.
                 */
                ALuint64 diff = clocktime.ClockTime - srcclock;
                values[1] = (ALdouble)(clocktime.Latency - minu64(clocktime.Latency, diff)) /
                            1000000000.0;
            }
            return AL_TRUE;

        case AL_SEC_OFFSET_CLOCK_SOFT:
            values[0] = GetSourceSecOffset(Source, Context, &srcclock);
            values[1] = srcclock / 1000000000.0;
            return AL_TRUE;

        case AL_POSITION:
            values[0] = Source->Position[0];
            values[1] = Source->Position[1];
            values[2] = Source->Position[2];
            return AL_TRUE;

        case AL_VELOCITY:
            values[0] = Source->Velocity[0];
            values[1] = Source->Velocity[1];
            values[2] = Source->Velocity[2];
            return AL_TRUE;

        case AL_DIRECTION:
            values[0] = Source->Direction[0];
            values[1] = Source->Direction[1];
            values[2] = Source->Direction[2];
            return AL_TRUE;

        case AL_ORIENTATION:
            values[0] = Source->Orientation[0][0];
            values[1] = Source->Orientation[0][1];
            values[2] = Source->Orientation[0][2];
            values[3] = Source->Orientation[1][0];
            values[4] = Source->Orientation[1][1];
            values[5] = Source->Orientation[1][2];
            return AL_TRUE;

        /* 1x int */
        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_SOURCE_STATE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_TYPE:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
        case AL_SOURCE_RESAMPLER_SOFT:
        case AL_SOURCE_SPATIALIZE_SOFT:
            if((err=GetSourceiv(Source, Context, (int)prop, ivals)) != AL_FALSE)
                *values = (ALdouble)ivals[0];
            return err;

        case AL_BUFFER:
        case AL_DIRECT_FILTER:
        case AL_AUXILIARY_SEND_FILTER:
        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            break;
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    SETERR_RETURN(Context, AL_INVALID_ENUM, AL_FALSE, "Invalid source double property 0x%04x",
                  prop);
}

static ALboolean GetSourceiv(ALsource *Source, ALCcontext *Context, SourceProp prop, ALint *values)
{
    ALbufferlistitem *BufferList;
    ALdouble dvals[6];
    ALboolean err;

    switch(prop)
    {
        case AL_SOURCE_RELATIVE:
            *values = Source->HeadRelative;
            return AL_TRUE;

        case AL_LOOPING:
            *values = Source->Looping;
            return AL_TRUE;

        case AL_BUFFER:
            {
                const ALbufferlistitem *Current = NULL;
                ALvoice *voice;

                if ((voice = GetSourceVoice(Source, Context)) != NULL)
                    Current = ATOMIC_LOAD(&voice->current_buffer, almemory_order_relaxed);

                BufferList = (Source->SourceType == AL_STATIC) ? Source->queue : Current;
                *values = (BufferList && BufferList->num_buffers >= 1 && BufferList->buffers[0]) ?
                    BufferList->buffers[0]->id : 0;
            }
            return AL_TRUE;

        case AL_SOURCE_STATE:
            *values = GetSourceState(Source, GetSourceVoice(Source, Context));
            return AL_TRUE;

        case AL_BUFFERS_QUEUED:
            if(!(BufferList=Source->queue))
                *values = 0;
            else
            {
                ALsizei count = 0;
                do {
                    count += BufferList->num_buffers;
                    BufferList = ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed);
                } while(BufferList != NULL);
                *values = count;
            }
            return AL_TRUE;

        case AL_BUFFERS_PROCESSED:
            if(Source->Looping || Source->SourceType != AL_STREAMING)
            {
                /* Buffers on a looping source are in a perpetual state of
                 * PENDING, so don't report any as PROCESSED */
                *values = 0;
            }
            else
            {
                const ALbufferlistitem *BufferList = Source->queue;
                const ALbufferlistitem *Current = NULL;
                ALsizei played = 0;
                ALvoice *voice;

                if((voice=GetSourceVoice(Source, Context)) != NULL)
                    Current = ATOMIC_LOAD(&voice->current_buffer, almemory_order_relaxed);
                else if(Source->state == AL_INITIAL)
                    Current = BufferList;

                while(BufferList && BufferList != Current)
                {
                    played += BufferList->num_buffers;
                    BufferList = ATOMIC_LOAD(&CONST_CAST(ALbufferlistitem*,BufferList)->next,
                                             almemory_order_relaxed);
                }
                *values = played;
            }
            return AL_TRUE;

        case AL_SOURCE_TYPE:
            *values = Source->SourceType;
            return AL_TRUE;

        case AL_DIRECT_FILTER_GAINHF_AUTO:
            *values = Source->DryGainHFAuto;
            return AL_TRUE;

        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
            *values = Source->WetGainAuto;
            return AL_TRUE;

        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
            *values = Source->WetGainHFAuto;
            return AL_TRUE;

        case AL_DIRECT_CHANNELS_SOFT:
            *values = Source->DirectChannels;
            return AL_TRUE;

        case AL_DISTANCE_MODEL:
            *values = Source->DistanceModel;
            return AL_TRUE;

        case AL_SOURCE_RESAMPLER_SOFT:
            *values = Source->Resampler;
            return AL_TRUE;

        case AL_SOURCE_SPATIALIZE_SOFT:
            *values = Source->Spatialize;
            return AL_TRUE;

        /* 1x float/double */
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_PITCH:
        case AL_GAIN:
        case AL_MIN_GAIN:
        case AL_MAX_GAIN:
        case AL_REFERENCE_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_CONE_OUTER_GAIN:
        case AL_MAX_DISTANCE:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_DOPPLER_FACTOR:
        case AL_AIR_ABSORPTION_FACTOR:
        case AL_ROOM_ROLLOFF_FACTOR:
        case AL_CONE_OUTER_GAINHF:
        case AL_SOURCE_RADIUS:
            if((err=GetSourcedv(Source, Context, prop, dvals)) != AL_FALSE)
                *values = (ALint)dvals[0];
            return err;

        /* 3x float/double */
        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            if((err=GetSourcedv(Source, Context, prop, dvals)) != AL_FALSE)
            {
                values[0] = (ALint)dvals[0];
                values[1] = (ALint)dvals[1];
                values[2] = (ALint)dvals[2];
            }
            return err;

        /* 6x float/double */
        case AL_ORIENTATION:
            if((err=GetSourcedv(Source, Context, prop, dvals)) != AL_FALSE)
            {
                values[0] = (ALint)dvals[0];
                values[1] = (ALint)dvals[1];
                values[2] = (ALint)dvals[2];
                values[3] = (ALint)dvals[3];
                values[4] = (ALint)dvals[4];
                values[5] = (ALint)dvals[5];
            }
            return err;

        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            break; /* i64 only */
        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
            break; /* Double only */
        case AL_STEREO_ANGLES:
            break; /* Float/double only */

        case AL_DIRECT_FILTER:
        case AL_AUXILIARY_SEND_FILTER:
            break; /* ??? */
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    SETERR_RETURN(Context, AL_INVALID_ENUM, AL_FALSE, "Invalid source integer property 0x%04x",
                  prop);
}

static ALboolean GetSourcei64v(ALsource *Source, ALCcontext *Context, SourceProp prop, ALint64 *values)
{
    ALCdevice *device = Context->Device;
    ClockLatency clocktime;
    ALuint64 srcclock;
    ALdouble dvals[6];
    ALint ivals[3];
    ALboolean err;

    switch(prop)
    {
        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
            /* Get the source offset with the clock time first. Then get the
             * clock time with the device latency. Order is important.
             */
            values[0] = GetSourceSampleOffset(Source, Context, &srcclock);
            almtx_lock(&device->BackendLock);
            clocktime = GetClockLatency(device);
            almtx_unlock(&device->BackendLock);
            if(srcclock == (ALuint64)clocktime.ClockTime)
                values[1] = clocktime.Latency;
            else
            {
                /* If the clock time incremented, reduce the latency by that
                 * much since it's that much closer to the source offset it got
                 * earlier.
                 */
                ALuint64 diff = clocktime.ClockTime - srcclock;
                values[1] = clocktime.Latency - minu64(clocktime.Latency, diff);
            }
            return AL_TRUE;

        case AL_SAMPLE_OFFSET_CLOCK_SOFT:
            values[0] = GetSourceSampleOffset(Source, Context, &srcclock);
            values[1] = srcclock;
            return AL_TRUE;

        /* 1x float/double */
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_PITCH:
        case AL_GAIN:
        case AL_MIN_GAIN:
        case AL_MAX_GAIN:
        case AL_REFERENCE_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_CONE_OUTER_GAIN:
        case AL_MAX_DISTANCE:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_DOPPLER_FACTOR:
        case AL_AIR_ABSORPTION_FACTOR:
        case AL_ROOM_ROLLOFF_FACTOR:
        case AL_CONE_OUTER_GAINHF:
        case AL_SOURCE_RADIUS:
            if((err=GetSourcedv(Source, Context, prop, dvals)) != AL_FALSE)
                *values = (ALint64)dvals[0];
            return err;

        /* 3x float/double */
        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            if((err=GetSourcedv(Source, Context, prop, dvals)) != AL_FALSE)
            {
                values[0] = (ALint64)dvals[0];
                values[1] = (ALint64)dvals[1];
                values[2] = (ALint64)dvals[2];
            }
            return err;

        /* 6x float/double */
        case AL_ORIENTATION:
            if((err=GetSourcedv(Source, Context, prop, dvals)) != AL_FALSE)
            {
                values[0] = (ALint64)dvals[0];
                values[1] = (ALint64)dvals[1];
                values[2] = (ALint64)dvals[2];
                values[3] = (ALint64)dvals[3];
                values[4] = (ALint64)dvals[4];
                values[5] = (ALint64)dvals[5];
            }
            return err;

        /* 1x int */
        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_SOURCE_STATE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_TYPE:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
        case AL_SOURCE_RESAMPLER_SOFT:
        case AL_SOURCE_SPATIALIZE_SOFT:
            if((err=GetSourceiv(Source, Context, prop, ivals)) != AL_FALSE)
                *values = ivals[0];
            return err;

        /* 1x uint */
        case AL_BUFFER:
        case AL_DIRECT_FILTER:
            if((err=GetSourceiv(Source, Context, prop, ivals)) != AL_FALSE)
                *values = (ALuint)ivals[0];
            return err;

        /* 3x uint */
        case AL_AUXILIARY_SEND_FILTER:
            if((err=GetSourceiv(Source, Context, prop, ivals)) != AL_FALSE)
            {
                values[0] = (ALuint)ivals[0];
                values[1] = (ALuint)ivals[1];
                values[2] = (ALuint)ivals[2];
            }
            return err;

        case AL_SEC_OFFSET_LATENCY_SOFT:
        case AL_SEC_OFFSET_CLOCK_SOFT:
            break; /* Double only */
        case AL_STEREO_ANGLES:
            break; /* Float/double only */
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    SETERR_RETURN(Context, AL_INVALID_ENUM, AL_FALSE, "Invalid source integer64 property 0x%04x",
                  prop);
}


AL_API ALvoid AL_APIENTRY alGenSources(ALsizei n, ALuint *sources)
{
    ALCcontext *context;
    ALsizei cur = 0;

    context = GetContextRef();
    if(!context) return;

    if(!(n >= 0))
        alSetError(context, AL_INVALID_VALUE, "Generating %d sources", n);
    else for(cur = 0;cur < n;cur++)
    {
        ALsource *source = AllocSource(context);
        if(!source)
        {
            alDeleteSources(cur, sources);
            break;
        }
        sources[cur] = source->id;

        if(context->Device->EAXIsActive)
            GenSourceSendPrimaryEAX(context, source);
    }

    ALCcontext_DecRef(context);
}


AL_API ALvoid AL_APIENTRY alDeleteSources(ALsizei n, const ALuint *sources)
{
    ALCcontext *context;
    ALsource *Source;
    ALsizei i;

    context = GetContextRef();
    if(!context) return;

    LockSourceList(context);
    if(!(n >= 0))
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Deleting %d sources", n);

    /* Check that all Sources are valid */
    for(i = 0;i < n;i++)
    {
        if(LookupSource(context, sources[i]) == NULL)
            SETERR_GOTO(context, AL_INVALID_NAME, done, "Invalid source ID %u", sources[i]);
    }
    for(i = 0;i < n;i++)
    {
        if((Source=LookupSource(context, sources[i])) != NULL)
            FreeSource(context, Source);
    }

done:
    UnlockSourceList(context);
    ALCcontext_DecRef(context);
}


AL_API ALboolean AL_APIENTRY alIsSource(ALuint source)
{
    ALCcontext *context;
    ALboolean ret;

    context = GetContextRef();
    if(!context) return AL_FALSE;

    LockSourceList(context);
    ret = (LookupSource(context, source) ? AL_TRUE : AL_FALSE);
    UnlockSourceList(context);

    ALCcontext_DecRef(context);

    return ret;
}


AL_API ALvoid AL_APIENTRY alSourcef(ALuint source, ALenum param, ALfloat value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(FloatValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM, "Invalid float property 0x%04x", param);
    else
        SetSourcefv(Source, Context, param, &value);
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSource3f(ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(FloatValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM, "Invalid 3-float property 0x%04x", param);
    else
    {
        ALfloat fvals[3] = { value1, value2, value3 };
        SetSourcefv(Source, Context, param, fvals);
    }
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSourcefv(ALuint source, ALenum param, const ALfloat *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(FloatValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM, "Invalid float-vector property 0x%04x", param);
    else
        SetSourcefv(Source, Context, param, values);
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourcedSOFT(ALuint source, ALenum param, ALdouble value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(DoubleValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM, "Invalid double property 0x%04x", param);
    else
    {
        ALfloat fval = (ALfloat)value;
        SetSourcefv(Source, Context, param, &fval);
    }
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSource3dSOFT(ALuint source, ALenum param, ALdouble value1, ALdouble value2, ALdouble value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(DoubleValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM, "Invalid 3-double property 0x%04x", param);
    else
    {
        ALfloat fvals[3] = { (ALfloat)value1, (ALfloat)value2, (ALfloat)value3 };
        SetSourcefv(Source, Context, param, fvals);
    }
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSourcedvSOFT(ALuint source, ALenum param, const ALdouble *values)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALint      count;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!((count=DoubleValsByProp(param)) > 0 && count <= 6))
        alSetError(Context, AL_INVALID_ENUM, "Invalid double-vector property 0x%04x", param);
    else
    {
        ALfloat fvals[6];
        ALint i;

        for(i = 0;i < count;i++)
            fvals[i] = (ALfloat)values[i];
        SetSourcefv(Source, Context, param, fvals);
    }
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourcei(ALuint source, ALenum param, ALint value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(IntValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM, "Invalid integer property 0x%04x", param);
    else
        SetSourceiv(Source, Context, param, &value);
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alSource3i(ALuint source, ALenum param, ALint value1, ALint value2, ALint value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(IntValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM, "Invalid 3-integer property 0x%04x", param);
    else
    {
        ALint ivals[3] = { value1, value2, value3 };
        SetSourceiv(Source, Context, param, ivals);
    }
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alSourceiv(ALuint source, ALenum param, const ALint *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(IntValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM, "Invalid integer-vector property 0x%04x", param);
    else
        SetSourceiv(Source, Context, param, values);
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourcei64SOFT(ALuint source, ALenum param, ALint64SOFT value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(Int64ValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM, "Invalid integer64 property 0x%04x", param);
    else
        SetSourcei64v(Source, Context, param, &value);
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alSource3i64SOFT(ALuint source, ALenum param, ALint64SOFT value1, ALint64SOFT value2, ALint64SOFT value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(Int64ValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM, "Invalid 3-integer64 property 0x%04x", param);
    else
    {
        ALint64SOFT i64vals[3] = { value1, value2, value3 };
        SetSourcei64v(Source, Context, param, i64vals);
    }
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alSourcei64vSOFT(ALuint source, ALenum param, const ALint64SOFT *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    almtx_lock(&Context->PropLock);
    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(Int64ValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM, "Invalid integer64-vector property 0x%04x", param);
    else
        SetSourcei64v(Source, Context, param, values);
    UnlockSourceList(Context);
    almtx_unlock(&Context->PropLock);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetSourcef(ALuint source, ALenum param, ALfloat *value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!value)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(FloatValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM, "Invalid float property 0x%04x", param);
    else
    {
        ALdouble dval;
        if(GetSourcedv(Source, Context, param, &dval))
            *value = (ALfloat)dval;
    }
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetSource3f(ALuint source, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(value1 && value2 && value3))
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(FloatValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM, "Invalid 3-float property 0x%04x", param);
    else
    {
        ALdouble dvals[3];
        if(GetSourcedv(Source, Context, param, dvals))
        {
            *value1 = (ALfloat)dvals[0];
            *value2 = (ALfloat)dvals[1];
            *value3 = (ALfloat)dvals[2];
        }
    }
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetSourcefv(ALuint source, ALenum param, ALfloat *values)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALint      count;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!((count=FloatValsByProp(param)) > 0 && count <= 6))
        alSetError(Context, AL_INVALID_ENUM, "Invalid float-vector property 0x%04x", param);
    else
    {
        ALdouble dvals[6];
        if(GetSourcedv(Source, Context, param, dvals))
        {
            ALint i;
            for(i = 0;i < count;i++)
                values[i] = (ALfloat)dvals[i];
        }
    }
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetSourcedSOFT(ALuint source, ALenum param, ALdouble *value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!value)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(DoubleValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM, "Invalid double property 0x%04x", param);
    else
        GetSourcedv(Source, Context, param, value);
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alGetSource3dSOFT(ALuint source, ALenum param, ALdouble *value1, ALdouble *value2, ALdouble *value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(value1 && value2 && value3))
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(DoubleValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM, "Invalid 3-double property 0x%04x", param);
    else
    {
        ALdouble dvals[3];
        if(GetSourcedv(Source, Context, param, dvals))
        {
            *value1 = dvals[0];
            *value2 = dvals[1];
            *value3 = dvals[2];
        }
    }
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alGetSourcedvSOFT(ALuint source, ALenum param, ALdouble *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(DoubleValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM, "Invalid double-vector property 0x%04x", param);
    else
        GetSourcedv(Source, Context, param, values);
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetSourcei(ALuint source, ALenum param, ALint *value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!value)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(IntValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM, "Invalid integer property 0x%04x", param);
    else
        GetSourceiv(Source, Context, param, value);
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetSource3i(ALuint source, ALenum param, ALint *value1, ALint *value2, ALint *value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(value1 && value2 && value3))
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(IntValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM, "Invalid 3-integer property 0x%04x", param);
    else
    {
        ALint ivals[3];
        if(GetSourceiv(Source, Context, param, ivals))
        {
            *value1 = ivals[0];
            *value2 = ivals[1];
            *value3 = ivals[2];
        }
    }
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetSourceiv(ALuint source, ALenum param, ALint *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(IntValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM, "Invalid integer-vector property 0x%04x", param);
    else
        GetSourceiv(Source, Context, param, values);
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetSourcei64SOFT(ALuint source, ALenum param, ALint64SOFT *value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!value)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(Int64ValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM, "Invalid integer64 property 0x%04x", param);
    else
        GetSourcei64v(Source, Context, param, value);
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alGetSource3i64SOFT(ALuint source, ALenum param, ALint64SOFT *value1, ALint64SOFT *value2, ALint64SOFT *value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!(value1 && value2 && value3))
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(Int64ValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM, "Invalid 3-integer64 property 0x%04x", param);
    else
    {
        ALint64 i64vals[3];
        if(GetSourcei64v(Source, Context, param, i64vals))
        {
            *value1 = i64vals[0];
            *value2 = i64vals[1];
            *value3 = i64vals[2];
        }
    }
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alGetSourcei64vSOFT(ALuint source, ALenum param, ALint64SOFT *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    LockSourceList(Context);
    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME, "Invalid source ID %u", source);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE, "NULL pointer");
    else if(!(Int64ValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM, "Invalid integer64-vector property 0x%04x", param);
    else
        GetSourcei64v(Source, Context, param, values);
    UnlockSourceList(Context);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourcePlay(ALuint source)
{
    alSourcePlayv(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourcePlayv(ALsizei n, const ALuint *sources)
{
    ALCcontext *context;
    ALCdevice *device;
    ALsource *source;
    ALvoice *voice;
    ALsizei i, j;

    context = GetContextRef();
    if(!context) return;

    LockSourceList(context);
    if(!(n >= 0))
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Playing %d sources", n);
    for(i = 0;i < n;i++)
    {
        if(!LookupSource(context, sources[i]))
            SETERR_GOTO(context, AL_INVALID_NAME, done, "Invalid source ID %u", sources[i]);
    }

    device = context->Device;
    ALCdevice_Lock(device);
    /* If the device is disconnected, go right to stopped. */
    if(!ATOMIC_LOAD(&device->Connected, almemory_order_acquire))
    {
        /* TODO: Send state change event? */
        for(i = 0;i < n;i++)
        {
            source = LookupSource(context, sources[i]);
            source->OffsetType = AL_NONE;
            source->Offset = 0.0;
            source->state = AL_STOPPED;
        }
        ALCdevice_Unlock(device);
        goto done;
    }

    while(n > context->MaxVoices-context->VoiceCount)
    {
        ALsizei newcount = context->MaxVoices << 1;
        if(context->MaxVoices >= newcount)
        {
            ALCdevice_Unlock(device);
            SETERR_GOTO(context, AL_OUT_OF_MEMORY, done,
                        "Overflow increasing voice count %d -> %d", context->MaxVoices, newcount);
        }
        AllocateVoices(context, newcount, device->NumAuxSends);
    }

    for(i = 0;i < n;i++)
    {
        ALbufferlistitem *BufferList;
        bool start_fading = false;
        ALint vidx = -1;

        source = LookupSource(context, sources[i]);
        /* Check that there is a queue containing at least one valid, non zero
         * length buffer.
         */
        BufferList = source->queue;
        while(BufferList && BufferList->max_samples == 0)
            BufferList = ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed);

        /* If there's nothing to play, go right to stopped. */
        if(UNLIKELY(!BufferList))
        {
            /* NOTE: A source without any playable buffers should not have an
             * ALvoice since it shouldn't be in a playing or paused state. So
             * there's no need to look up its voice and clear the source.
             */
            ALenum oldstate = GetSourceState(source, NULL);
            source->OffsetType = AL_NONE;
            source->Offset = 0.0;
            if(oldstate != AL_STOPPED)
            {
                source->state = AL_STOPPED;
                SendStateChangeEvent(context, source->id, AL_STOPPED);
            }
            continue;
        }

        voice = GetSourceVoice(source, context);
        switch(GetSourceState(source, voice))
        {
            case AL_PLAYING:
                assert(voice != NULL);
                /* A source that's already playing is restarted from the beginning. */
                ATOMIC_STORE(&voice->current_buffer, BufferList, almemory_order_relaxed);
                ATOMIC_STORE(&voice->position, 0, almemory_order_relaxed);
                ATOMIC_STORE(&voice->position_fraction, 0, almemory_order_release);
                continue;

            case AL_PAUSED:
                assert(voice != NULL);
                /* A source that's paused simply resumes. */
                ATOMIC_STORE(&voice->Playing, true, almemory_order_release);
                source->state = AL_PLAYING;
                SendStateChangeEvent(context, source->id, AL_PLAYING);
                continue;

            default:
                break;
        }

        /* Look for an unused voice to play this source with. */
        assert(voice == NULL);
        for(j = 0;j < context->VoiceCount;j++)
        {
            if(ATOMIC_LOAD(&context->Voices[j]->Source, almemory_order_acquire) == NULL)
            {
                vidx = j;
                break;
            }
        }
        if(vidx == -1)
            vidx = context->VoiceCount++;
        voice = context->Voices[vidx];
        ATOMIC_STORE(&voice->Playing, false, almemory_order_release);

        ATOMIC_FLAG_TEST_AND_SET(&source->PropsClean, almemory_order_acquire);
        UpdateSourceProps(source, voice, device->NumAuxSends, context);

        /* A source that's not playing or paused has any offset applied when it
         * starts playing.
         */
        if(source->Looping)
            ATOMIC_STORE(&voice->loop_buffer, source->queue, almemory_order_relaxed);
        else
            ATOMIC_STORE(&voice->loop_buffer, NULL, almemory_order_relaxed);
        ATOMIC_STORE(&voice->current_buffer, BufferList, almemory_order_relaxed);
        ATOMIC_STORE(&voice->position, 0, almemory_order_relaxed);
        ATOMIC_STORE(&voice->position_fraction, 0, almemory_order_relaxed);
        if(ApplyOffset(source, voice) != AL_FALSE)
            start_fading = ATOMIC_LOAD(&voice->position, almemory_order_relaxed) != 0 ||
                ATOMIC_LOAD(&voice->position_fraction, almemory_order_relaxed) != 0 ||
                ATOMIC_LOAD(&voice->current_buffer, almemory_order_relaxed) != BufferList;

        for(j = 0;j < BufferList->num_buffers;j++)
        {
            ALbuffer *buffer = BufferList->buffers[j];
            if(buffer)
            {
                voice->NumChannels = ChannelsFromFmt(buffer->FmtChannels);
                voice->SampleSize  = BytesFromFmt(buffer->FmtType);
                break;
            }
        }

        /* Clear previous samples. */
        memset(voice->PrevSamples, 0, sizeof(voice->PrevSamples));

        /* Clear the stepping value so the mixer knows not to mix this until
         * the update gets applied.
         */
        voice->Step = 0;

        voice->Flags = start_fading ? VOICE_IS_FADING : 0;
        if(source->SourceType == AL_STATIC) voice->Flags |= VOICE_IS_STATIC;
        memset(voice->Direct.Params, 0, sizeof(voice->Direct.Params[0])*voice->NumChannels);
        for(j = 0;j < device->NumAuxSends;j++)
            memset(voice->Send[j].Params, 0, sizeof(voice->Send[j].Params[0])*voice->NumChannels);
        if(device->AvgSpeakerDist > 0.0f)
        {
            ALfloat w1 = SPEEDOFSOUNDMETRESPERSEC /
                         (device->AvgSpeakerDist * device->Frequency);
            for(j = 0;j < voice->NumChannels;j++)
                NfcFilterCreate(&voice->Direct.Params[j].NFCtrlFilter, 0.0f, w1);
        }

        ATOMIC_STORE(&voice->Source, source, almemory_order_relaxed);
        ATOMIC_STORE(&voice->Playing, true, almemory_order_release);
        source->state = AL_PLAYING;
        source->VoiceIdx = vidx;

        /* If EAX is enabled, compute the EAX Update source. This is computed twice, so it must be rewritten*/
        if (context->Device->EAXIsActive)
        {
            CalcFilterGainsEAX(context, source);
            ApplyFilterGainsEAX(context);
            UpdateSourceEAX(context, source);
        }

        SendStateChangeEvent(context, source->id, AL_PLAYING);
    }
    ALCdevice_Unlock(device);

done:
    UnlockSourceList(context);
    ALCcontext_DecRef(context);
}

AL_API ALvoid AL_APIENTRY alSourcePause(ALuint source)
{
    alSourcePausev(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourcePausev(ALsizei n, const ALuint *sources)
{
    ALCcontext *context;
    ALCdevice *device;
    ALsource *source;
    ALvoice *voice;
    ALsizei i;

    context = GetContextRef();
    if(!context) return;

    LockSourceList(context);
    if(!(n >= 0))
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Pausing %d sources", n);
    for(i = 0;i < n;i++)
    {
        if(!LookupSource(context, sources[i]))
            SETERR_GOTO(context, AL_INVALID_NAME, done, "Invalid source ID %u", sources[i]);
    }

    device = context->Device;
    ALCdevice_Lock(device);
    for(i = 0;i < n;i++)
    {
        source = LookupSource(context, sources[i]);
        if((voice=GetSourceVoice(source, context)) != NULL)
            ATOMIC_STORE(&voice->Playing, false, almemory_order_release);
        if(GetSourceState(source, voice) == AL_PLAYING)
        {
            source->state = AL_PAUSED;
            SendStateChangeEvent(context, source->id, AL_PAUSED);
        }
    }
    ALCdevice_Unlock(device);

done:
    UnlockSourceList(context);
    ALCcontext_DecRef(context);
}

AL_API ALvoid AL_APIENTRY alSourceStop(ALuint source)
{
    alSourceStopv(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourceStopv(ALsizei n, const ALuint *sources)
{
    ALCcontext *context;
    ALCdevice *device;
    ALsource *source;
    ALvoice *voice;
    ALsizei i;

    context = GetContextRef();
    if(!context) return;

    LockSourceList(context);
    if(!(n >= 0))
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Stopping %d sources", n);
    for(i = 0;i < n;i++)
    {
        if(!LookupSource(context, sources[i]))
            SETERR_GOTO(context, AL_INVALID_NAME, done, "Invalid source ID %u", sources[i]);
    }

    device = context->Device;
    ALCdevice_Lock(device);
    for(i = 0;i < n;i++)
    {
        ALenum oldstate;
        source = LookupSource(context, sources[i]);
        if((voice=GetSourceVoice(source, context)) != NULL)
        {
            ATOMIC_STORE(&voice->Source, NULL, almemory_order_relaxed);
            ATOMIC_STORE(&voice->Playing, false, almemory_order_release);
            voice = NULL;
        }
        oldstate = GetSourceState(source, voice);
        if(oldstate != AL_INITIAL && oldstate != AL_STOPPED)
        {
            source->state = AL_STOPPED;
            SendStateChangeEvent(context, source->id, AL_STOPPED);
        }
        source->OffsetType = AL_NONE;
        source->Offset = 0.0;
    }
    ALCdevice_Unlock(device);

done:
    UnlockSourceList(context);
    ALCcontext_DecRef(context);
}

AL_API ALvoid AL_APIENTRY alSourceRewind(ALuint source)
{
    alSourceRewindv(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourceRewindv(ALsizei n, const ALuint *sources)
{
    ALCcontext *context;
    ALCdevice *device;
    ALsource *source;
    ALvoice *voice;
    ALsizei i;

    context = GetContextRef();
    if(!context) return;

    LockSourceList(context);
    if(!(n >= 0))
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Rewinding %d sources", n);
    for(i = 0;i < n;i++)
    {
        if(!LookupSource(context, sources[i]))
            SETERR_GOTO(context, AL_INVALID_NAME, done, "Invalid source ID %u", sources[i]);
    }

    device = context->Device;
    ALCdevice_Lock(device);
    for(i = 0;i < n;i++)
    {
        source = LookupSource(context, sources[i]);
        if((voice=GetSourceVoice(source, context)) != NULL)
        {
            ATOMIC_STORE(&voice->Source, NULL, almemory_order_relaxed);
            ATOMIC_STORE(&voice->Playing, false, almemory_order_release);
            voice = NULL;
        }
        if(GetSourceState(source, voice) != AL_INITIAL)
        {
            source->state = AL_INITIAL;
            SendStateChangeEvent(context, source->id, AL_INITIAL);
        }
        source->OffsetType = AL_NONE;
        source->Offset = 0.0;
    }
    ALCdevice_Unlock(device);

done:
    UnlockSourceList(context);
    ALCcontext_DecRef(context);
}


AL_API ALvoid AL_APIENTRY alSourceQueueBuffers(ALuint src, ALsizei nb, const ALuint *buffers)
{
    ALCdevice *device;
    ALCcontext *context;
    ALsource *source;
    ALsizei i;
    ALbufferlistitem *BufferListStart;
    ALbufferlistitem *BufferList;
    ALbuffer *BufferFmt = NULL;
    ALuint oldchannels;

    if(nb == 0)
        return;

    context = GetContextRef();
    if(!context) return;

    device = context->Device;

    LockSourceList(context);
    if(!(nb >= 0))
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Queueing %d buffers", nb);
    if((source=LookupSource(context, src)) == NULL)
        SETERR_GOTO(context, AL_INVALID_NAME, done, "Invalid source ID %u", src);

    if(source->SourceType == AL_STATIC)
    {
        /* Can't queue on a Static Source */
        SETERR_GOTO(context, AL_INVALID_OPERATION, done, "Queueing onto static source %u", src);
    }

    /* Check for a valid Buffer, for its frequency and format */
    BufferList = source->queue;
    while(BufferList)
    {
        for(i = 0;i < BufferList->num_buffers;i++)
        {
            if((BufferFmt=BufferList->buffers[i]) != NULL)
                break;
        }
        if(BufferFmt) break;
        BufferList = ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed);
    }

    LockBufferList(device);
    BufferListStart = NULL;
    BufferList = NULL;
    for(i = 0;i < nb;i++)
    {
        ALbuffer *buffer = NULL;
        if(buffers[i] && (buffer=LookupBuffer(device, buffers[i])) == NULL)
            SETERR_GOTO(context, AL_INVALID_NAME, buffer_error, "Queueing invalid buffer ID %u",
                        buffers[i]);

        if(!BufferListStart)
        {
            BufferListStart = al_calloc(DEF_ALIGN,
                FAM_SIZE(ALbufferlistitem, buffers, 1));
            BufferList = BufferListStart;
        }
        else
        {
            ALbufferlistitem *item = al_calloc(DEF_ALIGN,
                FAM_SIZE(ALbufferlistitem, buffers, 1));
            ATOMIC_STORE(&BufferList->next, item, almemory_order_relaxed);
            BufferList = item;
        }
        ATOMIC_INIT(&BufferList->next, NULL);
        BufferList->max_samples = buffer ? buffer->SampleLen : 0;
        BufferList->num_buffers = 1;
        BufferList->buffers[0] = buffer;
        if(!buffer) continue;

        IncrementRef(&buffer->ref);

        if(buffer->MappedAccess != 0 && !(buffer->MappedAccess&AL_MAP_PERSISTENT_BIT_SOFT))
            SETERR_GOTO(context, AL_INVALID_OPERATION, buffer_error,
                        "Queueing non-persistently mapped buffer %u", buffer->id);

        if(BufferFmt == NULL)
            BufferFmt = buffer;
        else if(BufferFmt->Frequency != buffer->Frequency ||
                BufferFmt->FmtChannels != buffer->FmtChannels ||
                BufferFmt->OriginalType != buffer->OriginalType)
        {
            alSetError(context, AL_INVALID_OPERATION, "Queueing buffer with mismatched format");

        buffer_error:
            /* A buffer failed (invalid ID or format), so unlock and release
             * each buffer we had. */
            while(BufferListStart)
            {
                ALbufferlistitem *next = ATOMIC_LOAD(&BufferListStart->next,
                                                     almemory_order_relaxed);
                for(i = 0;i < BufferListStart->num_buffers;i++)
                {
                    if((buffer=BufferListStart->buffers[i]) != NULL)
                        DecrementRef(&buffer->ref);
                }
                al_free(BufferListStart);
                BufferListStart = next;
            }
            UnlockBufferList(device);
            goto done;
        }
    }
    /* All buffers good. */
    UnlockBufferList(device);

    /* Source is now streaming */
    source->SourceType = AL_STREAMING;

    oldchannels = source->SourceChannels;
    source->SourceChannels = ChannelsFromUserFmt(BufferListStart->buffers[0]->FmtChannels);

    if(oldchannels != source->SourceChannels)
    {
        if(source->SourceChannels == 1)
            source->EAXSrcProps.lRoom = EAXSOURCE_DEFAULTROOM;
        else if (source->SourceChannels >= 2)
            source->EAXSrcProps.lRoom = EAXSOURCE_MINROOM;

        if(context->Device->EAXIsActive)
        {
            CalcFilterGainsEAX(context, source);
            ApplyFilterGainsEAX(context);
            UpdateSourceEAX(context, source);
        }
    }

    if(!(BufferList=source->queue))
        source->queue = BufferListStart;
    else
    {
        ALbufferlistitem *next;
        while((next=ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed)) != NULL)
            BufferList = next;
        ATOMIC_STORE(&BufferList->next, BufferListStart, almemory_order_release);
    }

done:
    UnlockSourceList(context);
    ALCcontext_DecRef(context);
}

AL_API void AL_APIENTRY alSourceQueueBufferLayersSOFT(ALuint src, ALsizei nb, const ALuint *buffers)
{
    ALCdevice *device;
    ALCcontext *context;
    ALbufferlistitem *BufferListStart;
    ALbufferlistitem *BufferList;
    ALbuffer *BufferFmt = NULL;
    ALsource *source;
    ALsizei i;
    ALuint oldchannels;

    if(nb == 0)
        return;

    context = GetContextRef();
    if(!context) return;

    device = context->Device;

    LockSourceList(context);
    if(!(nb >= 0 && nb < 16))
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Queueing %d buffer layers", nb);
    if((source=LookupSource(context, src)) == NULL)
        SETERR_GOTO(context, AL_INVALID_NAME, done, "Invalid source ID %u", src);

    if(source->SourceType == AL_STATIC)
    {
        /* Can't queue on a Static Source */
        SETERR_GOTO(context, AL_INVALID_OPERATION, done, "Queueing onto static source %u", src);
    }

    /* Check for a valid Buffer, for its frequency and format */
    BufferList = source->queue;
    while(BufferList)
    {
        for(i = 0;i < BufferList->num_buffers;i++)
        {
            if((BufferFmt=BufferList->buffers[i]) != NULL)
                break;
        }
        if(BufferFmt) break;
        BufferList = ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed);
    }

    LockBufferList(device);
    BufferListStart = al_calloc(DEF_ALIGN, FAM_SIZE(ALbufferlistitem, buffers, nb));
    BufferList = BufferListStart;
    ATOMIC_INIT(&BufferList->next, NULL);
    BufferList->max_samples = 0;
    BufferList->num_buffers = 0;
    for(i = 0;i < nb;i++)
    {
        ALbuffer *buffer = NULL;
        if(buffers[i] && (buffer=LookupBuffer(device, buffers[i])) == NULL)
            SETERR_GOTO(context, AL_INVALID_NAME, buffer_error, "Queueing invalid buffer ID %u",
                        buffers[i]);

        BufferList->buffers[BufferList->num_buffers++] = buffer;
        if(!buffer) continue;

        IncrementRef(&buffer->ref);

        BufferList->max_samples = maxi(BufferList->max_samples, buffer->SampleLen);

        if(buffer->MappedAccess != 0 && !(buffer->MappedAccess&AL_MAP_PERSISTENT_BIT_SOFT))
            SETERR_GOTO(context, AL_INVALID_OPERATION, buffer_error,
                        "Queueing non-persistently mapped buffer %u", buffer->id);

        if(BufferFmt == NULL)
            BufferFmt = buffer;
        else if(BufferFmt->Frequency != buffer->Frequency ||
                BufferFmt->FmtChannels != buffer->FmtChannels ||
                BufferFmt->OriginalType != buffer->OriginalType)
        {
            alSetError(context, AL_INVALID_OPERATION, "Queueing buffer with mismatched format");

        buffer_error:
            /* A buffer failed (invalid ID or format), so unlock and release
             * each buffer we had. */
            while(BufferListStart)
            {
                ALbufferlistitem *next = ATOMIC_LOAD(&BufferListStart->next,
                                                     almemory_order_relaxed);
                for(i = 0;i < BufferListStart->num_buffers;i++)
                {
                    if((buffer=BufferListStart->buffers[i]) != NULL)
                        DecrementRef(&buffer->ref);
                }
                al_free(BufferListStart);
                BufferListStart = next;
            }
            UnlockBufferList(device);
            goto done;
        }
    }
    /* All buffers good. */
    UnlockBufferList(device);

    /* Source is now streaming */
    source->SourceType = AL_STREAMING;

    oldchannels = source->SourceChannels;
    source->SourceChannels = ChannelsFromUserFmt(BufferListStart->buffers[0]->FmtChannels);

    if(oldchannels != source->SourceChannels)
    {
        if(source->SourceChannels == 1)
            source->EAXSrcProps.lRoom = EAXSOURCE_DEFAULTROOM;
        else if(source->SourceChannels >= 2)
            source->EAXSrcProps.lRoom = EAXSOURCE_MINROOM;

        if(context->Device->EAXIsActive)
        {
            CalcFilterGainsEAX(context, source);
            ApplyFilterGainsEAX(context);
            UpdateSourceEAX(context, source);
        }
    }

    if(!(BufferList=source->queue))
        source->queue = BufferListStart;
    else
    {
        ALbufferlistitem *next;
        while((next=ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed)) != NULL)
            BufferList = next;
        ATOMIC_STORE(&BufferList->next, BufferListStart, almemory_order_release);
    }

done:
    UnlockSourceList(context);
    ALCcontext_DecRef(context);
}

AL_API ALvoid AL_APIENTRY alSourceUnqueueBuffers(ALuint src, ALsizei nb, ALuint *buffers)
{
    ALCcontext *context;
    ALsource *source;
    ALbufferlistitem *BufferList;
    ALbufferlistitem *Current;
    ALvoice *voice;
    ALsizei i;

    context = GetContextRef();
    if(!context) return;

    LockSourceList(context);
    if(!(nb >= 0))
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Unqueueing %d buffers", nb);
    if((source=LookupSource(context, src)) == NULL)
        SETERR_GOTO(context, AL_INVALID_NAME, done, "Invalid source ID %u", src);

    /* Nothing to unqueue. */
    if(nb == 0) goto done;

    if(source->Looping)
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Unqueueing from looping source %u", src);
    if(source->SourceType != AL_STREAMING)
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Unqueueing from a non-streaming source %u",
                    src);

    /* Make sure enough buffers have been processed to unqueue. */
    BufferList = source->queue;
    Current = NULL;
    if((voice=GetSourceVoice(source, context)) != NULL)
        Current = ATOMIC_LOAD(&voice->current_buffer, almemory_order_relaxed);
    else if(source->state == AL_INITIAL)
        Current = BufferList;
    if(BufferList == Current)
        SETERR_GOTO(context, AL_INVALID_VALUE, done, "Unqueueing pending buffers");

    i = BufferList->num_buffers;
    while(i < nb)
    {
        /* If the next bufferlist to check is NULL or is the current one, it's
         * trying to unqueue pending buffers.
         */
        ALbufferlistitem *next = ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed);
        if(!next || next == Current)
            SETERR_GOTO(context, AL_INVALID_VALUE, done, "Unqueueing pending buffers");
        BufferList = next;

        i += BufferList->num_buffers;
    }

    while(nb > 0)
    {
        ALbufferlistitem *head = source->queue;
        ALbufferlistitem *next = ATOMIC_LOAD(&head->next, almemory_order_relaxed);
        for(i = 0;i < head->num_buffers && nb > 0;i++,nb--)
        {
            ALbuffer *buffer = head->buffers[i];
            if(!buffer)
                *(buffers++) = 0;
            else
            {
                *(buffers++) = buffer->id;
                DecrementRef(&buffer->ref);
            }
        }
        if(i < head->num_buffers)
        {
            /* This head has some buffers left over, so move them to the front
             * and update the sample and buffer count.
             */
            ALsizei max_length = 0;
            ALsizei j = 0;
            while(i < head->num_buffers)
            {
                ALbuffer *buffer = head->buffers[i++];
                if(buffer) max_length = maxi(max_length, buffer->SampleLen);
                head->buffers[j++] = buffer;
            }
            head->max_samples = max_length;
            head->num_buffers = j;
            break;
        }

        /* Otherwise, free this item and set the source queue head to the next
         * one.
         */
        al_free(head);
        source->queue = next;
    }

done:
    UnlockSourceList(context);
    ALCcontext_DecRef(context);
}


static void InitSourceParams(ALsource *Source, ALsizei num_sends, LPGUID list)
{
    ALsizei i;
    EAXACTIVEFXSLOTS ActiveSlots = EAXSOURCE_3DDEFAULTACTIVEFXSLOTID;

    Source->InnerAngle = 360.0f;
    Source->OuterAngle = 360.0f;
    Source->Pitch = 1.0f;
    Source->Position[0] = 0.0f;
    Source->Position[1] = 0.0f;
    Source->Position[2] = 0.0f;
    Source->Velocity[0] = 0.0f;
    Source->Velocity[1] = 0.0f;
    Source->Velocity[2] = 0.0f;
    Source->Direction[0] = 0.0f;
    Source->Direction[1] = 0.0f;
    Source->Direction[2] = 0.0f;
    Source->Orientation[0][0] =  0.0f;
    Source->Orientation[0][1] =  0.0f;
    Source->Orientation[0][2] = -1.0f;
    Source->Orientation[1][0] =  0.0f;
    Source->Orientation[1][1] =  1.0f;
    Source->Orientation[1][2] =  0.0f;
    Source->RefDistance = 1.0f;
    Source->MaxDistance = FLT_MAX;
    Source->RolloffFactor = 1.0f;
    Source->Gain = 1.0f;
    Source->MinGain = 0.0f;
    Source->MaxGain = 1.0f;
    Source->OuterGain = 0.0f;
    Source->OuterGainHF = 1.0f;

    Source->DryGainHFAuto = AL_TRUE;
    Source->WetGainAuto = AL_TRUE;
    Source->WetGainHFAuto = AL_TRUE;
    Source->AirAbsorptionFactor = 0.0f;
    Source->AirAbsorptionGainHF = AIRABSORBGAINHF;
    Source->RoomRolloffFactor = 0.0f;
    Source->DopplerFactor = 1.0f;
    Source->HeadRelative = AL_FALSE;
    Source->Looping = AL_FALSE;
    Source->DistanceModel = DefaultDistanceModel;
    Source->Resampler = ResamplerDefault;
    Source->DirectChannels = AL_FALSE;
    Source->Spatialize = SpatializeAuto;

    Source->StereoPan[0] = DEG2RAD( 30.0f);
    Source->StereoPan[1] = DEG2RAD(-30.0f);

    Source->Radius = 0.0f;

    Source->Direct.Gain = 1.0f;
    Source->Direct.GainHF = 1.0f;
    Source->Direct.HFReference = LOWPASSFREQREF;
    Source->Direct.GainLF = 1.0f;
    Source->Direct.LFReference = HIGHPASSFREQREF;
    Source->Send = al_calloc(16, num_sends*sizeof(Source->Send[0]));
    for(i = 0;i < num_sends;i++)
    {
        Source->Send[i].Slot = NULL;
        Source->Send[i].Gain = 1.0f;
        Source->Send[i].GainHF = 1.0f;
        Source->Send[i].HFReference = LOWPASSFREQREF;
        Source->Send[i].GainLF = 1.0f;
        Source->Send[i].LFReference = HIGHPASSFREQREF;
    }

    Source->EAXSrcProps.lDirect =                EAXSOURCE_DEFAULTDIRECT;
    Source->EAXSrcProps.lDirectHF =              EAXSOURCE_DEFAULTDIRECTHF;
    Source->EAXSrcProps.lRoom =                  EAXSOURCE_DEFAULTROOM;
    Source->EAXSrcProps.lRoomHF =                EAXSOURCE_DEFAULTROOMHF;
    Source->EAXSrcProps.lObstruction =           EAXSOURCE_DEFAULTOBSTRUCTION;
    Source->EAXSrcProps.flObstructionLFRatio =   EAXSOURCE_DEFAULTOBSTRUCTIONLFRATIO;
    Source->EAXSrcProps.lOcclusion =             EAXSOURCE_DEFAULTOCCLUSION;
    Source->EAXSrcProps.flOcclusionLFRatio =     EAXSOURCE_DEFAULTOCCLUSIONLFRATIO;
    Source->EAXSrcProps.flOcclusionRoomRatio =   EAXSOURCE_DEFAULTOCCLUSIONROOMRATIO;
    Source->EAXSrcProps.flOcclusionDirectRatio = EAXSOURCE_DEFAULTOCCLUSIONDIRECTRATIO;
    Source->EAXSrcProps.lExclusion =             EAXSOURCE_DEFAULTEXCLUSION;
    Source->EAXSrcProps.flExclusionLFRatio =     EAXSOURCE_DEFAULTEXCLUSIONLFRATIO;
    Source->EAXSrcProps.lOutsideVolumeHF =       EAXSOURCE_DEFAULTOUTSIDEVOLUMEHF;
    Source->EAXSrcProps.flDopplerFactor =        EAXSOURCE_DEFAULTDOPPLERFACTOR;
    Source->EAXSrcProps.flRolloffFactor =        EAXSOURCE_DEFAULTROLLOFFFACTOR;
    Source->EAXSrcProps.flRoomRolloffFactor =    EAXSOURCE_DEFAULTROOMROLLOFFFACTOR;
    Source->EAXSrcProps.flAirAbsorptionFactor =  EAXSOURCE_DEFAULTAIRABSORPTIONFACTOR;
    Source->EAXSrcProps.ulFlags =                EAXSOURCE_DEFAULTFLAGS;
    Source->EAXSrcProps.flMacroFXFactor =        EAXSOURCE_DEFAULTMACROFXFACTOR;

    Source->EAXActiveSlots =                ActiveSlots;
    Source->EAXSpeakersLevel.lLevel =       EAXSOURCE_DEFAULTSPEAKERLEVEL;
    Source->EAXSpeakersLevel.lSpeakerID =   EAXSPEAKER_FRONT_CENTER;  //review and analyzing it
    for (i=0; i<EAX_MAX_FXSLOTS; i++)
    {
        Source->EAXSendProps[i].guidReceivingFXSlotID =  list[i];
        Source->EAXSendProps[i].lSend =                  EAXSOURCE_DEFAULTSEND;
        Source->EAXSendProps[i].lSendHF =                EAXSOURCE_DEFAULTSENDHF;
        Source->EAXSendProps[i].lOcclusion =             EAXSOURCE_DEFAULTOCCLUSION;
        Source->EAXSendProps[i].flOcclusionLFRatio =     EAXSOURCE_DEFAULTOCCLUSIONLFRATIO;
        Source->EAXSendProps[i].flOcclusionRoomRatio =   EAXSOURCE_DEFAULTOCCLUSIONROOMRATIO;
        Source->EAXSendProps[i].flOcclusionDirectRatio = EAXSOURCE_DEFAULTOCCLUSIONDIRECTRATIO;
        Source->EAXSendProps[i].lExclusion =             EAXSOURCE_DEFAULTEXCLUSION;
        Source->EAXSendProps[i].flExclusionLFRatio =     EAXSOURCE_DEFAULTEXCLUSIONLFRATIO;

        Source->EAXSrcData.ActiveSends[i].SlotIdx = SLOT_NULL;
        Source->EAXSrcData.ActiveSends[i].Primary = AL_FALSE;
        Source->EAXSrcData.ActiveSends[i].Enviroment = AL_TRUE;
        Source->EAXSrcData.ActiveSends[i].Upmix = AL_TRUE;
    }
    Source->EAXSrcData.ActiveSends[1].SlotIdx = SLOT_0;
    Source->EAXSrcData.ActiveSends[1].Primary = AL_TRUE;

    Source->SourceChannels = AL_NONE;

    Source->Offset = 0.0;
    Source->OffsetType = AL_NONE;
    Source->SourceType = AL_UNDETERMINED;
    Source->state = AL_INITIAL;

    Source->queue = NULL;

    /* No way to do an 'init' here, so just test+set with relaxed ordering and
     * ignore the test.
     */
    ATOMIC_FLAG_TEST_AND_SET(&Source->PropsClean, almemory_order_relaxed);

    Source->VoiceIdx = -1;
}

static void DeinitSource(ALsource *source, ALsizei num_sends)
{
    ALbufferlistitem *BufferList;
    ALsizei i;

    BufferList = source->queue;
    while(BufferList != NULL)
    {
        ALbufferlistitem *next = ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed);
        for(i = 0;i < BufferList->num_buffers;i++)
        {
            if(BufferList->buffers[i] != NULL)
                DecrementRef(&BufferList->buffers[i]->ref);
        }
        al_free(BufferList);
        BufferList = next;
    }
    source->queue = NULL;

    if(source->Send)
    {
        for(i = 0;i < num_sends;i++)
        {
            if(source->Send[i].Slot)
                DecrementRef(&source->Send[i].Slot->ref);
            source->Send[i].Slot = NULL;
        }
        al_free(source->Send);
        source->Send = NULL;
    }
}

static void UpdateSourceProps(ALsource *source, ALvoice *voice, ALsizei num_sends, ALCcontext *context)
{
    struct ALvoiceProps *props;
    ALsizei i;

    /* Get an unused property container, or allocate a new one as needed. */
    props = ATOMIC_LOAD(&context->FreeVoiceProps, almemory_order_acquire);
    if(!props)
        props = al_calloc(16, FAM_SIZE(struct ALvoiceProps, Send, num_sends));
    else
    {
        struct ALvoiceProps *next;
        do {
            next = ATOMIC_LOAD(&props->next, almemory_order_relaxed);
        } while(ATOMIC_COMPARE_EXCHANGE_PTR_WEAK(&context->FreeVoiceProps, &props, next,
                almemory_order_acq_rel, almemory_order_acquire) == 0);
    }

    /* Copy in current property values. */
    props->Pitch = source->Pitch;
    props->Gain = source->Gain;
    props->OuterGain = source->OuterGain;
    props->MinGain = source->MinGain;
    props->MaxGain = source->MaxGain;
    props->InnerAngle = source->InnerAngle;
    props->OuterAngle = source->OuterAngle;
    props->RefDistance = source->RefDistance;
    props->MaxDistance = source->MaxDistance;
    props->RolloffFactor = source->RolloffFactor;
    for(i = 0;i < 3;i++)
        props->Position[i] = source->Position[i];
    for(i = 0;i < 3;i++)
        props->Velocity[i] = source->Velocity[i];
    for(i = 0;i < 3;i++)
        props->Direction[i] = source->Direction[i];
    for(i = 0;i < 2;i++)
    {
        ALsizei j;
        for(j = 0;j < 3;j++)
            props->Orientation[i][j] = source->Orientation[i][j];
    }
    props->HeadRelative = source->HeadRelative;
    props->DistanceModel = source->DistanceModel;
    props->Resampler = source->Resampler;
    props->DirectChannels = source->DirectChannels;
    props->SpatializeMode = source->Spatialize;

    props->DryGainHFAuto = source->DryGainHFAuto;
    props->WetGainAuto = source->WetGainAuto;
    props->WetGainHFAuto = source->WetGainHFAuto;
    props->OuterGainHF = source->OuterGainHF;

    props->AirAbsorptionFactor = source->AirAbsorptionFactor;
    props->AirAbsorptionGainHF = source->AirAbsorptionGainHF;
    props->RoomRolloffFactor = source->RoomRolloffFactor;
    props->DopplerFactor = source->DopplerFactor;

    props->StereoPan[0] = source->StereoPan[0];
    props->StereoPan[1] = source->StereoPan[1];

    props->Radius = source->Radius;

    props->Direct.Gain = source->Direct.Gain;
    props->Direct.GainHF = source->Direct.GainHF;
    props->Direct.HFReference = source->Direct.HFReference;
    props->Direct.GainLF = source->Direct.GainLF;
    props->Direct.LFReference = source->Direct.LFReference;

    for(i = 0;i < num_sends;i++)
    {
        props->Send[i].Slot = source->Send[i].Slot;
        props->Send[i].Gain = source->Send[i].Gain;
        props->Send[i].GainHF = source->Send[i].GainHF;
        props->Send[i].HFReference = source->Send[i].HFReference;
        props->Send[i].GainLF = source->Send[i].GainLF;
        props->Send[i].LFReference = source->Send[i].LFReference;
    }

    /* Set the new container for updating internal parameters. */
    props = ATOMIC_EXCHANGE_PTR(&voice->Update, props, almemory_order_acq_rel);
    if(props)
    {
        /* If there was an unused update container, put it back in the
         * freelist.
         */
        ATOMIC_REPLACE_HEAD(struct ALvoiceProps*, &context->FreeVoiceProps, props);
    }
}

void UpdateAllSourceProps(ALCcontext *context)
{
    ALsizei num_sends = context->Device->NumAuxSends;
    ALsizei pos;

    for(pos = 0;pos < context->VoiceCount;pos++)
    {
        ALvoice *voice = context->Voices[pos];
        ALsource *source = ATOMIC_LOAD(&voice->Source, almemory_order_acquire);
        if(source && !ATOMIC_FLAG_TEST_AND_SET(&source->PropsClean, almemory_order_acq_rel))
            UpdateSourceProps(source, voice, num_sends, context);
    }
}

static ALboolean GenSourceSendPrimaryEAX(ALCcontext *Context, ALsource *Source)
{
    ALeffectslot *slot   = NULL;
    ALCdevice    *device = Context->Device;
    ALint         idx    = Context->Device->EAXhw.PrimaryIdx;
    ALuint        value;

    value = (idx != SLOT_NULL) ? device->EAXhw.Slots[idx].Idx : AL_EFFECTSLOT_NULL;

    LockEffectSlotList(Context);
    if(!(value == AL_EFFECTSLOT_NULL || (slot = LookupEffectSlot(Context, value)) != NULL))
    {
        UnlockEffectSlotList(Context);
        SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid effect ID %u", value);
    }

    if(slot != Source->Send[SEND_1].Slot)
    {
        //ALvoice *voice;
        /* Add refcount on the new slot, and release the previous slot */
        if(slot) IncrementRef(&slot->ref);
        if(Source->Send[SEND_1].Slot)
            DecrementRef(&Source->Send[SEND_1].Slot->ref);
        Source->Send[SEND_1].Slot = slot;
    }
    UnlockEffectSlotList(Context);

    return AL_TRUE;
}

ALenum ApplyFilterGainsEAX(ALCcontext *context)
{
    EAXFILTERS *filter;
    ALsizei i;

    filter = &context->Device->EAXhw.Filters;

    /* Assign the EFX filter ID to the structure data passed by the function parameter.
       Limit de value to 0 dB acc. to EAX 4.0 programmer guide, pag. 59 */
    filter->Direct.Gain = minf(filter->Direct.Gain, EAX_DEFAULT_GAIN);

    if(filter->Direct.Idx)
    {
        alFilterf(filter->Direct.Idx, AL_LOWPASS_GAIN, mB_to_gain(filter->Direct.Gain));
        alFilterf(filter->Direct.Idx, AL_LOWPASS_GAINHF, mB_to_gain(filter->Direct.GainHF));
    }

    for(i=0; i<EAX_MAX_FXSLOTS; i++)
    {
        /* Assign the EFX filter ID to the structure data passed by the function parameter.
           Limit de value to 0 dB acc. to EAX 4.0 programmer guide, pag. 60 */
        filter->Send[i].Gain = minf(filter->Send[i].Gain, EAX_DEFAULT_GAIN);

        if (filter->Send[i].Idx)
        {
            alFilterf(filter->Send[i].Idx, AL_LOWPASS_GAIN, mB_to_gain(filter->Send[i].Gain));
            alFilterf(filter->Send[i].Idx, AL_LOWPASS_GAINHF, mB_to_gain(filter->Send[i].GainHF));
        }
     }
    return AL_NO_ERROR;
}

/* CalcFilterGainsEAX function. Computes de gains values of the
   direct and sends filters. */
ALenum CalcFilterGainsEAX(ALCcontext *context, ALsource *source)
{
    EAXSOURCEPROPERTIES *props;
    EAXFILTERGAINS EAXGains[EAX_MAX_FXSLOTS] = {EAX_DEFAULT_GAIN}; //Reset all gains to 0 dB
    EAXFILTERS    *filter;
    ALuint         MaxSends;
    ALCdevice     *device;
    ALuint i;

    device   =  context->Device;
    props    = &source->EAXSrcProps;
    MaxSends =  device->EAXSession.ulMaxActiveSends;
    filter   = &device->EAXhw.Filters;

    for (i=0; i<EAX_MAX_FXSLOTS; i++)
    {
        EAXSOURCEALLSENDPROPERTIES *send;
        EAXFXSLOTPROPERTIES        *FXSlot;

        /* Load pointers */
        send   = &source->EAXSendProps[i];
        FXSlot = &device->EAXSlotProps[i];

        /* EAX Send, SendHF, Unconditional */
        EAXGains[i].Send    = (ALfloat)send->lSend;
        EAXGains[i].SendHF  = (ALfloat)send->lSendHF;

        /*EAX FXSlot Occlusion, Occlusion HF ratio, Unconditional */
        EAXGains[i].Send   += FXSlot->flOcclusionLFRatio*FXSlot->lOcclusion;
        EAXGains[i].SendHF += (ALfloat)FXSlot->lOcclusion;

        if (FXSlot->ulFlags & EAXFXSLOTFLAGS_ENVIRONMENT)
        {
            /* EAX >=4 Send Occlusion */
            EAXGains[i].Direct   = maxf(send->flOcclusionLFRatio + send->flOcclusionDirectRatio - 1.0f,
                                        send->flOcclusionLFRatio*send->flOcclusionDirectRatio) * 
                                        send->lOcclusion;

            EAXGains[i].DirectHF = send->lOcclusion*send->flOcclusionDirectRatio;

            EAXGains[i].Send    += maxf(send->flOcclusionLFRatio + send->flOcclusionRoomRatio - 1.0f,
                                        send->flOcclusionLFRatio*send->flOcclusionRoomRatio) * 
                                        send->lOcclusion;

            EAXGains[i].SendHF  += send->lOcclusion*send->flOcclusionRoomRatio;

            /* EAX >=4 Send Exclusion, the previous results are added */
            EAXGains[i].Send    += send->lExclusion*send->flExclusionLFRatio;
            EAXGains[i].SendHF  += (ALfloat)send->lExclusion;

            /* EAX Room, RoomHF, Unconditional under ENVIRONMENT active flag */
            EAXGains[i].Send    += (ALfloat)props->lRoom;
            EAXGains[i].SendHF  += (ALfloat)props->lRoomHF;
        }

        /* Mute all sends of the data-out filter*/
        filter->Send[i].Gain   = EAX_MIN_GAIN;
        filter->Send[i].GainHF = EAX_MIN_GAIN;
    }

    /* EAX Direct path, Unconditional */
    filter->Direct.Gain    = (ALfloat)props->lDirect;
    filter->Direct.GainHF  = (ALfloat)props->lDirectHF;

    /* EAX general Obstruction, Unconditional */
    filter->Direct.Gain   += props->lObstruction*props->flObstructionLFRatio;
    filter->Direct.GainHF += (ALfloat)props->lObstruction;

    for (i=0; i<MaxSends; i++)
    {
        ALboolean Primary;
        ALuint    Idx;

        Primary = source->EAXSrcData.ActiveSends[i].Primary;
        Idx     = source->EAXSrcData.ActiveSends[i].SlotIdx;

        if (Idx == SLOT_NULL) continue;

        if ((device->EAXSlotProps[Idx].ulFlags & EAXFXSLOTFLAGS_ENVIRONMENT) && Primary)
        {
            /* EAX <= 3.0 occlusion */
            filter->Direct.Gain    += maxf(props->flOcclusionLFRatio + props->flOcclusionDirectRatio - 1.0f,
                                           props->flOcclusionLFRatio*props->flOcclusionDirectRatio) * 
                                           props->lOcclusion + EAXGains[Idx].Direct;

            filter->Direct.GainHF  += props->lOcclusion*props->flOcclusionDirectRatio + EAXGains[Idx].DirectHF;

            filter->Send[i].Gain    = maxf(props->flOcclusionLFRatio + props->flOcclusionRoomRatio - 1.0f,
                                           props->flOcclusionLFRatio*props->flOcclusionRoomRatio) * 
                                           props->lOcclusion + EAXGains[Idx].Send;

            filter->Send[i].GainHF  = props->lOcclusion*props->flOcclusionRoomRatio + EAXGains[Idx].SendHF;

            /* EAX <= 3.0 exclusion */
            filter->Send[i].Gain   += props->lExclusion*props->flExclusionLFRatio;
            filter->Send[i].GainHF += (ALfloat)props->lExclusion;
        }
        else
        {
            /* Done, All sends was processed */
            filter->Send[i].Gain   = EAXGains[Idx].Send;
            filter->Send[i].GainHF = EAXGains[Idx].SendHF;
        }
    }

    return AL_NO_ERROR;
}

void UpdateSourceEAX(ALCcontext *Context, ALsource *Source)
{
    ALCdevice    *device  = Context->Device;
    ALfilter     *direct_filter = NULL;
    ALfilter     *send_filter[EAX_MAX_FXSLOTS] = {NULL};
    ALeffectslot *slot[EAX_MAX_FXSLOTS] = {NULL};
    ALvoice      *voice;
    ALboolean     update;
    ALuint        idx;
    ALuint        i;
    ALfloat       AirAbsHF;
    ALfloat       HFRef;
    ALuint        NumSends;
    ALuint        NumSlots;

    update   = AL_FALSE;
    AirAbsHF = Context->EAXCxtProps.flAirAbsorptionHF;
    HFRef    = Context->EAXCxtProps.flHFReference;
    NumSends = device->EAXSession.ulMaxActiveSends;
    NumSlots = (device->EAXhw.MultiSlot) ? EAX_MAX_FXSLOTS : EAX_MIN_FXSLOTS;

    LockEffectSlotList(Context);

    for (i=0; i<NumSlots; i++)
    {
        idx = device->EAXhw.Slots[i].Idx;

        if(!(idx == AL_EFFECTSLOT_NULL || (slot[i]=LookupEffectSlot(Context, idx)) != NULL))
        {
            UnlockEffectSlotList(Context);
            SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid effect slot ID %u", idx);
        }
    }

    LockFilterList(device);

    idx = device->EAXhw.Filters.Direct.Idx;

    if(!(idx == AL_FILTER_NULL || (direct_filter=LookupFilter(device, idx)) != NULL))
    {
        UnlockFilterList(device);
        UnlockEffectSlotList(Context);
        SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid direct filter ID %u", idx);
    }

    for (i=0; i<NumSends; i++)
    {
        idx = device->EAXhw.Filters.Send[i].Idx;

        if(!((ALuint)i < NumSends))
        {
            UnlockFilterList(device);
            UnlockEffectSlotList(Context);
            SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid send %u", i);
        }
        if(!(idx == AL_FILTER_NULL || (send_filter[i]=LookupFilter(device, idx)) != NULL))
        {
            UnlockFilterList(device);
            UnlockEffectSlotList(Context);
            SETERR_RETURN(Context, AL_INVALID_VALUE, AL_FALSE, "Invalid send filter ID %u", idx);
        }
    }

    for(i=0; i<EAX_MAX_FXSLOTS; i++)
        if (Source->EAXSrcData.ActiveSends[i].Primary)
            Source->EAXSrcData.ActiveSends[i].SlotIdx = Context->Device->EAXhw.PrimaryIdx;

    /* ApplyAttnSourceParams */
    Source->DryGainHFAuto = (Source->EAXSrcProps.ulFlags & EAXSOURCEFLAGS_DIRECTHFAUTO) ? AL_TRUE : AL_FALSE;
    Source->WetGainAuto   = (Source->EAXSrcProps.ulFlags & EAXSOURCEFLAGS_ROOMAUTO) ? AL_TRUE : AL_FALSE;
    Source->WetGainHFAuto = (Source->EAXSrcProps.ulFlags & EAXSOURCEFLAGS_ROOMHFAUTO) ? AL_TRUE : AL_FALSE;
   /* REVIEW EAX 4.0 */
    Source->OuterGainHF         = mB_to_gain((ALfloat)Source->EAXSrcProps.lOutsideVolumeHF);
    Source->DopplerFactor       = Source->EAXSrcProps.flDopplerFactor;
    Source->RolloffFactor       = /*AL_ROLLOFF_DEFAULT_FACTOR*/1.0f + Source->EAXSrcProps.flRolloffFactor;
    Source->RoomRolloffFactor   = Source->EAXSrcProps.flRoomRolloffFactor;
    Source->AirAbsorptionFactor = Source->EAXSrcProps.flAirAbsorptionFactor;
    /* REVIEW EAX 5.0 */
    if(device->EAXManager.Target == EAX50_TARGET)
        Source->Spatialize = (Source->EAXSrcProps.ulFlags & EAXSOURCEFLAGS_UPMIX) ? AL_TRUE : AL_AUTO_SOFT;

    if(!direct_filter)
    {
        /* Disable filter */
        Source->Direct.Gain = 1.0f;
        Source->Direct.GainHF = 1.0f;
        Source->Direct.HFReference = LOWPASSFREQREF;
        Source->Direct.GainLF = 1.0f;
        Source->Direct.LFReference = HIGHPASSFREQREF;
    }
    else
    {
        Source->Direct.Gain = direct_filter->Gain;
        Source->Direct.GainHF = direct_filter->GainHF;
        Source->Direct.HFReference = direct_filter->HFReference;
        Source->Direct.GainLF = direct_filter->GainLF;
        Source->Direct.LFReference = direct_filter->LFReference;
    }

    for(i=0; i<NumSends; i++)
    {
        ALeffectslot *slot_final;
        ALfilter *send_filter_final;
        ALboolean IsEnvironment;
        ALboolean IsReverb;
        ALboolean IsHFRef;
        ALint     primary;

        primary = device->EAXhw.PrimaryIdx;
        IsHFRef = AL_TRUE;

        if (Source->EAXSrcData.ActiveSends[i].SlotIdx == SLOT_NULL)
        {
            slot_final        = NULL;
            send_filter_final = NULL;
        }
        else
        {
            if(Source->EAXSrcData.ActiveSends[i].Primary)
            {

                IsEnvironment = device->EAXSlotProps[primary].ulFlags &
                                EAXFXSLOTFLAGS_ENVIRONMENT;
                IsReverb      = device->EAXhw.Slots[primary].EffType == _EAX_REVERB_EFFECT ?
                                AL_TRUE : AL_FALSE;

                if(IsEnvironment && IsReverb)
                {
                    AirAbsHF = device->EAXEffectProps[primary].EAXReverb.flAirAbsorptionHF;
                    IsHFRef  = AL_FALSE;
                }
            }
            idx = Source->EAXSrcData.ActiveSends[i].SlotIdx;
            slot_final        = slot[idx];
            send_filter_final = send_filter[i];
        }

        if(!send_filter_final)
        {
            /* Disable filter */
            Source->Send[i].Gain = 1.0f;
            Source->Send[i].GainHF = 1.0f;
            Source->Send[i].HFReference = LOWPASSFREQREF;
            Source->Send[i].GainLF = 1.0f;
            Source->Send[i].LFReference = HIGHPASSFREQREF;
        }
        else
        {
            Source->Send[i].Gain = send_filter_final->Gain;
            Source->Send[i].GainHF = send_filter_final->GainHF;
            Source->Send[i].HFReference = IsHFRef ? HFRef : send_filter_final->HFReference;
            Source->Send[i].GainLF = send_filter_final->GainLF;
            Source->Send[i].LFReference = send_filter_final->LFReference;
        }

        if (slot_final != Source->Send[i].Slot && IsPlayingOrPaused(Source))
        {
            /* We must force an update if the auxiliary slot changed on an
             * active source, in case the slot is about to be deleted.
             */
            update = AL_TRUE;
        }
        /* Add refcount on the new slot, and release the previous slot */
        if(slot_final) IncrementRef(&slot_final->ref);
        if(Source->Send[i].Slot)
            DecrementRef(&Source->Send[i].Slot->ref);
        Source->Send[i].Slot = slot_final;
    }

    Source->AirAbsorptionGainHF = clampf(mB_to_gain(AirAbsHF), AIRABSORBGAINHFMIN, AIRABSORBGAINHFMAX);
   
    voice = GetSourceVoice(Source, Context);
    if(voice != NULL && (update || SourceShouldUpdate(Source, Context)))
        UpdateSourceProps(Source, voice, device->NumAuxSends, Context);
    else
        ATOMIC_FLAG_CLEAR(&Source->PropsClean, almemory_order_release);

    UnlockFilterList(device);
    UnlockEffectSlotList(Context);
}

void UpdateAllSourcesEAX(ALCcontext *context)
{
    SourceSubList *sublist, *subend;

    almtx_lock(&context->SourceLock);
    sublist = VECTOR_BEGIN(context->SourceList);
    subend = VECTOR_END(context->SourceList);
    for(;sublist != subend;++sublist)
    {
        ALuint64 usemask = ~sublist->FreeMask;
        while(usemask)
        {
            ALsizei idx = CTZ64(usemask);
            ALsource *source = sublist->Sources + idx;

            usemask &= ~(U64(1) << idx);

            CalcFilterGainsEAX(context, source);
            ApplyFilterGainsEAX(context);
            UpdateSourceEAX(context, source);
        }
    }
    almtx_unlock(&context->SourceLock);
}

/* GetSourceSampleOffset
 *
 * Gets the current read offset for the given Source, in 32.32 fixed-point
 * samples. The offset is relative to the start of the queue (not the start of
 * the current buffer).
 */
static ALint64 GetSourceSampleOffset(ALsource *Source, ALCcontext *context, ALuint64 *clocktime)
{
    ALCdevice *device = context->Device;
    const ALbufferlistitem *Current;
    ALuint64 readPos;
    ALuint refcount;
    ALvoice *voice;

    do {
        Current = NULL;
        readPos = 0;
        while(((refcount=ATOMIC_LOAD(&device->MixCount, almemory_order_acquire))&1))
            althrd_yield();
        *clocktime = GetDeviceClockTime(device);

        voice = GetSourceVoice(Source, context);
        if(voice)
        {
            Current = ATOMIC_LOAD(&voice->current_buffer, almemory_order_relaxed);

            readPos  = (ALuint64)ATOMIC_LOAD(&voice->position, almemory_order_relaxed) << 32;
            readPos |= (ALuint64)ATOMIC_LOAD(&voice->position_fraction, almemory_order_relaxed) <<
                       (32-FRACTIONBITS);
        }
        ATOMIC_THREAD_FENCE(almemory_order_acquire);
    } while(refcount != ATOMIC_LOAD(&device->MixCount, almemory_order_relaxed));

    if(voice)
    {
        const ALbufferlistitem *BufferList = Source->queue;
        while(BufferList && BufferList != Current)
        {
            readPos += (ALuint64)BufferList->max_samples << 32;
            BufferList = ATOMIC_LOAD(&CONST_CAST(ALbufferlistitem*,BufferList)->next,
                                     almemory_order_relaxed);
        }
        readPos = minu64(readPos, U64(0x7fffffffffffffff));
    }

    return (ALint64)readPos;
}

/* GetSourceSecOffset
 *
 * Gets the current read offset for the given Source, in seconds. The offset is
 * relative to the start of the queue (not the start of the current buffer).
 */
static ALdouble GetSourceSecOffset(ALsource *Source, ALCcontext *context, ALuint64 *clocktime)
{
    ALCdevice *device = context->Device;
    const ALbufferlistitem *Current;
    ALuint64 readPos;
    ALuint refcount;
    ALdouble offset;
    ALvoice *voice;

    do {
        Current = NULL;
        readPos = 0;
        while(((refcount=ATOMIC_LOAD(&device->MixCount, almemory_order_acquire))&1))
            althrd_yield();
        *clocktime = GetDeviceClockTime(device);

        voice = GetSourceVoice(Source, context);
        if(voice)
        {
            Current = ATOMIC_LOAD(&voice->current_buffer, almemory_order_relaxed);

            readPos  = (ALuint64)ATOMIC_LOAD(&voice->position, almemory_order_relaxed) <<
                       FRACTIONBITS;
            readPos |= ATOMIC_LOAD(&voice->position_fraction, almemory_order_relaxed);
        }
        ATOMIC_THREAD_FENCE(almemory_order_acquire);
    } while(refcount != ATOMIC_LOAD(&device->MixCount, almemory_order_relaxed));

    offset = 0.0;
    if(voice)
    {
        const ALbufferlistitem *BufferList = Source->queue;
        const ALbuffer *BufferFmt = NULL;
        while(BufferList && BufferList != Current)
        {
            ALsizei i = 0;
            while(!BufferFmt && i < BufferList->num_buffers)
                BufferFmt = BufferList->buffers[i++];
            readPos += (ALuint64)BufferList->max_samples << FRACTIONBITS;
            BufferList = ATOMIC_LOAD(&CONST_CAST(ALbufferlistitem*,BufferList)->next,
                                     almemory_order_relaxed);
        }

        while(BufferList && !BufferFmt)
        {
            ALsizei i = 0;
            while(!BufferFmt && i < BufferList->num_buffers)
                BufferFmt = BufferList->buffers[i++];
            BufferList = ATOMIC_LOAD(&CONST_CAST(ALbufferlistitem*,BufferList)->next,
                                     almemory_order_relaxed);
        }
        assert(BufferFmt != NULL);

        offset = (ALdouble)readPos / (ALdouble)FRACTIONONE /
                 (ALdouble)BufferFmt->Frequency;
    }

    return offset;
}

/* GetSourceOffset
 *
 * Gets the current read offset for the given Source, in the appropriate format
 * (Bytes, Samples or Seconds). The offset is relative to the start of the
 * queue (not the start of the current buffer).
 */
static ALdouble GetSourceOffset(ALsource *Source, ALenum name, ALCcontext *context)
{
    ALCdevice *device = context->Device;
    const ALbufferlistitem *Current;
    ALuint readPos;
    ALsizei readPosFrac;
    ALuint refcount;
    ALdouble offset;
    ALvoice *voice;

    do {
        Current = NULL;
        readPos = readPosFrac = 0;
        while(((refcount=ATOMIC_LOAD(&device->MixCount, almemory_order_acquire))&1))
            althrd_yield();
        voice = GetSourceVoice(Source, context);
        if(voice)
        {
            Current = ATOMIC_LOAD(&voice->current_buffer, almemory_order_relaxed);

            readPos = ATOMIC_LOAD(&voice->position, almemory_order_relaxed);
            readPosFrac = ATOMIC_LOAD(&voice->position_fraction, almemory_order_relaxed);
        }
        ATOMIC_THREAD_FENCE(almemory_order_acquire);
    } while(refcount != ATOMIC_LOAD(&device->MixCount, almemory_order_relaxed));

    offset = 0.0;
    if(voice)
    {
        const ALbufferlistitem *BufferList = Source->queue;
        const ALbuffer *BufferFmt = NULL;
        ALboolean readFin = AL_FALSE;
        ALuint totalBufferLen = 0;

        while(BufferList != NULL)
        {
            ALsizei i = 0;
            while(!BufferFmt && i < BufferList->num_buffers)
                BufferFmt = BufferList->buffers[i++];

            readFin |= (BufferList == Current);
            totalBufferLen += BufferList->max_samples;
            if(!readFin) readPos += BufferList->max_samples;

            BufferList = ATOMIC_LOAD(&CONST_CAST(ALbufferlistitem*,BufferList)->next,
                                     almemory_order_relaxed);
        }
        assert(BufferFmt != NULL);

        if(Source->Looping)
            readPos %= totalBufferLen;
        else
        {
            /* Wrap back to 0 */
            if(readPos >= totalBufferLen)
                readPos = readPosFrac = 0;
        }

        offset = 0.0;
        switch(name)
        {
            case AL_SEC_OFFSET:
                offset = (readPos + (ALdouble)readPosFrac/FRACTIONONE) / BufferFmt->Frequency;
                break;

            case AL_SAMPLE_OFFSET:
                offset = readPos + (ALdouble)readPosFrac/FRACTIONONE;
                break;

            case AL_BYTE_OFFSET:
                if(BufferFmt->OriginalType == UserFmtIMA4)
                {
                    ALsizei align = (BufferFmt->OriginalAlign-1)/2 + 4;
                    ALuint BlockSize = align * ChannelsFromFmt(BufferFmt->FmtChannels);
                    ALuint FrameBlockSize = BufferFmt->OriginalAlign;

                    /* Round down to nearest ADPCM block */
                    offset = (ALdouble)(readPos / FrameBlockSize * BlockSize);
                }
                else if(BufferFmt->OriginalType == UserFmtMSADPCM)
                {
                    ALsizei align = (BufferFmt->OriginalAlign-2)/2 + 7;
                    ALuint BlockSize = align * ChannelsFromFmt(BufferFmt->FmtChannels);
                    ALuint FrameBlockSize = BufferFmt->OriginalAlign;

                    /* Round down to nearest ADPCM block */
                    offset = (ALdouble)(readPos / FrameBlockSize * BlockSize);
                }
                else
                {
                    ALuint FrameSize = FrameSizeFromFmt(BufferFmt->FmtChannels,
                                                        BufferFmt->FmtType);
                    offset = (ALdouble)(readPos * FrameSize);
                }
                break;
        }
    }

    return offset;
}


/* ApplyOffset
 *
 * Apply the stored playback offset to the Source. This function will update
 * the number of buffers "played" given the stored offset.
 */
static ALboolean ApplyOffset(ALsource *Source, ALvoice *voice)
{
    ALbufferlistitem *BufferList;
    ALuint totalBufferLen;
    ALuint offset = 0;
    ALsizei frac = 0;

    /* Get sample frame offset */
    if(!GetSampleOffset(Source, &offset, &frac))
        return AL_FALSE;

    totalBufferLen = 0;
    BufferList = Source->queue;
    while(BufferList && totalBufferLen <= offset)
    {
        if((ALuint)BufferList->max_samples > offset-totalBufferLen)
        {
            /* Offset is in this buffer */
            ATOMIC_STORE(&voice->position, offset - totalBufferLen, almemory_order_relaxed);
            ATOMIC_STORE(&voice->position_fraction, frac, almemory_order_relaxed);
            ATOMIC_STORE(&voice->current_buffer, BufferList, almemory_order_release);
            return AL_TRUE;
        }
        totalBufferLen += BufferList->max_samples;

        BufferList = ATOMIC_LOAD(&BufferList->next, almemory_order_relaxed);
    }

    /* Offset is out of range of the queue */
    return AL_FALSE;
}


/* GetSampleOffset
 *
 * Retrieves the sample offset into the Source's queue (from the Sample, Byte
 * or Second offset supplied by the application). This takes into account the
 * fact that the buffer format may have been modifed since.
 */
static ALboolean GetSampleOffset(ALsource *Source, ALuint *offset, ALsizei *frac)
{
    const ALbuffer *BufferFmt = NULL;
    const ALbufferlistitem *BufferList;
    ALdouble dbloff, dblfrac;

    /* Find the first valid Buffer in the Queue */
    BufferList = Source->queue;
    while(BufferList)
    {
        ALsizei i;
        for(i = 0;i < BufferList->num_buffers && !BufferFmt;i++)
            BufferFmt = BufferList->buffers[i];
        if(BufferFmt) break;
        BufferList = ATOMIC_LOAD(&CONST_CAST(ALbufferlistitem*,BufferList)->next,
                                 almemory_order_relaxed);
    }
    if(!BufferFmt)
    {
        Source->OffsetType = AL_NONE;
        Source->Offset = 0.0;
        return AL_FALSE;
    }

    switch(Source->OffsetType)
    {
    case AL_BYTE_OFFSET:
        /* Determine the ByteOffset (and ensure it is block aligned) */
        *offset = (ALuint)Source->Offset;
        if(BufferFmt->OriginalType == UserFmtIMA4)
        {
            ALsizei align = (BufferFmt->OriginalAlign-1)/2 + 4;
            *offset /= align * ChannelsFromFmt(BufferFmt->FmtChannels);
            *offset *= BufferFmt->OriginalAlign;
        }
        else if(BufferFmt->OriginalType == UserFmtMSADPCM)
        {
            ALsizei align = (BufferFmt->OriginalAlign-2)/2 + 7;
            *offset /= align * ChannelsFromFmt(BufferFmt->FmtChannels);
            *offset *= BufferFmt->OriginalAlign;
        }
        else
            *offset /= FrameSizeFromFmt(BufferFmt->FmtChannels, BufferFmt->FmtType);
        *frac = 0;
        break;

    case AL_SAMPLE_OFFSET:
        dblfrac = modf(Source->Offset, &dbloff);
        *offset = (ALuint)mind(dbloff, UINT_MAX);
        *frac = (ALsizei)mind(dblfrac*FRACTIONONE, FRACTIONONE-1.0);
        break;

    case AL_SEC_OFFSET:
        dblfrac = modf(Source->Offset*BufferFmt->Frequency, &dbloff);
        *offset = (ALuint)mind(dbloff, UINT_MAX);
        *frac = (ALsizei)mind(dblfrac*FRACTIONONE, FRACTIONONE-1.0);
        break;
    }
    Source->OffsetType = AL_NONE;
    Source->Offset = 0.0;

    return AL_TRUE;
}


static ALsource *AllocSource(ALCcontext *context)
{
    ALCdevice *device = context->Device;
    SourceSubList *sublist, *subend;
    ALsource *source = NULL;
    ALsizei lidx = 0;
    ALsizei slidx;

    almtx_lock(&context->SourceLock);
    if(context->NumSources >= device->SourcesMax)
    {
        almtx_unlock(&context->SourceLock);
        alSetError(context, AL_OUT_OF_MEMORY, "Exceeding %u source limit", device->SourcesMax);
        return NULL;
    }
    sublist = VECTOR_BEGIN(context->SourceList);
    subend = VECTOR_END(context->SourceList);
    for(;sublist != subend;++sublist)
    {
        if(sublist->FreeMask)
        {
            slidx = CTZ64(sublist->FreeMask);
            source = sublist->Sources + slidx;
            break;
        }
        ++lidx;
    }
    if(UNLIKELY(!source))
    {
        const SourceSubList empty_sublist = { 0, NULL };
        /* Don't allocate so many list entries that the 32-bit ID could
         * overflow...
         */
        if(UNLIKELY(VECTOR_SIZE(context->SourceList) >= 1<<25))
        {
            almtx_unlock(&device->BufferLock);
            alSetError(context, AL_OUT_OF_MEMORY, "Too many sources allocated");
            return NULL;
        }
        lidx = (ALsizei)VECTOR_SIZE(context->SourceList);
        VECTOR_PUSH_BACK(context->SourceList, empty_sublist);
        sublist = &VECTOR_BACK(context->SourceList);
        sublist->FreeMask = ~U64(0);
        sublist->Sources = al_calloc(16, sizeof(ALsource)*64);
        if(UNLIKELY(!sublist->Sources))
        {
            VECTOR_POP_BACK(context->SourceList);
            almtx_unlock(&context->SourceLock);
            alSetError(context, AL_OUT_OF_MEMORY, "Failed to allocate source batch");
            return NULL;
        }

        slidx = 0;
        source = sublist->Sources + slidx;
    }

    memset(source, 0, sizeof(*source));
    InitSourceParams(source, device->NumAuxSends, device->EAXManager.TargetSlots);

    /* Add 1 to avoid source ID 0. */
    source->id = ((lidx<<6) | slidx) + 1;

    context->NumSources++;
    sublist->FreeMask &= ~(U64(1)<<slidx);
    almtx_unlock(&context->SourceLock);

    return source;
}

static void FreeSource(ALCcontext *context, ALsource *source)
{
    ALCdevice *device = context->Device;
    ALuint id = source->id - 1;
    ALsizei lidx = id >> 6;
    ALsizei slidx = id & 0x3f;
    ALvoice *voice;

    ALCdevice_Lock(device);
    if((voice=GetSourceVoice(source, context)) != NULL)
    {
        ATOMIC_STORE(&voice->Source, NULL, almemory_order_relaxed);
        ATOMIC_STORE(&voice->Playing, false, almemory_order_release);
    }
    ALCdevice_Unlock(device);

    DeinitSource(source, device->NumAuxSends);
    memset(source, 0, sizeof(*source));

    VECTOR_ELEM(context->SourceList, lidx).FreeMask |= U64(1) << slidx;
    context->NumSources--;
}

/* ReleaseALSources
 *
 * Destroys all sources in the source map.
 */
ALvoid ReleaseALSources(ALCcontext *context)
{
    ALCdevice *device = context->Device;
    SourceSubList *sublist = VECTOR_BEGIN(context->SourceList);
    SourceSubList *subend = VECTOR_END(context->SourceList);
    size_t leftover = 0;
    for(;sublist != subend;++sublist)
    {
        ALuint64 usemask = ~sublist->FreeMask;
        while(usemask)
        {
            ALsizei idx = CTZ64(usemask);
            ALsource *source = sublist->Sources + idx;

            DeinitSource(source, device->NumAuxSends);
            memset(source, 0, sizeof(*source));
            ++leftover;

            usemask &= ~(U64(1) << idx);
        }
        sublist->FreeMask = ~usemask;
    }
    if(leftover > 0)
        WARN("(%p) Deleted "SZFMT" Source%s\n", device, leftover, (leftover==1)?"":"s");
}
