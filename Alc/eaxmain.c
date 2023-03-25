
#include "config.h"

#define INITGUID
#define INIT_COMMON_DEFS

#include "alMain.h"
#include "alu.h"
#include "alSource.h"
#include "eaxeffects.h"

/* InitEmulatedHardwareEAX function */
inline ALenum InitEmulatedHardwareEAX(ALCcontext *context)
{
    ALCdevice *device;
    GUID       guid_eff[EAX_MAX_FXSLOTS];
    ALenum     enum_eff[EAX_MAX_FXSLOTS];
    ALenum     lock[EAX_MAX_FXSLOTS];
    ALenum     send_id[MAX_ACTIVE_SENDS];
    ALuint     slot_limit;
    ALuint     i;

    device = context->Device;

    guid_eff[SLOT_0] = EAX_REVERB_EFFECT;
    guid_eff[SLOT_1] = EAX_CHORUS_EFFECT;
    guid_eff[SLOT_2] = EAX_NULL_GUID;
    guid_eff[SLOT_3] = EAX_NULL_GUID;

    enum_eff[SLOT_0] = _EAX_REVERB_EFFECT;
    enum_eff[SLOT_1] = _EAX_CHORUS_EFFECT;
    enum_eff[SLOT_2] = _EAX_NULL_GUID;
    enum_eff[SLOT_3] = _EAX_NULL_GUID;

    lock[SLOT_0] = EAXFXSLOT_LOCKED;
    lock[SLOT_1] = EAXFXSLOT_LOCKED;
    lock[SLOT_2] = EAXFXSLOT_UNLOCKED;
    lock[SLOT_3] = EAXFXSLOT_UNLOCKED;

    send_id[0] = SEND_1;
    send_id[1] = SEND_0;
    send_id[2] = SEND_2;
    send_id[3] = SEND_3;

    if(!device->EAXIsActive && device->EAXManager.Target >= EAX40_TARGET)
    {
        device->EAXhw.MultiSlot = AL_TRUE;

        i          = 0;
       slot_limit = EAX_MAX_FXSLOTS;
    }
    else if(!device->EAXIsActive && device->EAXManager.Target < EAX40_TARGET)
    {
        i          = 0;
        slot_limit = EAX_MIN_FXSLOTS;
    }
    else if(device->EAXIsActive && device->EAXManager.Target >= EAX40_TARGET &&
           !device->EAXhw.MultiSlot)
    {
        device->EAXhw.MultiSlot = AL_TRUE;

        i          = EAX_MIN_FXSLOTS;
        slot_limit = EAX_MAX_FXSLOTS;
    }
    else
    {
        i          = EAX_MAX_FXSLOTS;
        slot_limit = EAX_MAX_FXSLOTS;
    }

    if (!device->EAXIsActive)
    {
        alGenFilters(1, &device->EAXhw.Filters.Direct.Idx);
        alFilteri(device->EAXhw.Filters.Direct.Idx, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        alFilterf(device->EAXhw.Filters.Direct.Idx, AL_LOWPASS_GAIN, AL_LOWPASS_DEFAULT_GAIN);
        alFilterf(device->EAXhw.Filters.Direct.Idx, AL_LOWPASS_GAINHF, AL_LOWPASS_DEFAULT_GAINHF);

        device->EAXhw.PrimaryIdx = SLOT_0;
        device->EAXIsActive = AL_TRUE;
    }

    while(i < slot_limit)
    {
        ALuint eff;

        eff = MapEffectEAX(enum_eff[i]);
        alGenAuxiliaryEffectSlots(1, &device->EAXhw.Slots[i].Idx);
        alGenEffects(1, &device->EAXhw.Slots[i].EffIdx);
        CopyDefPropsEffectEAX(&device->EAXEffectProps[i], enum_eff[i]);
        alEffecti(device->EAXhw.Slots[i].EffIdx, AL_EFFECT_TYPE, eff);

        device->EAXhw.Slots[i].EffType = enum_eff[i];

        //On EAX4-5 the default send in the primary is 1
        alGenFilters(1, &device->EAXhw.Filters.Send[send_id[i]].Idx);
        alFilteri(device->EAXhw.Filters.Send[send_id[i]].Idx, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        alFilterf(device->EAXhw.Filters.Send[send_id[i]].Idx, AL_LOWPASS_GAIN, AL_LOWPASS_DEFAULT_GAIN);
        alFilterf(device->EAXhw.Filters.Send[send_id[i]].Idx, AL_LOWPASS_GAINHF, AL_LOWPASS_DEFAULT_GAINHF);

        device->EAXSlotProps[i].guidLoadEffect = guid_eff[i];
        device->EAXSlotProps[i].lLock = lock[i];

        i++;
    }

    return AL_NO_ERROR;
}

/* VersionManagerEAX function */
inline ALenum VersionManagerEAX(EAXCALLPROPS *props, ALenum PropID)
{
    ALCdevice  *device;
    ALenum      eaxerr;
    ALuint      prop;
    ALuint      CntxtParam;
    ALuint      SrcParam;
    ALuint      SlotParam;

    device     = props->context->Device;
    eaxerr     = EAX_OK;
    prop       = props->property;
    CntxtParam = EAXCONTEXT_NONE;
    SrcParam   = EAXSOURCE_NONE;
    SlotParam  = EAXFXSLOT_PARAMETER;

    switch (PropID)
    {
        case _DSPROPSETID_EAX20_ListenerProperties:

            if (device->EAXManager.Target > EAX20_TARGET)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_OPERATION;
            }

            if ((prop & ~DSPROPERTY_EAX20LISTENER_DEFERRED) >= LISTENER20LIST_SIZE)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }

            props->property = listener2to3[prop & ~DSPROPERTY_EAX20LISTENER_DEFERRED] | 
                                          (prop &  DSPROPERTY_EAX20LISTENER_DEFERRED);
            break;

        case _DSPROPSETID_EAX20_BufferProperties:

            if (device->EAXManager.Target > EAX20_TARGET)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_OPERATION;
            }

            if ((prop & ~DSPROPERTY_EAX20BUFFER_DEFERRED) >= BUFFER20LIST_SIZE)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }

            props->property = buffer2to3[prop & ~DSPROPERTY_EAX20BUFFER_DEFERRED] |
                                        (prop &  DSPROPERTY_EAX20BUFFER_DEFERRED);
            break;

        case _DSPROPSETID_EAX30_ListenerProperties:

            if (device->EAXManager.Target > EAX30_TARGET)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_OPERATION;
            }
            device->EAXManager.Target = EAX30_TARGET; //Review

            if ((prop & ~DSPROPERTY_EAXLISTENER_DEFERRED) >= LISTENER30LIST_SIZE)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            break;

        case _DSPROPSETID_EAX30_BufferProperties:

            if (device->EAXManager.Target > EAX30_TARGET)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_OPERATION;
            }
            device->EAXManager.Target = EAX30_TARGET;

            if ((prop & ~DSPROPERTY_EAXBUFFER_DEFERRED) >= BUFFER30LIST_SIZE)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            break;

        case _EAXPROPERTYID_EAX40_FXSlot0:
        case _EAXPROPERTYID_EAX40_FXSlot1:
        case _EAXPROPERTYID_EAX40_FXSlot2:
        case _EAXPROPERTYID_EAX40_FXSlot3:

            SlotParam = prop;
            goto EAX40;

        case _EAXPROPERTYID_EAX40_Context:

            CntxtParam = prop;
            goto EAX40;

        case _EAXPROPERTYID_EAX40_Source:
            SrcParam = prop;
EAX40:
            if (device->EAXManager.Target > EAX40_TARGET)
            {
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_OPERATION;
            }
            else if (device->EAXManager.Target < EAX40_TARGET)
                device->EAXManager.Target = EAX40_TARGET;

            if (SlotParam >= (EAXFXSLOT_OFFSET + SLOT40LIST_SIZE) ||
                CntxtParam >= CNTXT40LIST_SIZE || SrcParam >= SOURCE40LIST_SIZE)
            {
                //Review
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            break;

        case _EAXPROPERTYID_EAX50_FXSlot0:
        case _EAXPROPERTYID_EAX50_FXSlot1:
        case _EAXPROPERTYID_EAX50_FXSlot2:
        case _EAXPROPERTYID_EAX50_FXSlot3:
        case _EAXPROPERTYID_EAX50_Context:
        case _EAXPROPERTYID_EAX50_Source:

            if (device->EAXManager.Target < EAX40_TARGET)
               device->EAXManager.Target = EAX40_TARGET;

            if (device->EAXManager.Target == EAX40_TARGET && PropID != _EAXPROPERTYID_EAX50_Context &&
                prop != EAXCONTEXT_EAXSESSION)
            {
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_OPERATION;
            }
            break;

        default:

            ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
            return AL_INVALID_OPERATION;
    }

    if (!device->EAXIsActive || !device->EAXhw.MultiSlot)
        InitEmulatedHardwareEAX(props->context);

    return AL_NO_ERROR;
}

/* CheckEffectEAX function */
inline ALenum CheckEffectEAX(EAXCALLPROPS *props, GUID *pGUID)
{
    ALenum eaxerr;
    ALenum i;

    if (IsEqualGUID(pGUID, &EAX_NULL_GUID))
    {
        props->guidID = _EAX_NULL_GUID;
        return AL_NO_ERROR;
    }
    else
    {
        for (i=MIN_EFFECT; i<=MAX_EFFECT; i++)
        {
            if (IsEqualGUID(pGUID, &id_list[i]))
            {
                props->guidID = i;
                return AL_NO_ERROR;
            }
        }
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_UNKNOWN_EFFECT);
        return AL_INVALID_OPERATION;
    }
}

/* CheckPrimaryEAX function */
inline ALenum CheckPrimaryEAX(EAXCALLPROPS *props, GUID *pGUID)
{
    ALCdevice *device = props->context->Device;
    ALenum eaxerr;
    ALuint i;

    if (IsEqualGUID(pGUID, &EAX_NULL_GUID))
    {
        props->slotID = SLOT_NULL;
        return AL_NO_ERROR;
    }
    else
    {
        for (i=0; i<EAX_MAX_FXSLOTS; i++)
        {
            if (IsEqualGUID(pGUID, &device->EAXManager.TargetSlots[i]))
            {
                props->slotID = i;
                return AL_NO_ERROR;
            }
        }
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_VALUE);
        return AL_INVALID_VALUE;
    }
}

/* ValidateSlotsEAX function */
inline ALenum ValidateSlotsEAX(ALCcontext *context, EAXSOURCEDATA *data, GUID *pGUID, ALuint size)
{
    EAXSOURCEDATA  tmp;
    ALCdevice     *device;
    ALuint i, j;
    ALenum eaxerr;

    tmp    = *data;
    device = context->Device;

    if (size > EAX_MAX_FXSLOTS)
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
        return AL_INVALID_OPERATION;
    }

    for (i=0; i<size; i++)
    {
        if (IsEqualGUID(&pGUID[i], &EAX_NULL_GUID))
        {
            tmp.ActiveSends[i].SlotIdx = SLOT_NULL;
            tmp.ActiveSends[i].Primary = AL_FALSE;
            continue;
        }

        if (IsEqualGUID(&pGUID[i], &EAX_PrimaryFXSlotID))
        {
            tmp.ActiveSends[i].SlotIdx = device->EAXhw.PrimaryIdx;
            tmp.ActiveSends[i].Primary = AL_TRUE;
            continue;
        }

        for (j=0; j<EAX_MAX_FXSLOTS; j++)
        {
            if (IsEqualGUID(&pGUID[i], &device->EAXManager.TargetSlots[j]))
            {
                tmp.ActiveSends[i].SlotIdx = j;
                tmp.ActiveSends[i].Primary = AL_FALSE;
                break;
            }
        }
        if (j == EAX_MAX_FXSLOTS)
        {
            ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_VALUE);
            return AL_INVALID_VALUE;
        }
    }
    *data = tmp;

    return AL_NO_ERROR;
}

/* CheckSizeEAX function */
inline ALenum CheckSizeEAX(ALCcontext *context, ALuint size, ALuint ref)
{
    ALenum eaxerr;

    if (size < ref)
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
        return AL_INVALID_OPERATION;
    }
    else
        return AL_NO_ERROR;
}

/* CheckContextSizeEAX function */
inline ALenum CheckContextSizeEAX(EAXCALLPROPS *props)
{
    ALenum eaxerr;
    ALenum err;
    ALenum target;

    target = props->context->Device->EAXManager.Target;
    err    = AL_NO_ERROR;

    switch (target)
    {
        case EAX40_TARGET:
            if (props->size < (sizeof(EAXCONTEXTPROPERTIES)-sizeof(ALfloat)))
                err = AL_INVALID_OPERATION;
            break;

        case EAX50_TARGET:
            if (props->size < sizeof(EAXCONTEXTPROPERTIES))
                err = AL_INVALID_OPERATION;
            break;
    }
    if (err)
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);

    return err;
}

/* CheckSlotSizeEAX function */
inline ALenum CheckSlotSizeEAX(EAXCALLPROPS *props)
{
    ALenum eaxerr;
    ALenum err;
    ALenum target;

    target = props->context->Device->EAXManager.Target;
    err    = AL_NO_ERROR;

    switch (target)
    {
        case EAX40_TARGET:
            if (props->size < (sizeof(EAXFXSLOTPROPERTIES) - sizeof(LONG) - sizeof(ALfloat)))
                err = AL_INVALID_OPERATION;
                break;

        case EAX50_TARGET:
            if (props->size < sizeof(EAXFXSLOTPROPERTIES))
                err = AL_INVALID_OPERATION;
                break;
    }
    if (err)
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);

    return err;
}

/* CheckSourceSizeEAX function */
inline ALenum CheckSourceSizeEAX(EAXCALLPROPS *props)
{
    ALenum eaxerr;
    ALenum err;
    ALenum target;

    target = props->context->Device->EAXManager.Target;
    err    = AL_NO_ERROR;

    switch (target)
    {
        case EAX20_TARGET:
            if (props->size < sizeof(EAX20BUFFERPROPERTIES))
                err = AL_INVALID_OPERATION;
            break;

        case EAX30_TARGET:
        case EAX40_TARGET:
            if (props->size < sizeof(EAXBUFFERPROPERTIES))
                err = AL_INVALID_OPERATION;
            break;

        case EAX50_TARGET:
            if (props->size < sizeof(EAXSOURCEPROPERTIES))
                err = AL_INVALID_OPERATION;
            break;
    }
    if (err)
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);

    return err;
}

/*CheckFloatValueEAX function */
static inline ALenum CheckFloatValueEAX(ALCcontext *context, ALfloat value, ALfloat min, ALfloat max)
{
    ALenum eaxerr;

    if(value >= min && value <= max)
        return AL_NO_ERROR;
    else
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_VALUE);
        return AL_INVALID_VALUE;
    }
}

/*CheckIntValueEAX function */
static inline ALenum CheckIntValueEAX(ALCcontext *context, LONG value, LONG min, LONG max)
{
    ALenum eaxerr;

    if (value >= min && value <= max)
        return AL_NO_ERROR;
    else
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_VALUE);
        return AL_INVALID_VALUE;
    }
}

/*CheckUintValueEAX function */
static inline ALenum CheckUintValueEAX(ALCcontext *context, ULONG value, ULONG min, ULONG max)
{
    ALenum eaxerr;

    if (value >= min && value <= max)
        return AL_NO_ERROR;
    else
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_VALUE);
        return AL_INVALID_VALUE;
    }
}

/*CheckSessionValueEAX function */
inline ALenum CheckSessionValueEAX(ALCcontext *context, EAXSESSIONPROPERTIES *pSession)
{
    ALenum eaxerr;

    if (!(pSession->ulEAXVersion >= EAXCONTEXT_MINEAXSESSION &&
          pSession->ulEAXVersion <= EAXCONTEXT_MAXEAXSESSION))
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_EAX_VERSION);
        return AL_INVALID_VALUE;
    }

    if(!(pSession->ulMaxActiveSends >= EAXCONTEXT_MINMAXACTIVESENDS &&
         pSession->ulMaxActiveSends <= EAXCONTEXT_MAXMAXACTIVESENDS))
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
        return AL_INVALID_OPERATION;
    }

    if (pSession->ulEAXVersion == EAX_40 && 
        pSession->ulMaxActiveSends != EAXCONTEXT_DEFAULTMAXACTIVESENDS)
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
        return AL_INVALID_OPERATION;
    }
    return AL_NO_ERROR;
}

/* CheckSendIndexEAX function */
inline ALenum CheckSendIndexEAX(ALCcontext *context, GUID *pGUID, ALuint *index)
{
    GUID      *Slots;
    ALuint     i;
    ALenum     eaxerr;

    Slots   = context->Device->EAXManager.TargetSlots;

    for (i=0; i<EAX_MAX_FXSLOTS; i++)
    {
        if (IsEqualGUID(pGUID, &Slots[i]))
        {
            *index = i;
            return AL_NO_ERROR;
        }
    }
    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&context->EAXLastError, &eaxerr, EAXERR_INVALID_VALUE);
    return AL_INVALID_VALUE;
}

/* CheckContextParamsEAX function */
inline ALenum CheckContextParamsEAX(EAXCALLPROPS *props, EAXCONTEXTPROPERTIES *CxtOut)
{
    ALenum target;
    ALenum err;

    target =  props->context->Device->EAXManager.Target;
    err    = AL_NO_ERROR;

    switch (target)
    {
         case EAX40_TARGET:
            memcpy(CxtOut, props->value, sizeof(EAXCONTEXTPROPERTIES) - sizeof(ALfloat));
            break;

         case EAX50_TARGET:
            memcpy(CxtOut, props->value, sizeof(EAXCONTEXTPROPERTIES));
            break;
    }

    err = CheckPrimaryEAX(props, &CxtOut->guidPrimaryFXSlotID);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    CxtOut->flDistanceFactor,
                    EAXCONTEXT_MINDISTANCEFACTOR,
                    EAXCONTEXT_MINDISTANCEFACTOR);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    CxtOut->flAirAbsorptionHF,
                    EAXCONTEXT_MINAIRABSORPTIONHF,
                    EAXCONTEXT_MAXAIRABSORPTIONHF);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    CxtOut->flHFReference,
                    EAXCONTEXT_MINHFREFERENCE,
                    EAXCONTEXT_MAXHFREFERENCE);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    CxtOut->flMacroFXFactor,
                    EAXCONTEXT_MINMACROFXFACTOR,
                    EAXCONTEXT_MAXMACROFXFACTOR);
    return err;
}

/* CheckSlotParamsEAX function */
inline ALenum CheckSlotParamsEAX(EAXCALLPROPS *props, EAXFXSLOTPROPERTIES *SlotOut)
{
    ALenum target;
    ALenum err;
    ALenum eaxerr;

    target =  props->context->Device->EAXManager.Target;
    err    = AL_NO_ERROR;

    switch (target)
    {
        case EAX40_TARGET:
            memcpy(SlotOut, props->value, sizeof(EAXFXSLOTPROPERTIES) - sizeof(LONG) - sizeof(ALfloat));
            break;

        case EAX50_TARGET:
            memcpy(SlotOut, props->value, sizeof(EAXFXSLOTPROPERTIES));
            break;
    }

    err = CheckEffectEAX(props, &SlotOut->guidLoadEffect);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    SlotOut->lVolume,
                    EAXFXSLOT_MINVOLUME,
                    EAXFXSLOT_MAXVOLUME);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    SlotOut->lLock,
                    EAXFXSLOT_MINLOCK,
                    EAXFXSLOT_MAXLOCK);
    if (err) return err;

    if (target < EAX50_TARGET)
        SlotOut->ulFlags &= EAXFXSLOTFLAGS_ENVIRONMENT;
    else
        SlotOut->ulFlags &= (EAXFXSLOTFLAGS_ENVIRONMENT |
                             EAXFXSLOTFLAGS_UPMIX);

    err = CheckIntValueEAX(props->context,
                    SlotOut->lOcclusion,
                    EAXFXSLOT_MINOCCLUSION,
                    EAXFXSLOT_MAXOCCLUSION);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    SlotOut->flOcclusionLFRatio,
                    EAXFXSLOT_MINOCCLUSIONLFRATIO,
                    EAXFXSLOT_MAXOCCLUSIONLFRATIO);
    if (err) return err;
    //Review
    if (target == EAX40_TARGET && (props->slotID == SLOT_0 || props->slotID == SLOT_1))
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
        err = AL_INVALID_OPERATION;
    }
    return err;
}

/* CheckSourceParamsEAX function */
inline ALenum CheckSourceParamsEAX(EAXCALLPROPS *props, EAXSOURCEPROPERTIES *SrcOut)
{
    ALenum target;
    ALenum err;

    target =  props->context->Device->EAXManager.Target;
    err    = AL_NO_ERROR;

    switch (target)
    {
        case EAX20_TARGET:
        {
            EAX20BUFFERPROPERTIES *SrcIn = props->value;

            SrcOut->lDirect               = SrcIn->lDirect;
            SrcOut->lDirectHF             = SrcIn->lDirectHF;
            SrcOut->lRoom                 = SrcIn->lRoom;
            SrcOut->lRoomHF               = SrcIn->lRoomHF;
            SrcOut->flRoomRolloffFactor   = SrcIn->flRoomRolloffFactor;
            SrcOut->lObstruction          = SrcIn->lObstruction;
            SrcOut->flObstructionLFRatio  = SrcIn->flObstructionLFRatio;
            SrcOut->lOcclusion            = SrcIn->lOcclusion;
            SrcOut->flOcclusionLFRatio    = SrcIn->flOcclusionLFRatio;
            SrcOut->flOcclusionRoomRatio  = SrcIn->flOcclusionRoomRatio;
            SrcOut->lOutsideVolumeHF      = SrcIn->lOutsideVolumeHF;
            SrcOut->flAirAbsorptionFactor = SrcIn->flAirAbsorptionFactor;
            SrcOut->ulFlags               = SrcIn->dwFlags;
            break;
        }
        case EAX30_TARGET:
        case EAX40_TARGET:
            memcpy(SrcOut, props->value, sizeof(EAXBUFFERPROPERTIES));
            break;

        case EAX50_TARGET:
            memcpy(SrcOut, props->value, sizeof(EAXSOURCEPROPERTIES));
            break;
    }

    err = CheckIntValueEAX(props->context,
                SrcOut->lDirect,
                EAXSOURCE_MINDIRECT,
                EAXSOURCE_MAXDIRECT);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                SrcOut->lDirectHF,
                EAXSOURCE_MINDIRECTHF,
                EAXSOURCE_MAXDIRECTHF);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                SrcOut->lRoom, //Affected by Source_2D
                EAXSOURCE_MINROOM,
                EAXSOURCE_MAXROOM);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                SrcOut->lRoomHF,
                EAXSOURCE_MINROOMHF,
                EAXSOURCE_MAXROOMHF);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                SrcOut->lObstruction, //Affected by Source_2D
                EAXSOURCE_MINOBSTRUCTION,
                EAXSOURCE_MAXOBSTRUCTION);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flObstructionLFRatio, //Affected by Source_2D
                EAXSOURCE_MINOBSTRUCTIONLFRATIO,
                EAXSOURCE_MAXOBSTRUCTIONLFRATIO);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                SrcOut->lOcclusion, //Affected by Source_2D
                EAXSOURCE_MINOCCLUSION,
                EAXSOURCE_MAXOCCLUSION);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flOcclusionLFRatio, //Affected by Source_2D
                EAXSOURCE_MINOCCLUSIONLFRATIO,
                EAXSOURCE_MAXOCCLUSIONLFRATIO);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flOcclusionRoomRatio, //Affected by Source_2D
                EAXSOURCE_MINOCCLUSIONROOMRATIO,
                EAXSOURCE_MAXOCCLUSIONROOMRATIO);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flOcclusionDirectRatio, //Affected by Source_2D
                EAXSOURCE_MINOCCLUSIONDIRECTRATIO,
                EAXSOURCE_MAXOCCLUSIONDIRECTRATIO);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                SrcOut->lExclusion, //Affected by Source_2D
                EAXSOURCE_MINEXCLUSION,
                EAXSOURCE_MAXEXCLUSION);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flExclusionLFRatio, //Affected by Source_2D
                EAXSOURCE_MINEXCLUSIONLFRATIO,
                EAXSOURCE_MAXEXCLUSIONLFRATIO);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                SrcOut->lOutsideVolumeHF,
                EAXSOURCE_MINOUTSIDEVOLUMEHF,
                EAXSOURCE_MAXOUTSIDEVOLUMEHF);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flDopplerFactor,
                EAXSOURCE_MINDOPPLERFACTOR,
                EAXSOURCE_MAXDOPPLERFACTOR);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flRolloffFactor,
                EAXSOURCE_MINROLLOFFFACTOR,
                EAXSOURCE_MAXROLLOFFFACTOR);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flRoomRolloffFactor,
                EAXSOURCE_MINROOMROLLOFFFACTOR,
                EAXSOURCE_MAXROOMROLLOFFFACTOR);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                SrcOut->flAirAbsorptionFactor,
                EAXSOURCE_MINAIRABSORPTIONFACTOR,
                EAXSOURCE_MAXAIRABSORPTIONFACTOR);
    if (err) return err;

    if(target < EAX50_TARGET)
        SrcOut->ulFlags &= (EAXSOURCEFLAGS_DIRECTHFAUTO |
                            EAXSOURCEFLAGS_ROOMAUTO     |
                            EAXSOURCEFLAGS_ROOMHFAUTO);
    else
        SrcOut->ulFlags &= (EAXSOURCEFLAGS_DIRECTHFAUTO      |
                            EAXSOURCEFLAGS_ROOMAUTO          |
                            EAXSOURCEFLAGS_ROOMHFAUTO        |
                            EAXSOURCEFLAGS_3DELEVATIONFILTER |
                            EAXSOURCEFLAGS_UPMIX             |
                            EAXSOURCEFLAGS_APPLYSPEAKERLEVELS);

    err = CheckFloatValueEAX(props->context,
                SrcOut->flMacroFXFactor,
                EAXSOURCE_MINMACROFXFACTOR,
                EAXSOURCE_MAXMACROFXFACTOR);

    return err;
}
/* CheckSendParamsEAX function */
inline ALenum CheckSendParamsEAX(ALCcontext *context, EAXSOURCEALLSENDPROPERTIES *pSend, ALuint *index)
{
    ALenum err;
    GUID *pGUID;

    pGUID = &pSend->guidReceivingFXSlotID;

    err = CheckSendIndexEAX(context, pGUID, index);
    if (err) return err;

    err = CheckIntValueEAX(context,
                    pSend->lSend,
                    EAXSOURCE_MINSEND,
                    EAXSOURCE_MAXSEND);
    if (err) return err;

    err = CheckIntValueEAX(context,
                    pSend->lSendHF,
                    EAXSOURCE_MINSENDHF,
                    EAXSOURCE_MAXSENDHF);
    if (err) return err;

    err = CheckIntValueEAX(context,
                    pSend->lOcclusion,
                    EAXSOURCE_MINOCCLUSION,
                    EAXSOURCE_MAXOCCLUSION);
    if (err) return err;

    err = CheckFloatValueEAX(context,
                    pSend->flOcclusionDirectRatio,
                    EAXSOURCE_MINOCCLUSIONDIRECTRATIO,
                    EAXSOURCE_MAXOCCLUSIONDIRECTRATIO);
    if (err) return err;

    err = CheckFloatValueEAX(context,
                    pSend->flOcclusionLFRatio,
                    EAXSOURCE_MINOCCLUSIONLFRATIO,
                    EAXSOURCE_MAXOCCLUSIONLFRATIO);
    if (err) return err;

    err = CheckFloatValueEAX(context,
                    pSend->flOcclusionRoomRatio,
                    EAXSOURCE_MINOCCLUSIONROOMRATIO,
                    EAXSOURCE_MAXOCCLUSIONROOMRATIO);
    if (err) return err;

    err = CheckIntValueEAX(context,
                    pSend->lExclusion,
                    EAXSOURCE_MINEXCLUSION,
                    EAXSOURCE_MAXEXCLUSION);
    if (err) return err;

    err = CheckFloatValueEAX(context,
                    pSend->flExclusionLFRatio,
                    EAXSOURCE_MINEXCLUSIONLFRATIO,
                    EAXSOURCE_MAXEXCLUSIONLFRATIO);
    return err;
}

/* CopyContextParamsEAX function */
inline ALvoid CopyContextParamsEAX(EAXCALLPROPS *props, EAXCONTEXTPROPERTIES *CxtIn)
{
    ALenum target = props->context->Device->EAXManager.Target;

    switch (target)
    {
        case EAX40_TARGET:
            memcpy(props->value, CxtIn, sizeof(EAXCONTEXTPROPERTIES) - sizeof(ALfloat));
            break;

        case EAX50_TARGET:
            memcpy(props->value, CxtIn, sizeof(EAXCONTEXTPROPERTIES));
            break;
    }
}

/* CopySlotParamsEAX function */
inline ALvoid CopySlotParamsEAX(EAXCALLPROPS *props, EAXFXSLOTPROPERTIES *SlotIn)
{
    ALenum target = props->context->Device->EAXManager.Target;
    EAXFXSLOTPROPERTIES *value = (EAXFXSLOTPROPERTIES *)props->value;

    switch (target)
    {
        case EAX40_TARGET:
            memcpy(props->value, SlotIn, sizeof(EAXFXSLOTPROPERTIES) - sizeof(LONG) - sizeof(ALfloat));
            value->ulFlags &= EAXFXSLOTFLAGS_ENVIRONMENT;
            break;

        case EAX50_TARGET:
            memcpy(props->value, SlotIn, sizeof(EAXFXSLOTPROPERTIES));
            value->ulFlags &= (EAXFXSLOTFLAGS_ENVIRONMENT | EAXFXSLOTFLAGS_UPMIX);
            break;
    }
}

/* CopySourceParamsEAX function */
inline ALvoid CopySourceParamsEAX(EAXCALLPROPS *props, EAXSOURCEPROPERTIES *SrcIn)
{
    ALenum target = props->context->Device->EAXManager.Target;

    switch (target)
    {
        case EAX20_TARGET:
        {
            EAX20BUFFERPROPERTIES *SrcOut = props->value;

            SrcOut->lDirect               = SrcIn->lDirect;
            SrcOut->lDirectHF             = SrcIn->lDirectHF;
            SrcOut->lRoom                 = SrcIn->lRoom;
            SrcOut->lRoomHF               = SrcIn->lRoomHF;
            SrcOut->flRoomRolloffFactor   = SrcIn->flRoomRolloffFactor;
            SrcOut->lObstruction          = SrcIn->lObstruction;
            SrcOut->flObstructionLFRatio  = SrcIn->flObstructionLFRatio;
            SrcOut->lOcclusion            = SrcIn->lOcclusion;
            SrcOut->flOcclusionLFRatio    = SrcIn->flOcclusionLFRatio;
            SrcOut->flOcclusionRoomRatio  = SrcIn->flOcclusionRoomRatio;
            SrcOut->lOutsideVolumeHF      = SrcIn->lOutsideVolumeHF;
            SrcOut->flAirAbsorptionFactor = SrcIn->flAirAbsorptionFactor;
            SrcOut->dwFlags               = SrcIn->ulFlags & EAX20BUFFER_DEFAULTFLAGS;
            break;
        }
        case EAX30_TARGET:
        case EAX40_TARGET:
            memcpy(props->value, SrcIn, sizeof(EAXBUFFERPROPERTIES));
            break;

        case EAX50_TARGET:
            memcpy(props->value, SrcIn, sizeof(EAXSOURCEPROPERTIES));
            break;
    }
}

/* UpdateContextEAX function. Enable the FX Slot that contains the environment listener */
ALenum UpdateContextEAX(ALCcontext *context)
{
    context->EAXIsDefer = AL_FALSE;

    alListenerf(AL_METERS_PER_UNIT, context->EAXCxtProps.flDistanceFactor);
    UpdateAllSourcesEAX(context);

    return AL_NO_ERROR;
}
/* UpdateSlotEAX function. */
ALenum UpdateSlotEAX(EAXCALLPROPS *props)
{
    ALCdevice *device;
    ALint      id;
    ALfloat    Volume;
    ALint      EnvFlag;

    device   = props->context->Device;
    id       = props->slotID;
    Volume   = (ALfloat)device->EAXSlotProps[id].lVolume;
    EnvFlag  = device->EAXSlotProps[id].ulFlags & EAXFXSLOTFLAGS_ENVIRONMENT;

    props->context->EAXIsDefer = AL_FALSE;

    alAuxiliaryEffectSlotf(device->EAXhw.Slots[id].Idx, AL_EFFECTSLOT_GAIN, mB_to_gain(Volume));
    alAuxiliaryEffectSloti(device->EAXhw.Slots[id].Idx, AL_EFFECTSLOT_AUXILIARY_SEND_AUTO, EnvFlag);
    ApplyEffectParamsEAX(props);

    UpdateAllSourcesEAX(props->context);

    return AL_NO_ERROR;
}

/* LookupSourceEAX function */
static inline ALsource *LookupSourceEAX(ALCcontext *context, ALuint id)
{
    SourceSubList *sublist;
    ALuint lidx = (id-1) >> 6;
    ALsizei slidx = (id-1) & 0x3F;

    if(UNLIKELY(lidx >= VECTOR_SIZE(context->SourceList)))
        return NULL;
    sublist = &VECTOR_ELEM(context->SourceList, lidx);
    if(UNLIKELY(sublist->FreeMask & (U64(1)<<slidx)))
        return NULL;
    return sublist->Sources + slidx;
}

/*ContextEAX function. Process the EAXSet/EAXSet Context calls*/

inline ALenum ContextEAX(EAXCALLPROPS *props)
{
    LPEAXPROPS val   = {props->value};
    ALenum     err;
    ALenum     eaxerr;
    ALCdevice *device;

    err    = AL_NO_ERROR;
    device = props->context->Device;

    /*Process the Property ID Deferred flag*/
    if (props->property & EAXCONTEXT_PARAMETER_DEFER)
        props->context->EAXIsDefer = AL_TRUE;

    switch (props->property & ~EAXCONTEXT_PARAMETER_DEFER)
    {
        case EAXCONTEXT_NONE:
            break;

        case EAXCONTEXT_ALLPARAMETERS:

            if (err = CheckContextSizeEAX(props))
                return err;
            else if (props->isSet)
            {
                EAXCONTEXTPROPERTIES tmp = props->context->EAXCxtProps;

                if(err = CheckContextParamsEAX(props, &tmp))
                    return err;

                device->EAXhw.PrimaryIdx    = props->slotID;
                props->context->EAXCxtProps = tmp;
            }
            else
                CopyContextParamsEAX(props, &props->context->EAXCxtProps);
            break;

        case EAXCONTEXT_PRIMARYFXSLOTID/*2*/:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(GUID)))
                return err;
            else if (props->isSet)
            {
                err = CheckPrimaryEAX(props, val.pGUID);
                if(err) return err;

                device->EAXhw.PrimaryIdx = props->slotID;
                props->context->EAXCxtProps.guidPrimaryFXSlotID = *val.pGUID;
            }
            else
                *val.pGUID = props->context->EAXCxtProps.guidPrimaryFXSlotID;
            break;

        case EAXCONTEXT_DISTANCEFACTOR:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXCONTEXT_MINDISTANCEFACTOR,
                                EAXCONTEXT_MAXDISTANCEFACTOR);
                if (err) return err;

                props->context->EAXCxtProps.flDistanceFactor = *val.pFloat;
            }
            else
                *val.pFloat = props->context->EAXCxtProps.flDistanceFactor;
            break;

        case EAXCONTEXT_AIRABSORPTIONHF:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXCONTEXT_MINAIRABSORPTIONHF,
                                EAXCONTEXT_MAXAIRABSORPTIONHF);
                if (err) return err;

                props->context->EAXCxtProps.flAirAbsorptionHF = *val.pFloat;
            }
            else
                *val.pFloat = props->context->EAXCxtProps.flAirAbsorptionHF;
            break;

        case EAXCONTEXT_HFREFERENCE:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXCONTEXT_MINHFREFERENCE,
                                EAXCONTEXT_MAXHFREFERENCE);
                if (err) return err;

                props->context->EAXCxtProps.flHFReference = *val.pFloat;
            }
            else
                *val.pFloat = props->context->EAXCxtProps.flHFReference;
            break;

        case EAXCONTEXT_LASTERROR:

            if (props->isSet)
            {
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                err = AL_INVALID_OPERATION;
            }
            else if (props->size < sizeof(LONG))
            {
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                err = AL_INVALID_OPERATION;
            }
            else 
                *val.pLong = ATOMIC_EXCHANGE_SEQ(&props->context->LastError, EAX_OK);
            return err;

        case EAXCONTEXT_SPEAKERCONFIG:
            UNIMPLEMENTED;
            return err;

        case EAXCONTEXT_EAXSESSION/*8*/:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXSESSIONPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                ALCint    attrList[3];
                ALuint    Version, Sends, Target;

                if(err = CheckSessionValueEAX(props->context, val.pSession))
                    return err;

                Version = val.pSession->ulEAXVersion;
                Sends   = val.pSession->ulMaxActiveSends;
                Target  = device->EAXManager.Target;

                attrList[0] = ALC_MAX_AUXILIARY_SENDS;
                attrList[1] = Sends;
                attrList[2] = '\0';

                if (Version == EAX_40 && device->EAXManager.Target == EAX50_TARGET)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                    return AL_INVALID_OPERATION;
                }

                if (Version == EAX_50 && device->EAXManager.Target < EAX50_TARGET)
                {
                    ALCint Idx = device->EAXhw.PrimaryIdx;

                    device->EAXManager.Target = EAX50_TARGET;
                    device->EAXManager.TargetSlots = &id_list[_EAXPROPERTYID_EAX50_FXSlot0];
                    if (Idx != SLOT_NULL)
                        props->context->EAXCxtProps.guidPrimaryFXSlotID = device->EAXManager.TargetSlots[Idx];
                }

                alcGetIntegerv(props->context->Device, ALC_MAX_AUXILIARY_SENDS, 1, &Sends);
                if (Sends != attrList[1] || Target != device->EAXManager.Target)
                    alcResetDeviceSOFT(props->context->Device, attrList);

                device->EAXSession.ulEAXVersion     = Version;
                device->EAXSession.ulMaxActiveSends = attrList[1];
            }
            else
                *val.pSession = device->EAXSession;
            break;

        case EAXCONTEXT_MACROFXFACTOR:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXCONTEXT_MINMACROFXFACTOR,
                                EAXCONTEXT_MAXMACROFXFACTOR);
                if (err) return err;

                props->context->EAXCxtProps.flMacroFXFactor = *val.pFloat;
            }
            else
                *val.pFloat = props->context->EAXCxtProps.flMacroFXFactor;
            break;
            UNIMPLEMENTED;

        default:

            ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
            return AL_INVALID_ENUM;
    }

    if (props->isSet && !props->context->EAXIsDefer)
        UpdateContextEAX(props->context);

    return err;
}

/*SlotEAX function. Process the EAXSet/EAXGet slot calls*/
inline ALenum SlotEAX(EAXCALLPROPS *props)
{
    LPEAXPROPS val = {props->value};
    ALenum     eaxerr;
    ALenum     err;
    ALCdevice *device;
    ALuint     SlotID;

    device = props->context->Device;
    SlotID = props->slotID;
    err    = AL_NO_ERROR;

    switch (props->property)
    {
/*0x10000*/
        case EAXFXSLOT_NONE:
            break;
/*0x10001*/
        case EAXFXSLOT_ALLPARAMETERS:

            if (err = CheckSlotSizeEAX(props))
                return err;
            else if (props->isSet)
            {
                EAXFXSLOTPROPERTIES tmp = device->EAXSlotProps[SlotID];
                ALenum Effect;

                if (err = CheckSlotParamsEAX(props, &tmp))
                    return err;

                Effect = MapEffectEAX(props->guidID);
                CopyDefPropsEffectEAX(&device->EAXEffectProps[SlotID], props->guidID);
                alEffecti(device->EAXhw.Slots[SlotID].EffIdx, AL_EFFECT_TYPE, Effect);

                tmp.ulFlags |= device->EAXSlotProps[SlotID].ulFlags;
                device->EAXhw.Slots[SlotID].EffType = props->guidID;
                device->EAXSlotProps[SlotID]        = tmp;
            }
            else
                CopySlotParamsEAX(props, &device->EAXSlotProps[SlotID]);
            break;
/*0x10002*/
        case EAXFXSLOT_LOADEFFECT:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(GUID)))
                return err;
            else if (props->isSet)
            {
                ALenum Effect;

                if (device->EAXManager.Target == EAX40_TARGET &&
                   (SlotID == SLOT_0 || SlotID == SLOT_1))
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                    return AL_INVALID_OPERATION;
                }
                if (err = CheckEffectEAX(props, val.pGUID))
                    return err;

                Effect = MapEffectEAX(props->guidID);
                CopyDefPropsEffectEAX(&device->EAXEffectProps[SlotID], props->guidID);
                alEffecti(device->EAXhw.Slots[SlotID].EffIdx, AL_EFFECT_TYPE, Effect);

                device->EAXhw.Slots[SlotID].EffType         = props->guidID;
                device->EAXSlotProps[SlotID].guidLoadEffect = *val.pGUID;
            }
            else
                *val.pGUID = device->EAXSlotProps[SlotID].guidLoadEffect;
           break;
/*0x10003*/
        case EAXFXSLOT_VOLUME:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXFXSLOT_MINVOLUME,
                                EAXFXSLOT_MAXVOLUME);
                if (err) return err;

                device->EAXSlotProps[SlotID].lVolume = *val.pLong;
            }
            else
                *val.pLong = device->EAXSlotProps[SlotID].lVolume;
            break;
/*0x10004*/
        case EAXFXSLOT_LOCK:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXFXSLOT_MINLOCK,
                                EAXFXSLOT_MAXLOCK);
                if (err) return err;

                if (device->EAXManager.Target == EAX40_TARGET &&
                    (SlotID == SLOT_0 || SlotID == SLOT_1) &&
                    *val.pLong == EAXFXSLOT_UNLOCKED)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                    return AL_INVALID_OPERATION;
                }
                device->EAXSlotProps[SlotID].lLock = *val.pLong;
            }
            else
                *val.pLong = device->EAXSlotProps[SlotID].lLock;
            break;
/*0x10005*/
        case EAXFXSLOT_FLAGS:
        {
            ALuint eaxmask;

            if (device->EAXManager.Target < EAX50_TARGET)
                eaxmask = EAXFXSLOTFLAGS_ENVIRONMENT;
            else
                eaxmask = (EAXFXSLOTFLAGS_ENVIRONMENT | EAXFXSLOTFLAGS_UPMIX);

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet) //Implementar
            {
                device->EAXSlotProps[SlotID].ulFlags = *val.pUlong & eaxmask;
            }
            else
                *val.pUlong = device->EAXSlotProps[SlotID].ulFlags & eaxmask;

        }
            break;
/*0x10006*/
        case EAXFXSLOT_OCCLUSION:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXFXSLOT_MINOCCLUSION,
                                EAXFXSLOT_MAXOCCLUSION);
                if (err) return err;

                device->EAXSlotProps[SlotID].lOcclusion = *val.pLong;
            }
            else
                *val.pLong = device->EAXSlotProps[SlotID].lOcclusion;
            break;
/*0x10007*/
        case EAXFXSLOT_OCCLUSIONLFRATIO:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                 *val.pFloat,
                                 EAXFXSLOT_MINOCCLUSIONLFRATIO,
                                 EAXFXSLOT_MAXOCCLUSIONLFRATIO);
                if (err) return err;

                device->EAXSlotProps[SlotID].flOcclusionLFRatio = *val.pFloat;
            }
            else
                *val.pFloat = device->EAXSlotProps[SlotID].flOcclusionLFRatio;
            break;

        default:
        {
//*rang=[0,0x40]
            if ((props->property & ~EAXEFFECTS_DEFERRED) <= MAX_EAXFXSLOT_PARAMETER)
            {
                /*Process the Property ID Deferred flag*/
                if (props->property & EAXEFFECTS_DEFERRED)
                    props->context->EAXIsDefer = AL_TRUE;

                if(err = EffectEAX(props))
                    return err;

                if (props->isSet && !props->context->EAXIsDefer)
                    UpdateSlotEAX(props);
                return err;
            }
            else 
            {
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
        }
    }

    if (props->isSet)
        UpdateSlotEAX(props);
    return err;
}

/*SourceEAX function. Process the EAXSet/EAXGet source calls*/
inline ALenum SourceEAX(EAXCALLPROPS *props)
{
    LPEAXPROPS  val = {props->value};
    ALCdevice   *device;
    ALsource    *src;
    ALenum      type;
    ALuint      i, num;
    ALuint      Index[EAX_MAX_FXSLOTS];
    ALenum      err;
    ALenum      eaxerr;

    err    =  AL_NO_ERROR;
    device = props->context->Device;

    almtx_lock(&props->context->SourceLock);
    /*Check whether the source exists*/
    if ((src = LookupSourceEAX(props->context, props->source)) == NULL)
    {
        err = AL_INVALID_NAME;
        goto EAX_SOURCE_ERROR;
    }
    /*Process the Source type*/
    if (src->SourceChannels == 0)
        type = SOURCE_NONE;
    else if (src->SourceChannels == 1)
        type = SOURCE_3D;
    else
        type = SOURCE_2D;

    /*Process the Property ID Deferred flag*/
    if (props->property & EAXSOURCE_PARAMETER_DEFERRED)
        props->context->EAXIsDefer = AL_TRUE;

    switch (props->property & ~EAXSOURCE_PARAMETER_DEFERRED)
    {
        case EAXSOURCE_NONE:
            break;

        case EAXSOURCE_ALLPARAMETERS:

            if (err = CheckSourceSizeEAX(props))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                EAXSOURCEPROPERTIES tmp = src->EAXSrcProps;

                if (err = CheckSourceParamsEAX(props, &tmp))
                    goto EAX_SOURCE_ERROR;

                tmp.ulFlags |= src->EAXSrcProps.ulFlags;
                src->EAXSrcProps = tmp;
            }
            else
                CopySourceParamsEAX(props, &src->EAXSrcProps);
            break;

        case EAXSOURCE_OBSTRUCTIONPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXOBSTRUCTIONPROPERTIES)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINOBSTRUCTION,
                                EAXSOURCE_MAXOBSTRUCTION);
                if (err) goto EAX_SOURCE_ERROR;

                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINOBSTRUCTIONLFRATIO,
                                EAXSOURCE_MAXOBSTRUCTIONLFRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.lObstruction         = val.pObstuction->lObstruction;
                src->EAXSrcProps.flObstructionLFRatio = val.pObstuction->flObstructionLFRatio;
            }
            else
            {
                val.pObstuction->lObstruction         = src->EAXSrcProps.lObstruction;
                val.pObstuction->flObstructionLFRatio = src->EAXSrcProps.flObstructionLFRatio;
            }
            break;

        case EAXSOURCE_OCCLUSIONPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXOCCLUSIONPROPERTIES)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINOCCLUSION,
                                EAXSOURCE_MAXOCCLUSION);
                if (err) goto EAX_SOURCE_ERROR;

                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINOCCLUSIONLFRATIO,
                                EAXSOURCE_MAXOCCLUSIONLFRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINOCCLUSIONROOMRATIO,
                                EAXSOURCE_MAXOCCLUSIONROOMRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINOCCLUSIONDIRECTRATIO,
                                EAXSOURCE_MAXOCCLUSIONDIRECTRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.lOcclusion             = val.pOcclusion->lOcclusion;
                src->EAXSrcProps.flObstructionLFRatio   = val.pOcclusion->flOcclusionLFRatio;
                src->EAXSrcProps.flOcclusionRoomRatio   = val.pOcclusion->flOcclusionRoomRatio;
                src->EAXSrcProps.flOcclusionDirectRatio = val.pOcclusion->flOcclusionDirectRatio;
            }
            else
            {
                val.pOcclusion->lOcclusion             = src->EAXSrcProps.lOcclusion;
                val.pOcclusion->flOcclusionLFRatio     = src->EAXSrcProps.flObstructionLFRatio;
                val.pOcclusion->flOcclusionRoomRatio   = src->EAXSrcProps.flOcclusionRoomRatio;
                val.pOcclusion->flOcclusionDirectRatio = src->EAXSrcProps.flOcclusionDirectRatio;
            }
            break;

        case EAXSOURCE_EXCLUSIONPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXEXCLUSIONPROPERTIES)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINEXCLUSION,
                                EAXSOURCE_MAXEXCLUSION);
                if (err) goto EAX_SOURCE_ERROR;

                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINEXCLUSIONLFRATIO,
                                EAXSOURCE_MAXEXCLUSIONLFRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.lExclusion         = val.pExclusion->lExclusion;
                src->EAXSrcProps.flExclusionLFRatio = val.pExclusion->flExclusionLFRatio;
            }
            else
            {
                val.pExclusion->lExclusion         = src->EAXSrcProps.lExclusion;
                val.pExclusion->flExclusionLFRatio = src->EAXSrcProps.flExclusionLFRatio;
            }
            break;

        case EAXSOURCE_DIRECT:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINDIRECT,
                                EAXSOURCE_MAXDIRECT);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.lDirect = *val.pLong;
            }
            else
                *val.pLong = src->EAXSrcProps.lDirect;
            break;

        case EAXSOURCE_DIRECTHF:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINDIRECTHF,
                                EAXSOURCE_MAXDIRECTHF);
                if (err) goto EAX_SOURCE_ERROR;

                    src->EAXSrcProps.lDirectHF = *val.pLong;
            }
            else
                *val.pLong = src->EAXSrcProps.lDirectHF;
            break;

        case EAXSOURCE_ROOM:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINROOM,
                                EAXSOURCE_MAXROOM);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.lRoom = *val.pLong;
            }
            else
                *val.pLong = src->EAXSrcProps.lRoom;
            break;

        case EAXSOURCE_ROOMHF:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINROOMHF,
                                EAXSOURCE_MAXROOMHF);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.lRoomHF = *val.pLong;
            }
            else
                *val.pLong = src->EAXSrcProps.lRoomHF;
            break;

        case EAXSOURCE_OBSTRUCTION:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINOBSTRUCTION,
                                EAXSOURCE_MAXOBSTRUCTION);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.lObstruction = *val.pLong;
            }
            else
                *val.pLong = src->EAXSrcProps.lObstruction;
            break;

        case EAXSOURCE_OBSTRUCTIONLFRATIO:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINOBSTRUCTIONLFRATIO,
                                EAXSOURCE_MAXOBSTRUCTIONLFRATIO);
                goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.flObstructionLFRatio = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flObstructionLFRatio;
            break;

        case EAXSOURCE_OCCLUSION:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINOCCLUSION,
                                EAXSOURCE_MAXOCCLUSION);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.lOcclusion = *val.pLong;
            }
            else
                *val.pLong = src->EAXSrcProps.lOcclusion;
            break;

        case EAXSOURCE_OCCLUSIONLFRATIO:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINOCCLUSIONLFRATIO,
                                EAXSOURCE_MAXOCCLUSIONLFRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.flOcclusionLFRatio = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flOcclusionLFRatio;
            break;

        case EAXSOURCE_OCCLUSIONROOMRATIO:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINOCCLUSIONROOMRATIO,
                                EAXSOURCE_MAXOCCLUSIONROOMRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.flOcclusionRoomRatio = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flOcclusionRoomRatio;
            break;

        case EAXSOURCE_OCCLUSIONDIRECTRATIO:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINOCCLUSIONDIRECTRATIO,
                                EAXSOURCE_MAXOCCLUSIONDIRECTRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.flOcclusionDirectRatio = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flOcclusionDirectRatio;
            break;

        case EAXSOURCE_EXCLUSION:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINEXCLUSION,
                                EAXSOURCE_MAXEXCLUSION);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.lExclusion = *val.pLong;
            }
            else
                *val.pLong = src->EAXSrcProps.lExclusion;
            break;

        case EAXSOURCE_EXCLUSIONLFRATIO:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINEXCLUSIONLFRATIO,
                                EAXSOURCE_MAXEXCLUSIONLFRATIO);
                if (err) goto EAX_SOURCE_ERROR;

                if (type == SOURCE_2D)
                {
                    ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INCOMPATIBLE_SOURCE_TYPE);
                    err = AL_INVALID_OPERATION;
                    goto EAX_SOURCE_ERROR;
                }
                src->EAXSrcProps.flExclusionLFRatio = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flExclusionLFRatio;
            break;

        case EAXSOURCE_OUTSIDEVOLUMEHF:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXSOURCE_MINOUTSIDEVOLUMEHF,
                                EAXSOURCE_MAXOUTSIDEVOLUMEHF);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.lOutsideVolumeHF = *val.pLong;
            }
            else
                *val.pLong = src->EAXSrcProps.lOutsideVolumeHF;
            break;

        case EAXSOURCE_DOPPLERFACTOR:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINDOPPLERFACTOR,
                                EAXSOURCE_MAXDOPPLERFACTOR);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.flDopplerFactor = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flDopplerFactor;
            break;

        case EAXSOURCE_ROLLOFFFACTOR:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINROLLOFFFACTOR,
                                EAXSOURCE_MAXROLLOFFFACTOR);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.flRolloffFactor = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flRolloffFactor;
            break;

        case EAXSOURCE_ROOMROLLOFFFACTOR:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINROOMROLLOFFFACTOR,
                                EAXSOURCE_MAXROOMROLLOFFFACTOR);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.flRoomRolloffFactor = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flRoomRolloffFactor;
            break;

        case EAXSOURCE_AIRABSORPTIONFACTOR:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXSOURCE_MINAIRABSORPTIONFACTOR,
                                EAXSOURCE_MAXAIRABSORPTIONFACTOR);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.flAirAbsorptionFactor = *val.pFloat;
            }
            else
                *val.pFloat = src->EAXSrcProps.flAirAbsorptionFactor;
            break;

        case EAXSOURCE_FLAGS:
        {
            ALuint eaxmask;

            if (device->EAXManager.Target < EAX50_TARGET)
                eaxmask = (EAXSOURCEFLAGS_DIRECTHFAUTO |
                           EAXSOURCEFLAGS_ROOMAUTO     |
                           EAXSOURCEFLAGS_ROOMHFAUTO);
            else
                eaxmask = (EAXSOURCEFLAGS_DIRECTHFAUTO      |
                           EAXSOURCEFLAGS_ROOMAUTO          |
                           EAXSOURCEFLAGS_ROOMHFAUTO        |
                           EAXSOURCEFLAGS_3DELEVATIONFILTER |
                           EAXSOURCEFLAGS_UPMIX             |
                           EAXSOURCEFLAGS_APPLYSPEAKERLEVELS);

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                src->EAXSrcProps.ulFlags = *val.pUlong & eaxmask;
            }
            else
                *val.pUlong = src->EAXSrcProps.ulFlags & eaxmask;
        }
            break;

        case EAXSOURCE_SENDPARAMETERS:

            num = props->size / sizeof(EAXSOURCESENDPROPERTIES);

            if (err = CheckSizeEAX(props->context, num, MIN_SIZE))
                goto EAX_SOURCE_ERROR;
            clampu(num, 1, EAX_MAX_FXSLOTS);

            if (props->isSet)
            {
                for (i=0; i<num; i++)
                {
                    GUID *pGUID = &val.pSrcsend[i].guidReceivingFXSlotID;

                    err = CheckSendIndexEAX(props->context, pGUID, &Index[i]);
                    if (err) goto EAX_SOURCE_ERROR;

                    err = CheckIntValueEAX(props->context,
                                    val.pSrcsend[i].lSend,
                                    EAXSOURCE_MINSEND,
                                    EAXSOURCE_MAXSEND);
                    if (err) goto EAX_SOURCE_ERROR;

                    err = CheckIntValueEAX(props->context,
                                    val.pSrcsend[i].lSendHF,
                                    EAXSOURCE_MINSENDHF,
                                    EAXSOURCE_MAXSENDHF);
                    if (err) goto EAX_SOURCE_ERROR;
                }

                for (i=0; i<num; i++)
                {
                    ALuint j = Index[i];

                    src->EAXSendProps[j].lSend = val.pSrcsend[i].lSend;
                    src->EAXSendProps[j].lSendHF = val.pSrcsend[i].lSendHF;
                }
            }
            else
            {
                for (i=0; i<num; i++)
                {
                    val.pSrcsend[i].lSend   = src->EAXSendProps[i].lSend;
                    val.pSrcsend[i].lSendHF = src->EAXSendProps[i].lSendHF;
                }
            }
            break;

        case EAXSOURCE_ALLSENDPARAMETERS:

            num = props->size / sizeof(EAXSOURCEALLSENDPROPERTIES);

            if (err = CheckSizeEAX(props->context, num, MIN_SIZE))
                goto EAX_SOURCE_ERROR;
            clampu(num, 1, EAX_MAX_FXSLOTS);

            if (props->isSet)
            {
                for (i=0; i<num; i++)
                {
                    err = CheckSendParamsEAX(props->context, 
                                &val.pSrcallsend[i],
                                &Index[i]);
                   if (err) goto EAX_SOURCE_ERROR;
                }

                for (i=0; i<num; i++)
                {
                    ALuint j = Index[i];

                    src->EAXSendProps[j] = val.pSrcallsend[i];
                }
            }
            else
            {
                for (i=0; i<num; i++)
                    val.pSrcallsend[i] = src->EAXSendProps[i];
            }
            break;

        case EAXSOURCE_OCCLUSIONSENDPARAMETERS:

            num = props->size / sizeof(EAXSOURCEOCCLUSIONSENDPROPERTIES);

            if (err = CheckSizeEAX(props->context, num, MIN_SIZE))
                goto EAX_SOURCE_ERROR;
            clampu(num, 1, EAX_MAX_FXSLOTS);

            if (props->isSet)
            {
                for (i=0; i<num; i++)
                {
                    GUID *pGUID = &val.pSrcocclsend[i].guidReceivingFXSlotID;

                    err = CheckSendIndexEAX(props->context, pGUID, &Index[i]);
                    if (err) goto EAX_SOURCE_ERROR;

                    err = CheckIntValueEAX(props->context,
                                    val.pSrcocclsend[i].lOcclusion,
                                    EAXSOURCE_MINOCCLUSION,
                                    EAXSOURCE_MAXOCCLUSION);
                    if (err) goto EAX_SOURCE_ERROR;

                    err = CheckFloatValueEAX(props->context,
                                    val.pSrcocclsend[i].flOcclusionDirectRatio,
                                    EAXSOURCE_MINOCCLUSIONDIRECTRATIO,
                                    EAXSOURCE_MAXOCCLUSIONDIRECTRATIO);
                    if (err) goto EAX_SOURCE_ERROR;

                    err = CheckFloatValueEAX(props->context,
                                    val.pSrcocclsend[i].flOcclusionLFRatio,
                                    EAXSOURCE_MINOCCLUSIONLFRATIO,
                                    EAXSOURCE_MAXOCCLUSIONLFRATIO);
                    if (err) goto EAX_SOURCE_ERROR;

                    err = CheckFloatValueEAX(props->context,
                                    val.pSrcocclsend[i].flOcclusionRoomRatio,
                                    EAXSOURCE_MINOCCLUSIONROOMRATIO,
                                    EAXSOURCE_MAXOCCLUSIONROOMRATIO);
                    if (err) goto EAX_SOURCE_ERROR;
                }

                for (i=0; i<num; i++)
                {
                    ALuint j = Index[i];

                    src->EAXSendProps[j].lOcclusion             = val.pSrcocclsend[i].lOcclusion;
                    src->EAXSendProps[j].flOcclusionDirectRatio = val.pSrcocclsend[i].flOcclusionDirectRatio;
                    src->EAXSendProps[j].flOcclusionLFRatio     = val.pSrcocclsend[i].flOcclusionLFRatio;
                    src->EAXSendProps[j].flOcclusionRoomRatio   = val.pSrcocclsend[i].flOcclusionRoomRatio;
                }
            }
            else
            {
                for (i=0; i<num; i++)
                {
                    val.pSrcocclsend[i].lOcclusion             = src->EAXSendProps[i].lOcclusion;
                    val.pSrcocclsend[i].flOcclusionDirectRatio = src->EAXSendProps[i].flOcclusionDirectRatio;
                    val.pSrcocclsend[i].flOcclusionLFRatio     = src->EAXSendProps[i].flOcclusionLFRatio;
                    val.pSrcocclsend[i].flOcclusionRoomRatio   = src->EAXSendProps[i].flOcclusionRoomRatio;
                }
            }
            break;

        case EAXSOURCE_EXCLUSIONSENDPARAMETERS:

            num = props->size / sizeof(EAXSOURCEEXCLUSIONSENDPROPERTIES);

            if (err = CheckSizeEAX(props->context, num, MIN_SIZE))
                goto EAX_SOURCE_ERROR;
            clampu(num, 1, EAX_MAX_FXSLOTS);

            if (props->isSet)
            {
                for (i=0; i<num; i++)
                {
                    GUID *pGUID = &val.pSrcexclsend[i].guidReceivingFXSlotID;

                    err = CheckSendIndexEAX(props->context, pGUID, &Index[i]);
                    if (err) goto EAX_SOURCE_ERROR;

                    err = CheckIntValueEAX(props->context,
                                    val.pSrcexclsend[i].lExclusion,
                                    EAXSOURCE_MINEXCLUSION,
                                    EAXSOURCE_MAXEXCLUSION);
                    if (err) goto EAX_SOURCE_ERROR;

                    err = CheckFloatValueEAX(props->context,
                                    val.pSrcexclsend[i].flExclusionLFRatio,
                                    EAXSOURCE_MINEXCLUSIONLFRATIO,
                                    EAXSOURCE_MAXEXCLUSIONLFRATIO);
                    if (err) goto EAX_SOURCE_ERROR;
                }

                for (i=0; i<num; i++)
                {
                    ALuint j = Index[i];

                    src->EAXSendProps[j].lExclusion         = val.pSrcexclsend[i].lExclusion;
                    src->EAXSendProps[j].flExclusionLFRatio = val.pSrcexclsend[i].flExclusionLFRatio;
                }
            }
            else
            {
                for (i=0; i<num; i++)
                {
                    val.pSrcexclsend[i].lExclusion         = src->EAXSendProps[i].lExclusion;
                    val.pSrcexclsend[i].flExclusionLFRatio = src->EAXSendProps[i].flExclusionLFRatio;
                }
            }
            break;

        case EAXSOURCE_ACTIVEFXSLOTID:

            num = props->size / sizeof(GUID);

            if (err = CheckSizeEAX(props->context, num, MIN_SIZE))
                goto EAX_SOURCE_ERROR;
            clampu(num, 1, device->EAXSession.ulMaxActiveSends); //Revisar el hardcoded

            if (props->isSet)
            {
                if (err = ValidateSlotsEAX(props->context,
                                    &src->EAXSrcData,
                                    val.pActiveslots->guidActiveFXSlots,
                                    num))
                    goto EAX_SOURCE_ERROR;

                for (i=0; i<num; i++)
                     src->EAXActiveSlots.guidActiveFXSlots[i] = val.pActiveslots->guidActiveFXSlots[i];
            }
            else
            {
                for (i=0; i<num; i++)
                    val.pActiveslots->guidActiveFXSlots[i] = src->EAXActiveSlots.guidActiveFXSlots[i];
            }
            break;

        case EAXSOURCE_MACROFXFACTOR:
            UNIMPLEMENTED;
            break;

        case EAXSOURCE_SPEAKERLEVELS:
            UNIMPLEMENTED;
            break;

        case EAXSOURCE_ALL2DPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXSOURCE2DPROPERTIES)))
                goto EAX_SOURCE_ERROR;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                            val.pSource2d->lDirect,
                            EAXSOURCE_MINDIRECT,
                            EAXSOURCE_MAXDIRECT);
                if (err) goto EAX_SOURCE_ERROR;

                err = CheckIntValueEAX(props->context,
                            val.pSource2d->lDirectHF,
                            EAXSOURCE_MINDIRECTHF,
                            EAXSOURCE_MAXDIRECTHF);
                if (err) goto EAX_SOURCE_ERROR;

                err = CheckIntValueEAX(props->context,
                            val.pSource2d->lRoom,
                            EAXSOURCE_MINROOM,
                            EAXSOURCE_MAXROOM);
                if (err) goto EAX_SOURCE_ERROR;

                err = CheckIntValueEAX(props->context,
                            val.pSource2d->lRoomHF,
                            EAXSOURCE_MINROOMHF,
                            EAXSOURCE_MAXROOMHF);
                if (err) goto EAX_SOURCE_ERROR;

                src->EAXSrcProps.lDirect   = val.pSource2d->lDirect;
                src->EAXSrcProps.lDirectHF = val.pSource2d->lDirectHF;
                src->EAXSrcProps.lRoom     = val.pSource2d->lRoom;
                src->EAXSrcProps.lRoomHF   = val.pSource2d->lRoomHF;
            }
            else
            {
                val.pSource2d->lDirect   = src->EAXSrcProps.lDirect;
                val.pSource2d->lDirectHF = src->EAXSrcProps.lDirectHF;
                val.pSource2d->lRoom     = src->EAXSrcProps.lRoom;
                val.pSource2d->lRoomHF   = src->EAXSrcProps.lRoomHF;
            }
            break;

        default:
            ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
            err = AL_INVALID_ENUM;
            goto EAX_SOURCE_ERROR;
    }

    if (props->isSet && !props->context->EAXIsDefer)
    {
        props->context->EAXIsDefer = AL_FALSE;

        CalcFilterGainsEAX(props->context, src);
        ApplyFilterGainsEAX(props->context);
        UpdateSourceEAX(props->context, src);
    }

EAX_SOURCE_ERROR:
    almtx_unlock(&props->context->SourceLock);
    return err;
}

ALenum EAXMain(EAXCALLPROPS *props)
{
    ALenum      id;
    ALenum      err;
    ALenum      eaxerr;

    err     = AL_NO_ERROR;
    eaxerr  = EAX_OK;

    props->context = GetContextRef();
    if (!props->context)
        return AL_INVALID_NAME;
    alcSuspendContext(props->context);

    for(id=0; id<OBJECTS_SIZE; id++)
        if(IsEqualGUID(&id_list[id], props->propertySetID))
            break;

    if(id >= OBJECTS_SIZE)
    {
        ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
        err = AL_INVALID_NAME;
        goto EAX_CALL_PROCESSED;
    }

    if(err = VersionManagerEAX(props, id))
        goto EAX_CALL_PROCESSED;

    switch(id)
    {
        case _EAXPROPERTYID_EAX40_Context:
        case _EAXPROPERTYID_EAX50_Context:

            err = ContextEAX(props);
            break;

        case _DSPROPSETID_EAX10_ListenerProperties:
            UNIMPLEMENTED;
            break;

        case _DSPROPSETID_EAX20_ListenerProperties:
        case _DSPROPSETID_EAX30_ListenerProperties:
        case _EAXPROPERTYID_EAX40_FXSlot0:
        case _EAXPROPERTYID_EAX50_FXSlot0:

            props->slotID = SLOT_0;
            goto slots;

        case _EAXPROPERTYID_EAX40_FXSlot1:
        case _EAXPROPERTYID_EAX50_FXSlot1:

            props->slotID = SLOT_1;
            goto slots;

        case _EAXPROPERTYID_EAX40_FXSlot2:
        case _EAXPROPERTYID_EAX50_FXSlot2:

            props->slotID = SLOT_2;
            goto slots;

        case _EAXPROPERTYID_EAX40_FXSlot3:
        case _EAXPROPERTYID_EAX50_FXSlot3:

            props->slotID = SLOT_3;
        slots:
            err = SlotEAX(props);
            break;

        case _DSPROPSETID_EAX10_BufferProperties:
            UNIMPLEMENTED;
            break;

        case _DSPROPSETID_EAX20_BufferProperties:
        case _DSPROPSETID_EAX30_BufferProperties:
        case _EAXPROPERTYID_EAX40_Source:
        case _EAXPROPERTYID_EAX50_Source:

            err = SourceEAX(props);
            break;
    }

EAX_CALL_PROCESSED:
    alcProcessContext(props->context);
    ALCcontext_DecRef(props->context);

    return err;
}

AL_API ALenum AL_APIENTRY EAXSet(const GUID *propertySetID, ALuint property, ALuint source, ALvoid *value, ALuint size)
{
    EAXCALLPROPS props;

    props.propertySetID =  propertySetID;
    props.property      =  property;
    props.source        =  source;
    props.value         =  value;
    props.size          =  size;
    props.context       =  NULL;
    props.slotID        =  SLOT_NULL;
    props.guidID        = _EAX_NULL_GUID;
    props.isSet         =  AL_TRUE;

    return EAXMain(&props);
}

AL_API ALenum AL_APIENTRY EAXGet(const GUID *propertySetID, ALuint property, ALuint source, ALvoid *value, ALuint size)
{
    EAXCALLPROPS props;

    props.propertySetID =  propertySetID;
    props.property      =  property;
    props.source        =  source;
    props.value         =  value;
    props.size          =  size;
    props.context       =  NULL;
    props.slotID        =  SLOT_NULL;
    props.guidID        = _EAX_NULL_GUID;
    props.isSet         =  AL_FALSE;

    return EAXMain(&props);
}

AL_API ALboolean AL_APIENTRY EAXSetBufferMode(ALsizei n, ALuint *buffers, ALint value)
{
    return (n > 0 && buffers != NULL) ? AL_TRUE : AL_FALSE;
}

AL_API ALenum AL_APIENTRY EAXGetBufferMode(ALuint buffer, ALint *pReserved)
{
    return (alIsBuffer(buffer)) ? AL_STORAGE_AUTOMATIC : AL_INVALID_NAME;
}
