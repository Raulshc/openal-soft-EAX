
#include "config.h"

#include "alMain.h"
#include "alu.h"

/* CheckSizeEAX function */
static inline ALenum CheckSizeEAX(ALCcontext *context, ALuint size, ALuint ref)
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

/* CheckSourceSizeEAX function */
static inline ALenum CheckReverbSizeEAX(EAXCALLPROPS *props)
{
    ALenum target;
    ALenum eaxerr;
    ALenum err;

    target = props->context->Device->EAXManager.Target;
    err    = AL_NO_ERROR;

    switch (target)
    {
        case EAX20_TARGET:

            if (props->size < sizeof(EAX20LISTENERPROPERTIES))
                err = AL_INVALID_OPERATION;
            break;

        case EAX30_TARGET:
        case EAX40_TARGET:
        case EAX50_TARGET:

            if (props->size < sizeof(EAXLISTENERPROPERTIES))
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

    if (value >= min && value <= max)
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

/* CheckReverbParamsEAX function */
static inline ALenum CheckReverbParamsEAX(EAXCALLPROPS *props, EAXREVERBPROPERTIES *ReverbOut)
{
    ALenum target;
    ALenum err;

    target = props->context->Device->EAXManager.Target;
    err    = AL_NO_ERROR;

    switch (target)
    {
         case EAX20_TARGET:
         {
            EAX20LISTENERPROPERTIES *ReverbIn = props->value;

            ReverbOut->lRoom                  = ReverbIn->lRoom;
            ReverbOut->lRoomHF                = ReverbIn->lRoomHF;
            ReverbOut->flRoomRolloffFactor    = ReverbIn->flRoomRolloffFactor;
            ReverbOut->flDecayTime            = ReverbIn->flDecayTime;
            ReverbOut->flDecayHFRatio         = ReverbIn->flDecayHFRatio;
            ReverbOut->lReflections           = ReverbIn->lReflections;
            ReverbOut->flReflectionsDelay     = ReverbIn->flReflectionsDelay;
            ReverbOut->lReverb                = ReverbIn->lReverb;
            ReverbOut->flReverbDelay          = ReverbIn->flReverbDelay;
            ReverbOut->ulEnvironment          = ReverbIn->dwEnvironment;
            ReverbOut->flEnvironmentSize      = ReverbIn->flEnvironmentSize;
            ReverbOut->flEnvironmentDiffusion = ReverbIn->flEnvironmentDiffusion;
            ReverbOut->flAirAbsorptionHF      = ReverbIn->flAirAbsorptionHF;
            ReverbOut->ulFlags                = ReverbIn->dwFlags;
            break;
         }
         case EAX30_TARGET:
         case EAX40_TARGET:
         case EAX50_TARGET:
            memcpy(ReverbOut, props->value, sizeof(EAXREVERBPROPERTIES));
            break;
    }

    err = CheckUintValueEAX(props->context,
                    ReverbOut->ulEnvironment,
                    EAXREVERB_MINENVIRONMENT,
                    EAXREVERB_MAXENVIRONMENT);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flEnvironmentSize,
                    EAXREVERB_MINENVIRONMENTSIZE,
                    EAXREVERB_MAXENVIRONMENTSIZE);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flEnvironmentDiffusion,
                    EAXREVERB_MINENVIRONMENTDIFFUSION,
                    EAXREVERB_MAXENVIRONMENTDIFFUSION);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    ReverbOut->lRoom,
                    EAXREVERB_MINROOM,
                    EAXREVERB_MAXROOM);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    ReverbOut->lRoomHF,
                    EAXREVERB_MINROOMHF,
                    EAXREVERB_MAXROOMHF);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    ReverbOut->lRoomLF,
                    EAXREVERB_MINROOMLF,
                    EAXREVERB_MAXROOMLF);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flDecayTime,
                    EAXREVERB_MINDECAYTIME,
                    EAXREVERB_MAXDECAYTIME);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flDecayHFRatio,
                    EAXREVERB_MINDECAYHFRATIO,
                    EAXREVERB_MAXDECAYHFRATIO);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flDecayLFRatio,
                    EAXREVERB_MINDECAYLFRATIO,
                    EAXREVERB_MAXDECAYLFRATIO);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    ReverbOut->lReflections,
                    EAXREVERB_MINREFLECTIONS,
                    EAXREVERB_MAXREFLECTIONS);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flReflectionsDelay,
                    EAXREVERB_MINREFLECTIONSDELAY,
                    EAXREVERB_MAXREFLECTIONSDELAY);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->vReflectionsPan.x,
                    EAX_VECTOR_COMPONENT_MIN,
                    EAX_VECTOR_COMPONENT_MAX);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->vReflectionsPan.y,
                    EAX_VECTOR_COMPONENT_MIN,
                    EAX_VECTOR_COMPONENT_MAX);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->vReflectionsPan.z,
                    EAX_VECTOR_COMPONENT_MIN,
                    EAX_VECTOR_COMPONENT_MAX);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    ReverbOut->lReverb,
                    EAXREVERB_MINREVERB,
                    EAXREVERB_MAXREVERB);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flReverbDelay,
                    EAXREVERB_MINREVERBDELAY,
                    EAXREVERB_MAXREVERBDELAY);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->vReverbPan.x,
                    EAX_VECTOR_COMPONENT_MIN,
                    EAX_VECTOR_COMPONENT_MAX);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->vReverbPan.y,
                    EAX_VECTOR_COMPONENT_MIN,
                    EAX_VECTOR_COMPONENT_MAX);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->vReverbPan.z,
                    EAX_VECTOR_COMPONENT_MIN,
                    EAX_VECTOR_COMPONENT_MAX);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flEchoTime,
                    EAXREVERB_MINECHOTIME,
                    EAXREVERB_MAXECHOTIME);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flEchoDepth,
                    EAXREVERB_MINECHODEPTH,
                    EAXREVERB_MAXECHODEPTH);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flModulationTime,
                    EAXREVERB_MINMODULATIONTIME,
                    EAXREVERB_MAXMODULATIONTIME);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flModulationDepth,
                    EAXREVERB_MINMODULATIONDEPTH,
                    EAXREVERB_MAXMODULATIONDEPTH);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flAirAbsorptionHF,
                    EAXREVERB_MINAIRABSORPTIONHF,
                    EAXREVERB_MAXAIRABSORPTIONHF);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flHFReference,
                    EAXREVERB_MINHFREFERENCE,
                    EAXREVERB_MAXHFREFERENCE);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flLFReference,
                    EAXREVERB_MINLFREFERENCE,
                    EAXREVERB_MAXLFREFERENCE);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    ReverbOut->flRoomRolloffFactor,
                    EAXREVERB_MINROOMROLLOFFFACTOR,
                    EAXREVERB_MAXROOMROLLOFFFACTOR);
    if (err) return err;

    if (target < EAX30_TARGET)
        ReverbOut->ulFlags &= EAXREVERB_DEFAULTFLAGS;
    else
        ReverbOut->ulFlags &= (EAXREVERB_DEFAULTFLAGS       |
                               EAXREVERBFLAGS_ECHOTIMESCALE |
                               EAXREVERBFLAGS_MODULATIONTIMESCALE);
    return err;
}

/* CopyReverbParamsEAX function */
static inline ALvoid CopyReverbParamsEAX(EAXCALLPROPS *props, EAXREVERBPROPERTIES *ReverbIn)
{
    ALenum target = props->context->Device->EAXManager.Target;

    switch (target)
    {
         case EAX20_TARGET:
         {
            EAX20LISTENERPROPERTIES *ReverbOut = props->value;

            ReverbOut->lRoom                  = ReverbIn->lRoom;
            ReverbOut->lRoomHF                = ReverbIn->lRoomHF;
            ReverbOut->flRoomRolloffFactor    = ReverbIn->flRoomRolloffFactor;
            ReverbOut->flDecayTime            = ReverbIn->flDecayTime;
            ReverbOut->flDecayHFRatio         = ReverbIn->flDecayHFRatio;
            ReverbOut->lReflections           = ReverbIn->lReflections;
            ReverbOut->flReflectionsDelay     = ReverbIn->flReflectionsDelay;
            ReverbOut->lReverb                = ReverbIn->lReverb;
            ReverbOut->flReverbDelay          = ReverbIn->flReverbDelay;
            ReverbOut->dwEnvironment          = ReverbIn->ulEnvironment;
            ReverbOut->flEnvironmentSize      = ReverbIn->flEnvironmentSize;
            ReverbOut->flEnvironmentDiffusion = ReverbIn->flEnvironmentDiffusion;
            ReverbOut->flAirAbsorptionHF      = ReverbIn->flAirAbsorptionHF;
            ReverbOut->dwFlags                = ReverbIn->ulFlags & EAX20LISTENER_DEFAULTFLAGS;
            break;
         }
         case EAX30_TARGET:
         case EAX40_TARGET:
         case EAX50_TARGET:
            memcpy(ReverbIn, props->value, sizeof(EAXREVERBPROPERTIES));
            break;
    }
}

/* CheckAutowahParamsEAX function */
static inline ALenum CheckAutowahParamsEAX(EAXCALLPROPS *props)
{
    EAXAUTOWAHPROPERTIES *autowah;
    ALenum err;

    autowah = (LPEAXAUTOWAHPROPERTIES)props->value;
    err     = AL_NO_ERROR;

    err = CheckFloatValueEAX(props->context,
                    autowah->flAttackTime,
                    EAXAUTOWAH_MINATTACKTIME,
                    EAXAUTOWAH_MAXATTACKTIME);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    autowah->flReleaseTime,
                    EAXAUTOWAH_MINRELEASETIME,
                    EAXAUTOWAH_MAXRELEASETIME);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    autowah->lResonance,
                    EAXAUTOWAH_MINRESONANCE,
                    EAXAUTOWAH_MAXRESONANCE);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    autowah->lPeakLevel,
                    EAXAUTOWAH_MINPEAKLEVEL,
                    EAXAUTOWAH_MAXPEAKLEVEL);
    return err;
}

/* CheckChorusParamsEAX function */
static inline ALenum CheckChorusParamsEAX(EAXCALLPROPS *props)
{
    EAXCHORUSPROPERTIES *chorus;
    ALenum err;

    chorus = (LPEAXCHORUSPROPERTIES)props->value;
    err    = AL_NO_ERROR;

    err = CheckUintValueEAX(props->context,
                    chorus->ulWaveform,
                    EAXCHORUS_MINWAVEFORM,
                    EAXCHORUS_MAXWAVEFORM);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    chorus->lPhase,
                    EAXCHORUS_MINPHASE,
                    EAXCHORUS_MAXPHASE);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    chorus->flRate,
                    EAXCHORUS_MINRATE,
                    EAXCHORUS_MAXRATE);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    chorus->flDepth,
                    EAXCHORUS_MINDEPTH,
                    EAXCHORUS_MAXDEPTH);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    chorus->flFeedback,
                    EAXCHORUS_MINFEEDBACK,
                    EAXCHORUS_MAXFEEDBACK);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    chorus->flDelay,
                    EAXCHORUS_MINDELAY,
                    EAXCHORUS_MAXDELAY);
    return err;
}

/* CheckDistortionParamsEAX function */
static inline ALenum CheckDistortionParamsEAX(EAXCALLPROPS *props)
{
    EAXDISTORTIONPROPERTIES *distortion;
    ALenum err;

    distortion = (LPEAXDISTORTIONPROPERTIES)props->value;
    err        = AL_NO_ERROR;

    err = CheckFloatValueEAX(props->context,
                    distortion->flEdge,
                    EAXDISTORTION_MINEDGE,
                    EAXDISTORTION_MAXEDGE);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    distortion->lGain,
                    EAXDISTORTION_MINGAIN,
                    EAXDISTORTION_MAXGAIN);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    distortion->flLowPassCutOff,
                    EAXDISTORTION_MINLOWPASSCUTOFF,
                    EAXDISTORTION_MAXLOWPASSCUTOFF);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    distortion->flEQCenter,
                    EAXDISTORTION_MINEQCENTER,
                    EAXDISTORTION_MAXEQCENTER);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    distortion->flEQBandwidth,
                    EAXDISTORTION_MINEQBANDWIDTH,
                    EAXDISTORTION_MAXEQBANDWIDTH);
    return err;
}

/* CheckEchoParamsEAX function */
static inline ALenum CheckEchoParamsEAX(EAXCALLPROPS *props)
{
    EAXECHOPROPERTIES *echo;
    ALenum err;

    echo = (LPEAXECHOPROPERTIES)props->value;
    err  = AL_NO_ERROR;

    err = CheckFloatValueEAX(props->context,
                    echo->flDelay,
                    EAXECHO_MINDELAY,
                    EAXECHO_MAXDELAY);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    echo->flLRDelay,
                    EAXECHO_MINLRDELAY,
                    EAXECHO_MAXLRDELAY);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    echo->flDamping,
                    EAXECHO_MINDAMPING,
                    EAXECHO_MAXDAMPING);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    echo->flFeedback,
                    EAXECHO_MINFEEDBACK,
                    EAXECHO_MAXFEEDBACK);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    echo->flSpread,
                    EAXECHO_MINSPREAD,
                    EAXECHO_MAXSPREAD);
    return err;
}

/* CheckEqualizerParamsEAX function */
static inline ALenum CheckEqualizerParamsEAX(EAXCALLPROPS *props)
{
    EAXEQUALIZERPROPERTIES *equalizer;
    ALenum err;

    equalizer = (LPEAXEQUALIZERPROPERTIES)props->value;
    err       = AL_NO_ERROR;

    err = CheckIntValueEAX(props->context,
                    equalizer->lLowGain,
                    EAXEQUALIZER_MINLOWGAIN,
                    EAXEQUALIZER_MAXLOWGAIN);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    equalizer->flLowCutOff,
                    EAXEQUALIZER_MINLOWCUTOFF,
                    EAXEQUALIZER_MAXLOWCUTOFF);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    equalizer->lMid1Gain,
                    EAXEQUALIZER_MINMID1GAIN,
                    EAXEQUALIZER_MAXMID1GAIN);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    equalizer->flMid1Center,
                    EAXEQUALIZER_MINMID1CENTER,
                    EAXEQUALIZER_MAXMID1CENTER);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    equalizer->flMid1Width,
                    EAXEQUALIZER_MINMID1WIDTH,
                    EAXEQUALIZER_MAXMID1WIDTH);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    equalizer->lMid2Gain,
                    EAXEQUALIZER_MINMID2GAIN,
                    EAXEQUALIZER_MAXMID2GAIN);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    equalizer->flMid2Center,
                    EAXEQUALIZER_MINMID2CENTER,
                    EAXEQUALIZER_MAXMID2CENTER);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    equalizer->flMid2Width,
                    EAXEQUALIZER_MINMID2WIDTH,
                    EAXEQUALIZER_MAXMID2WIDTH);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    equalizer->lHighGain,
                    EAXEQUALIZER_MINHIGHGAIN,
                    EAXEQUALIZER_MAXHIGHGAIN);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    equalizer->flHighCutOff,
                    EAXEQUALIZER_MINHIGHCUTOFF,
                    EAXEQUALIZER_MAXHIGHCUTOFF);
    return err;
}

/* CheckFlangerParamsEAX function */
static inline ALenum CheckFlangerParamsEAX(EAXCALLPROPS *props)
{
    EAXFLANGERPROPERTIES *flanger;
    ALenum err;

    flanger = (LPEAXFLANGERPROPERTIES)props->value;
    err     = AL_NO_ERROR;

    err = CheckUintValueEAX(props->context,
                    flanger->ulWaveform,
                    EAXFLANGER_MINWAVEFORM,
                    EAXFLANGER_MAXWAVEFORM);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    flanger->lPhase,
                    EAXFLANGER_MINPHASE,
                    EAXFLANGER_MAXPHASE);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    flanger->flRate,
                    EAXFLANGER_MINRATE,
                    EAXFLANGER_MAXRATE);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    flanger->flDepth,
                    EAXFLANGER_MINDEPTH,
                    EAXFLANGER_MAXDEPTH);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    flanger->flFeedback,
                    EAXFLANGER_MINFEEDBACK,
                    EAXFLANGER_MAXFEEDBACK);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    flanger->flDelay,
                    EAXFLANGER_MINDELAY,
                    EAXFLANGER_MAXDELAY);
    return err;
}

/* CheckFshifterParamsEAX function */
static inline ALenum CheckFshifterParamsEAX(EAXCALLPROPS *props)
{
    EAXFREQUENCYSHIFTERPROPERTIES *fshifter;
    ALenum err;

    fshifter = (LPEAXFREQUENCYSHIFTERPROPERTIES)props->value;
    err      = AL_NO_ERROR;

    err = CheckFloatValueEAX(props->context,
                    fshifter->flFrequency,
                    EAXFREQUENCYSHIFTER_MINFREQUENCY,
                    EAXFREQUENCYSHIFTER_MAXFREQUENCY);
    if (err) return err;

    err = CheckUintValueEAX(props->context,
                    fshifter->ulLeftDirection,
                    EAXFREQUENCYSHIFTER_MINLEFTDIRECTION,
                    EAXFREQUENCYSHIFTER_MAXLEFTDIRECTION);
    if (err) return err;

    err = CheckUintValueEAX(props->context,
                    fshifter->ulRightDirection,
                    EAXFREQUENCYSHIFTER_MINRIGHTDIRECTION,
                    EAXFREQUENCYSHIFTER_MAXRIGHTDIRECTION);
    return err;
}

/* CheckVmorpherParamsEAX function */
static inline ALenum CheckVmorpherParamsEAX(EAXCALLPROPS *props)
{
    EAXVOCALMORPHERPROPERTIES *vmorpher;
    ALenum err;

    vmorpher = (LPEAXVOCALMORPHERPROPERTIES)props->value;
    err = AL_NO_ERROR;

    err = CheckUintValueEAX(props->context,
                    vmorpher->ulPhonemeA,
                    EAXVOCALMORPHER_MINPHONEMEA,
                    EAXVOCALMORPHER_MAXPHONEMEA);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    vmorpher->lPhonemeACoarseTuning,
                    EAXVOCALMORPHER_MINPHONEMEACOARSETUNING,
                    EAXVOCALMORPHER_MAXPHONEMEACOARSETUNING);
    if (err) return err;

    err = CheckUintValueEAX(props->context,
                    vmorpher->ulPhonemeB,
                    EAXVOCALMORPHER_MINPHONEMEB,
                    EAXVOCALMORPHER_MAXPHONEMEB);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    vmorpher->lPhonemeBCoarseTuning,
                    EAXVOCALMORPHER_MINPHONEMEBCOARSETUNING,
                    EAXVOCALMORPHER_MAXPHONEMEBCOARSETUNING);
    if (err) return err;

    err = CheckUintValueEAX(props->context,
                    vmorpher->ulWaveform,
                    EAXVOCALMORPHER_MINWAVEFORM,
                    EAXVOCALMORPHER_MAXWAVEFORM);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    vmorpher->flRate,
                    EAXVOCALMORPHER_MINRATE,
                    EAXVOCALMORPHER_MAXRATE);
    return err;
}

/* CheckPshifterParamsEAX function */
static inline ALenum CheckPshifterParamsEAX(EAXCALLPROPS *props)
{
    EAXPITCHSHIFTERPROPERTIES *pshifter;
    ALenum err;

    pshifter = (LPEAXPITCHSHIFTERPROPERTIES)props->value;
    err = AL_NO_ERROR;

    err = CheckIntValueEAX(props->context,
                    pshifter->lCoarseTune,
                    EAXPITCHSHIFTER_MINCOARSETUNE,
                    EAXPITCHSHIFTER_MAXCOARSETUNE);
    if (err) return err;

    err = CheckIntValueEAX(props->context,
                    pshifter->lFineTune,
                    EAXPITCHSHIFTER_MINFINETUNE,
                    EAXPITCHSHIFTER_MAXFINETUNE);
    return err;
}

/* CheckModulatorParamsEAX function */
static inline ALenum CheckModulatorParamsEAX(EAXCALLPROPS *props)
{
    EAXRINGMODULATORPROPERTIES *modulator;
    ALenum err;

    modulator = (LPEAXRINGMODULATORPROPERTIES)props->value;
    err = AL_NO_ERROR;

    err = CheckFloatValueEAX(props->context,
                    modulator->flFrequency,
                    EAXRINGMODULATOR_MINFREQUENCY,
                    EAXRINGMODULATOR_MAXFREQUENCY);
    if (err) return err;

    err = CheckFloatValueEAX(props->context,
                    modulator->flHighPassCutOff,
                    EAXRINGMODULATOR_MINHIGHPASSCUTOFF,
                    EAXRINGMODULATOR_MAXHIGHPASSCUTOFF);
    if (err) return err;

    err = CheckUintValueEAX(props->context,
                    modulator->ulWaveform,
                    EAXRINGMODULATOR_MINWAVEFORM,
                    EAXRINGMODULATOR_MAXWAVEFORM);
    return err;
}

/* MapEffectEAX function */
ALenum MapEffectEAX(ALenum eEffect)
{
    /* Convert an EAX Effect ID to its EFX Effect token */
    switch (eEffect)
    {
        case _EAX_NULL_GUID:
            return AL_EFFECT_NULL;

        case _EAX_REVERB_EFFECT:
            return AL_EFFECT_EAXREVERB;

        case _EAX_AGCCOMPRESSOR_EFFECT:
            return AL_EFFECT_COMPRESSOR;

        case _EAX_AUTOWAH_EFFECT:
            return AL_EFFECT_AUTOWAH;

        case _EAX_CHORUS_EFFECT:
            return AL_EFFECT_CHORUS;

        case _EAX_DISTORTION_EFFECT:
            return AL_EFFECT_DISTORTION;

        case _EAX_ECHO_EFFECT:
            return AL_EFFECT_ECHO;

        case _EAX_EQUALIZER_EFFECT:
            return AL_EFFECT_EQUALIZER;

        case _EAX_FLANGER_EFFECT:
            return AL_EFFECT_FLANGER;

        case _EAX_FREQUENCYSHIFTER_EFFECT:
            return AL_EFFECT_FREQUENCY_SHIFTER;

        case _EAX_VOCALMORPHER_EFFECT:
            return AL_EFFECT_VOCAL_MORPHER;

        case _EAX_PITCHSHIFTER_EFFECT:
            return AL_EFFECT_PITCH_SHIFTER;

        case _EAX_RINGMODULATOR_EFFECT:
            return AL_EFFECT_RING_MODULATOR;

        default:
            return AL_EFFECT_NULL;
    }
}

/* CopyDefPropsEffectEAX function. Load the default properties of an 
   EAX effect.*/
ALenum CopyDefPropsEffectEAX(EAXEFFECTPROPS *props, ALenum EffType)
{
    switch (EffType)
    {
        case _EAX_NULL_GUID:
            memset(props, 0, sizeof(EAXEFFECTPROPS));
            break;

        case _EAX_REVERB_EFFECT:
            memcpy(&props->EAXReverb, &defReverbProps, 
                   sizeof(EAXREVERBPROPERTIES));
            break;

        case _EAX_AGCCOMPRESSOR_EFFECT:
            memcpy(&props->Compressor, &defCompressorProps, 
                   sizeof(EAXAGCCOMPRESSORPROPERTIES));
            break;

        case _EAX_AUTOWAH_EFFECT:
            memcpy(&props->Autowah, &defAutowahProps, 
                   sizeof(EAXAUTOWAHPROPERTIES));
            break;

        case _EAX_CHORUS_EFFECT:
            memcpy(&props->Chorus, &defChorusProps,
                   sizeof(EAXCHORUSPROPERTIES));
            break;

        case _EAX_DISTORTION_EFFECT:
            memcpy(&props->Distorsion, &defDistortionProps,
                   sizeof(EAXDISTORTIONPROPERTIES));
            break;

        case _EAX_ECHO_EFFECT:
            memcpy(&props->Echo, &defEchoProps,
                   sizeof(EAXECHOPROPERTIES));
            break;

        case _EAX_EQUALIZER_EFFECT:
            memcpy(&props->Equalizer, &defEqualizerProps,
                   sizeof(EAXEQUALIZERPROPERTIES));
            break;

        case _EAX_FLANGER_EFFECT:
            memcpy(&props->Flanger, &defFlangerProps,
                   sizeof(EAXFLANGERPROPERTIES));
            break;

        case _EAX_FREQUENCYSHIFTER_EFFECT:
            memcpy(&props->Fshifter, &defFshifterProps,
                   sizeof(EAXFREQUENCYSHIFTERPROPERTIES));
            break;

        case _EAX_VOCALMORPHER_EFFECT:
            memcpy(&props->Vmorpher, &defVmorpherProps,
                   sizeof(EAXVOCALMORPHERPROPERTIES));
            break;

        case _EAX_PITCHSHIFTER_EFFECT:
            memcpy(&props->Pshifter, &defPshifterProps,
                   sizeof(EAXPITCHSHIFTERPROPERTIES));
            break;

        case _EAX_RINGMODULATOR_EFFECT:
            memcpy(&props->Chorus, &defModulatorProps,
                   sizeof(EAXRINGMODULATORPROPERTIES));
            break;
    }

    return AL_NO_ERROR; //Review if needed
}

/* ApplyReverbParamsEAX function */
static void ApplyReverbParamsEAX(ALuint effect, const EAXREVERBPROPERTIES *props)
{
    /* FIXME: Need to validate property values... Ignore? Clamp? Error? */
    alEffectf(effect, AL_EAXREVERB_DENSITY,
        clampf(powf(props->flEnvironmentSize, 3.0f) / 16.0f, 0.0f, 1.0f)
    );
    alEffectf(effect, AL_EAXREVERB_DIFFUSION, props->flEnvironmentDiffusion);

    alEffectf(effect, AL_EAXREVERB_GAIN, mB_to_gain((ALfloat)props->lRoom));
    alEffectf(effect, AL_EAXREVERB_GAINHF, mB_to_gain((ALfloat)props->lRoomHF));
    alEffectf(effect, AL_EAXREVERB_GAINLF, mB_to_gain((ALfloat)props->lRoomLF));

    alEffectf(effect, AL_EAXREVERB_DECAY_TIME, props->flDecayTime);
    alEffectf(effect, AL_EAXREVERB_DECAY_HFRATIO, props->flDecayHFRatio);
    alEffectf(effect, AL_EAXREVERB_DECAY_LFRATIO, props->flDecayLFRatio);

    /* NOTE: Imprecision can cause some converted volume levels to land outside
     * EFX's gain limits (e.g. EAX's +1000mB volume limit gets converted to
     * 3.162something, while EFX defines the limit as 3.16; close enough for
     * practical uses, but still technically an error).
     */
    alEffectf(effect, AL_EAXREVERB_REFLECTIONS_GAIN,
        clampf(mB_to_gain((ALfloat)props->lReflections), AL_EAXREVERB_MIN_REFLECTIONS_GAIN,
            AL_EAXREVERB_MAX_REFLECTIONS_GAIN)
    );
    alEffectf(effect, AL_EAXREVERB_REFLECTIONS_DELAY, props->flReflectionsDelay);
    alEffectfv(effect, AL_EAXREVERB_REFLECTIONS_PAN, &props->vReflectionsPan.x);

    alEffectf(effect, AL_EAXREVERB_LATE_REVERB_GAIN,
        clampf(mB_to_gain((ALfloat)props->lReverb), AL_EAXREVERB_MIN_LATE_REVERB_GAIN,
            AL_EAXREVERB_MAX_LATE_REVERB_GAIN)
    );
    alEffectf(effect, AL_EAXREVERB_LATE_REVERB_DELAY, props->flReverbDelay);
    alEffectfv(effect, AL_EAXREVERB_LATE_REVERB_PAN, &props->vReverbPan.x);

    alEffectf(effect, AL_EAXREVERB_ECHO_TIME, props->flEchoTime);
    alEffectf(effect, AL_EAXREVERB_ECHO_DEPTH, props->flEchoDepth);

    alEffectf(effect, AL_EAXREVERB_MODULATION_TIME, props->flModulationTime);
    alEffectf(effect, AL_EAXREVERB_MODULATION_DEPTH, props->flModulationDepth);

    alEffectf(effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF,
        clampf(mB_to_gain(props->flAirAbsorptionHF), AL_EAXREVERB_MIN_AIR_ABSORPTION_GAINHF,
            AL_EAXREVERB_MAX_AIR_ABSORPTION_GAINHF)
    );

    alEffectf(effect, AL_EAXREVERB_HFREFERENCE, props->flHFReference);
    alEffectf(effect, AL_EAXREVERB_LFREFERENCE, props->flLFReference);

    alEffectf(effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, props->flRoomRolloffFactor);

    alEffecti(effect, AL_EAXREVERB_DECAY_HFLIMIT,
        (props->ulFlags&EAXREVERBFLAGS_DECAYHFLIMIT) ?
        AL_TRUE : AL_FALSE);

    alGetError();
}

/* ApplyCompressorParamsEAX function */
static void ApplyCompressorParamsEAX(ALuint effect, const EAXAGCCOMPRESSORPROPERTIES *props)
{
    alEffecti(effect, AL_COMPRESSOR_ONOFF, props->ulOnOff);

    alGetError();
}

/* ApplyAutowahParamsEAX function */
static void ApplyAutowahParamsEAX(ALuint effect, const EAXAUTOWAHPROPERTIES *props)
{
    alEffectf(effect, AL_AUTOWAH_ATTACK_TIME, props->flAttackTime);
    alEffectf(effect, AL_AUTOWAH_RELEASE_TIME, props->flReleaseTime);
    alEffectf(effect, AL_AUTOWAH_RESONANCE, mB_to_gain((ALfloat)props->lResonance));
    alEffectf(effect, AL_AUTOWAH_PEAK_GAIN, mB_to_gain((ALfloat)props->lPeakLevel));

    alGetError();
}

/* ApplyChorusParamsEAX function */
static void ApplyChorusParamsEAX(ALuint effect, const EAXCHORUSPROPERTIES *props)
{
    alEffecti(effect, AL_CHORUS_WAVEFORM, props->ulWaveform);
    alEffecti(effect, AL_CHORUS_PHASE, props->lPhase);
    alEffectf(effect, AL_CHORUS_RATE, props->flRate);
    alEffectf(effect, AL_CHORUS_DEPTH, props->flDepth);
    alEffectf(effect, AL_CHORUS_FEEDBACK, props->flFeedback);
    alEffectf(effect, AL_CHORUS_DELAY, props->flDelay);

    alGetError();
}

/* ApplyDistortionParamsEAX function */
static void ApplyDistortionParamsEAX(ALuint effect, const EAXDISTORTIONPROPERTIES *props)
{
    alEffectf(effect, AL_DISTORTION_EDGE, props->flEdge);
    alEffectf(effect, AL_DISTORTION_GAIN, mB_to_gain((ALfloat)props->lGain));
    alEffectf(effect, AL_DISTORTION_LOWPASS_CUTOFF, props->flLowPassCutOff);
    alEffectf(effect, AL_DISTORTION_EQCENTER, props->flEQCenter);
    alEffectf(effect, AL_DISTORTION_EQBANDWIDTH, props->flEQBandwidth);

    alGetError();
}

/* ApplyEchoParamsEAX function */
static void ApplyEchoParamsEAX(ALuint effect, const EAXECHOPROPERTIES *props)
{
    alEffectf(effect, AL_ECHO_DELAY, props->flDelay);
    alEffectf(effect, AL_ECHO_LRDELAY, props->flLRDelay);
    alEffectf(effect, AL_ECHO_DAMPING, props->flDamping);
    alEffectf(effect, AL_ECHO_FEEDBACK, props->flFeedback);
    alEffectf(effect, AL_ECHO_SPREAD, props->flSpread);

    alGetError();
}

/* ApplyEqualizerParamsEAX function */
static void ApplyEqualizerParamsEAX(ALuint effect, const EAXEQUALIZERPROPERTIES *props)
{
    alEffectf(effect, AL_EQUALIZER_LOW_GAIN, mB_to_gain((ALfloat)props->lLowGain));
    alEffectf(effect, AL_EQUALIZER_LOW_CUTOFF, props->flLowCutOff);
    alEffectf(effect, AL_EQUALIZER_MID1_GAIN, mB_to_gain((ALfloat)props->lMid1Gain));
    alEffectf(effect, AL_EQUALIZER_MID1_CENTER, props->flMid1Center);
    alEffectf(effect, AL_EQUALIZER_MID1_WIDTH, props->flMid1Width);
    alEffectf(effect, AL_EQUALIZER_MID2_GAIN, mB_to_gain((ALfloat)props->lMid2Gain));
    alEffectf(effect, AL_EQUALIZER_MID2_CENTER, props->flMid2Center);
    alEffectf(effect, AL_EQUALIZER_MID2_WIDTH, props->flMid2Width);
    alEffectf(effect, AL_EQUALIZER_HIGH_GAIN, mB_to_gain((ALfloat)props->lHighGain));
    alEffectf(effect, AL_EQUALIZER_HIGH_CUTOFF, props->flHighCutOff);

    alGetError();
}

/* ApplyFlangerParamsEAX function */
static void ApplyFlangerParamsEAX(ALuint effect, const EAXFLANGERPROPERTIES *props)
{
    alEffecti(effect, AL_FLANGER_WAVEFORM, props->ulWaveform);
    alEffecti(effect, AL_FLANGER_PHASE, props->lPhase);
    alEffectf(effect, AL_FLANGER_RATE, props->flRate);
    alEffectf(effect, AL_FLANGER_DEPTH, props->flDepth);
    alEffectf(effect, AL_FLANGER_FEEDBACK, props->flFeedback);
    alEffectf(effect, AL_FLANGER_DELAY, props->flDelay);

    alGetError();
}

/* ApplyFshifterParamsEAX function */
static void ApplyFshifterParamsEAX(ALuint effect, const EAXFREQUENCYSHIFTERPROPERTIES *props)
{
    alEffectf(effect, AL_FREQUENCY_SHIFTER_FREQUENCY, props->flFrequency);
    alEffecti(effect, AL_FREQUENCY_SHIFTER_LEFT_DIRECTION, props->ulLeftDirection);
    alEffecti(effect, AL_FREQUENCY_SHIFTER_RIGHT_DIRECTION, props->ulRightDirection);

    alGetError();
}

/* ApplyVmorpherParamsEAX function */
static void ApplyVmorpherParamsEAX(ALuint effect, const EAXVOCALMORPHERPROPERTIES *props)
{
    alEffecti(effect, AL_VOCAL_MORPHER_PHONEMEA, props->ulPhonemeA);
    alEffecti(effect, AL_VOCAL_MORPHER_PHONEMEA_COARSE_TUNING, props->lPhonemeACoarseTuning);
    alEffecti(effect, AL_VOCAL_MORPHER_PHONEMEB, props->ulPhonemeB);
    alEffecti(effect, AL_VOCAL_MORPHER_PHONEMEB_COARSE_TUNING, props->lPhonemeBCoarseTuning);
    alEffecti(effect, AL_VOCAL_MORPHER_WAVEFORM, props->ulWaveform);
    alEffectf(effect, AL_VOCAL_MORPHER_RATE, props->flRate);

    alGetError();
}
/* ApplyPshifterParamsEAX function */
static void ApplyPshifterParamsEAX(ALuint effect, const EAXPITCHSHIFTERPROPERTIES *props)
{
    alEffecti(effect, AL_PITCH_SHIFTER_COARSE_TUNE, props->lCoarseTune);
    alEffecti(effect, AL_PITCH_SHIFTER_FINE_TUNE, props->lFineTune);

    alGetError();
}

/* ApplyModulatorParamsEAX function */
static void ApplyModulatorParamsEAX(ALuint effect, const EAXRINGMODULATORPROPERTIES *props)
{
    alEffectf(effect, AL_RING_MODULATOR_FREQUENCY, props->flFrequency);
    alEffectf(effect, AL_RING_MODULATOR_HIGHPASS_CUTOFF, props->flHighPassCutOff);
    alEffecti(effect, AL_RING_MODULATOR_WAVEFORM, props->ulWaveform);

    alGetError();
}

/* ApplyEffectParamsEAX function. Modify the effect params (e.g. AL_EFFECT_EAXREVERB) and attach
the modified effect into an EFX auxiliary slot. */
ALenum ApplyEffectParamsEAX(EAXCALLPROPS *props)
{
    EAXEFFECTPROPS *eff_props;
    ALCdevice      *device;
    ALuint          effect_idx;
    ALenum          effect_type;
    ALint           slotID;

    device      =  props->context->Device;
    slotID      =  props->slotID;
    effect_idx  =  device->EAXhw.Slots[slotID].EffIdx;
    effect_type =  device->EAXhw.Slots[slotID].EffType;
    eff_props   = &device->EAXEffectProps[slotID];

    switch (effect_type)
    {
        case _EAX_NULL_GUID:
            break;

        case _EAX_REVERB_EFFECT:
            ApplyReverbParamsEAX(effect_idx, (LPEAXREVERBPROPERTIES)eff_props);
            break;

        case _EAX_AGCCOMPRESSOR_EFFECT:
            ApplyCompressorParamsEAX(effect_idx, (LPEAXAGCCOMPRESSORPROPERTIES)eff_props);
            break;

        case _EAX_AUTOWAH_EFFECT:
            ApplyAutowahParamsEAX(effect_idx, (LPEAXAUTOWAHPROPERTIES)eff_props);
            break;

        case _EAX_CHORUS_EFFECT:
            ApplyChorusParamsEAX(effect_idx, (LPEAXCHORUSPROPERTIES)eff_props);
            break;

        case _EAX_DISTORTION_EFFECT:
            ApplyDistortionParamsEAX(effect_idx, (LPEAXDISTORTIONPROPERTIES)eff_props);
            break;

        case _EAX_ECHO_EFFECT:
            ApplyEchoParamsEAX(effect_idx, (LPEAXECHOPROPERTIES)eff_props);
            break;

        case _EAX_EQUALIZER_EFFECT:
            ApplyEqualizerParamsEAX(effect_idx, (LPEAXEQUALIZERPROPERTIES)eff_props);
            break;

        case _EAX_FLANGER_EFFECT:
            ApplyFlangerParamsEAX(effect_idx, (LPEAXFLANGERPROPERTIES)eff_props);
            break;

        case _EAX_FREQUENCYSHIFTER_EFFECT:
            ApplyFshifterParamsEAX(effect_idx, (LPEAXFREQUENCYSHIFTERPROPERTIES)eff_props);
            break;

        case _EAX_VOCALMORPHER_EFFECT:
            ApplyVmorpherParamsEAX(effect_idx, (LPEAXVOCALMORPHERPROPERTIES)eff_props);
            break;

        case _EAX_PITCHSHIFTER_EFFECT:
            ApplyPshifterParamsEAX(effect_idx, (LPEAXPITCHSHIFTERPROPERTIES)eff_props);
            break;

        case _EAX_RINGMODULATOR_EFFECT:
            ApplyModulatorParamsEAX(effect_idx, (LPEAXRINGMODULATORPROPERTIES)eff_props);
            break;
    }
    alAuxiliaryEffectSloti(device->EAXhw.Slots[slotID].Idx, AL_EFFECTSLOT_EFFECT, effect_idx);

    return AL_NO_ERROR;
}

/* CalcScaleEnvSizeEAX function */
static inline ALenum CalcReverbEnvSizeEAX(ALCcontext *context, EAXREVERBPROPERTIES *props, ALfloat newsize)
{
    ALfloat scale  = newsize / props->flEnvironmentSize;
    ALenum  target = context->Device->EAXManager.Target;

    props->flEnvironmentSize = newsize;

    if((props->ulFlags & EAXREVERBFLAGS_DECAYTIMESCALE))
    {
        props->flDecayTime *= scale;
        props->flDecayTime = clampf(props->flDecayTime, 0.1f, 20.0f);
    }
    if((props->ulFlags & EAXREVERBFLAGS_REFLECTIONSSCALE))
    {
        props->lReflections -= gain_to_mB(scale);
        props->lReflections = clampi(props->lReflections, -10000, 1000);
    }
    if((props->ulFlags & EAXREVERBFLAGS_REFLECTIONSDELAYSCALE))
    {
        props->flReflectionsDelay *= scale;
        props->flReflectionsDelay = clampf(props->flReflectionsDelay, 0.0f, 0.3f);
    }
    if((props->ulFlags & EAXREVERBFLAGS_REVERBSCALE))
    {
        long diff = gain_to_mB(scale);
        /* This is scaled by an extra 1/3rd if decay time isn't also scaled, to
         * account for the (lack of) change on the send's initial decay.
         */
        if(!(props->ulFlags & EAXREVERBFLAGS_DECAYTIMESCALE))
            diff = diff * 3 / 2;
        props->lReverb -= diff;
        props->lReverb = clampi(props->lReverb, -10000, 2000);
    }
    if((props->ulFlags & EAXREVERBFLAGS_REVERBDELAYSCALE))
    {
        props->flReverbDelay *= scale;
        props->flReverbDelay = clampf(props->flReverbDelay, 0.0f, 0.1f);
    }
    if((props->ulFlags & EAXREVERBFLAGS_ECHOTIMESCALE) && target > EAX20_TARGET)
    {
        props->flEchoTime *= scale;
        props->flEchoTime = clampf(props->flEchoTime, 0.075f, 0.25f);
    }
    if((props->ulFlags & EAXREVERBFLAGS_MODULATIONTIMESCALE) && target > EAX20_TARGET)
    {
        props->flModulationTime *= scale;
        props->flModulationTime = clampf(props->flModulationTime, 0.04f, 4.0f);
    }

    return AL_NO_ERROR;
}

/*ReverbEAX function. Process the EAXSet/EAXGet reverb calls*/
static inline ALenum ReverbEAX(EAXCALLPROPS *props)
{
    EAXREVERBPROPERTIES *reverb;
    LPEAXPROPS           val = {props->value};
    ALCdevice           *device;
    ALenum               err;

    device = props->context->Device;
    reverb = &device->EAXEffectProps[props->slotID].EAXReverb;
    err    = AL_NO_ERROR;

    switch (props->property & ~EAXREVERB_DEFERRED)
    {
        case EAXREVERB_NONE:
             break;

        case EAXREVERB_ALLPARAMETERS:
            if (err = CheckReverbSizeEAX(props))
                return err;
            else if(props->isSet)
            {
                EAXREVERBPROPERTIES tmp = *reverb;

                if (err = CheckReverbParamsEAX(props, &tmp))
                    return err;

                tmp.ulFlags |= reverb->ulFlags;
                *reverb      = tmp;
            }
            else
                CopyReverbParamsEAX(props, reverb);

            break;

        case EAXREVERB_ENVIRONMENT:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if(props->isSet)
            {
                ALuint uEnv;

                err = CheckUintValueEAX(props->context,
                                *val.pUlong,
                                EAXREVERB_MINENVIRONMENT,
                                EAXREVERB_MAXENVIRONMENT);
                if (err) return err;

                uEnv = *val.pUlong;
                memcpy(reverb, &reverb_presets[uEnv],
                        sizeof(EAXREVERBPROPERTIES));
            }
            else
                *val.pUlong = reverb->ulEnvironment;
            break;

        case EAXREVERB_ENVIRONMENTSIZE:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINENVIRONMENTSIZE,
                                EAXREVERB_MAXENVIRONMENTSIZE);
                if (err) return err;

                reverb->flEnvironmentSize = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
                CalcReverbEnvSizeEAX(props->context, reverb, *val.pFloat);
            }
            else
                *val.pFloat = reverb->flEnvironmentSize;
            break;

        case EAXREVERB_ENVIRONMENTDIFFUSION:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINENVIRONMENTDIFFUSION,
                                EAXREVERB_MAXENVIRONMENTDIFFUSION);
                if (err) return err;

                reverb->flEnvironmentDiffusion = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flEnvironmentDiffusion;
            break;

        case EAXREVERB_ROOM:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if(props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXREVERB_MINROOM,
                                EAXREVERB_MAXROOM);
                if (err) return err;

                reverb->lRoom = *val.pLong;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pLong = reverb->lRoom;
            break;

        case EAXREVERB_ROOMHF:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXREVERB_MINROOMHF,
                                EAXREVERB_MAXROOMHF);
                if (err) return err;

                reverb->lRoomHF = *val.pLong;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pLong = reverb->lRoomHF;
            break;

        case EAXREVERB_ROOMLF:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXREVERB_MINROOMLF,
                                EAXREVERB_MAXROOMLF);
                if (err) return err;

                reverb->lRoomLF = *val.pLong;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pLong = reverb->lRoomLF;
            break;

        case EAXREVERB_DECAYTIME:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINDECAYTIME,
                                EAXREVERB_MAXDECAYTIME);
                if (err) return err;

                reverb->flDecayTime = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flDecayTime;
            break;

        case EAXREVERB_DECAYHFRATIO:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINDECAYHFRATIO,
                                EAXREVERB_MAXDECAYHFRATIO);
                if (err) return err;

                reverb->flDecayHFRatio = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flDecayHFRatio;
           break;

        case EAXREVERB_DECAYLFRATIO:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINDECAYLFRATIO,
                                EAXREVERB_MAXDECAYLFRATIO);
                if (err) return err;

                reverb->flDecayLFRatio = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flDecayLFRatio;
            break;

        case EAXREVERB_REFLECTIONS:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXREVERB_MINREFLECTIONS,
                                EAXREVERB_MAXREFLECTIONS);
                if (err) return err;

                reverb->lReflections = *val.pLong;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pLong = reverb->lReflections;
            break;

     case EAXREVERB_REFLECTIONSDELAY:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINREFLECTIONSDELAY,
                                EAXREVERB_MAXREFLECTIONSDELAY);
                if (err) return err;

                reverb->flReflectionsDelay = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flReflectionsDelay;
            break;

        case EAXREVERB_REFLECTIONSPAN:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXVECTOR)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                val.pVector->x,
                                EAX_VECTOR_COMPONENT_MIN,
                                EAX_VECTOR_COMPONENT_MAX);
                if (err) return err;

                err = CheckFloatValueEAX(props->context,
                                val.pVector->y,
                                EAX_VECTOR_COMPONENT_MIN,
                                EAX_VECTOR_COMPONENT_MAX);
                if (err) return err;

                err = CheckFloatValueEAX(props->context,
                                val.pVector->z,
                                EAX_VECTOR_COMPONENT_MIN,
                                EAX_VECTOR_COMPONENT_MAX);
                if (err) return err;

                reverb->vReflectionsPan = *val.pVector;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pVector = reverb->vReflectionsPan;
            break;

        case EAXREVERB_REVERB:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXREVERB_MINREVERB,
                                EAXREVERB_MAXREVERB);
                if (err) return err;

                reverb->lReverb = *val.pLong;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pLong = reverb->lReverb;
            break;

        case EAXREVERB_REVERBDELAY:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINREVERBDELAY,
                                EAXREVERB_MAXREVERBDELAY);
                if (err) return err;

                reverb->flReverbDelay = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flReverbDelay;
            break;

        case EAXREVERB_REVERBPAN:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXVECTOR)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                val.pVector->x,
                                EAX_VECTOR_COMPONENT_MIN,
                                EAX_VECTOR_COMPONENT_MAX);
                if (err) return err;

                err = CheckFloatValueEAX(props->context,
                                val.pVector->y,
                                EAX_VECTOR_COMPONENT_MIN,
                                EAX_VECTOR_COMPONENT_MAX);
                if (err) return err;

                err = CheckFloatValueEAX(props->context,
                                val.pVector->z,
                                EAX_VECTOR_COMPONENT_MIN,
                                EAX_VECTOR_COMPONENT_MAX);
                if (err) return err;

                reverb->vReverbPan = *val.pVector;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pVector = reverb->vReverbPan;
            break;

        case EAXREVERB_ECHOTIME:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINECHOTIME,
                                EAXREVERB_MAXECHOTIME);
                if (err) return err;

                reverb->flEchoTime = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flEchoTime;
            break;

        case EAXREVERB_ECHODEPTH:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINECHODEPTH,
                                EAXREVERB_MAXECHODEPTH);
                if (err) return err;

                reverb->flEchoDepth = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flEchoDepth;
            break;

        case EAXREVERB_MODULATIONTIME:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINMODULATIONTIME,
                                EAXREVERB_MAXMODULATIONTIME);
                if (err) return err;

                reverb->flModulationTime = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flModulationTime;
            break;

        case EAXREVERB_MODULATIONDEPTH:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINMODULATIONDEPTH,
                                EAXREVERB_MAXMODULATIONDEPTH);
                if (err) return err;

                reverb->flModulationDepth = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flModulationDepth;
            break;

       case EAXREVERB_AIRABSORPTIONHF:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINAIRABSORPTIONHF,
                                EAXREVERB_MAXAIRABSORPTIONHF);
                if (err) return err;

                reverb->flAirAbsorptionHF = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flAirAbsorptionHF;
            break;

        case EAXREVERB_HFREFERENCE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINHFREFERENCE,
                                EAXREVERB_MAXHFREFERENCE);
                if (err) return err;

                reverb->flHFReference = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flHFReference;
            break;

        case EAXREVERB_LFREFERENCE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINLFREFERENCE,
                                EAXREVERB_MAXLFREFERENCE);
                if (err) return err;

                reverb->flLFReference = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flLFReference;
            break;

        case EAXREVERB_ROOMROLLOFFFACTOR:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXREVERB_MINROOMROLLOFFFACTOR,
                                EAXREVERB_MAXROOMROLLOFFFACTOR);

                reverb->flRoomRolloffFactor = *val.pFloat;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pFloat = reverb->flRoomRolloffFactor;
            break;

        case EAXREVERB_FLAGS:
        {
            ALuint eaxmask;

            if (device->EAXManager.Target < EAX30_TARGET)
                eaxmask = EAX20LISTENER_DEFAULTFLAGS;
            else
                eaxmask = (EAXREVERB_DEFAULTFLAGS       |
                           EAXREVERBFLAGS_ECHOTIMESCALE |
                           EAXREVERBFLAGS_MODULATIONTIMESCALE);

            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                //Implementar el chequeo de flags
                /*if (err) return err; //Revisar contra HW*/
                reverb->ulFlags = *val.pUlong & eaxmask;
                reverb->ulEnvironment = EAX_ENVIRONMENT_UNDEFINED;
            }
            else
                *val.pUlong = reverb->ulFlags & eaxmask;
        }
            break;
    }

    return err;
}

/* CompressorEAX function. Process the EAXSet/EAXGet AGCCompressor calls */
static inline ALenum CompressorEAX(EAXCALLPROPS *props)
{
    EAXAGCCOMPRESSORPROPERTIES *compressor;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device     = props->context->Device;
    compressor = &device->EAXEffectProps[props->slotID].Compressor;
    err        = AL_NO_ERROR;

    switch (props->property & ~EAXAGCCOMPRESSOR_DEFERRED)
    {
        case EAXAGCCOMPRESSOR_NONE:
            break;

        case EAXAGCCOMPRESSOR_ALLPARAMETERS:
        case EAXAGCCOMPRESSOR_ONOFF:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXAGCCOMPRESSORPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                      *val.pUlong,
                       EAXAGCCOMPRESSOR_MINONOFF,
                       EAXAGCCOMPRESSOR_MAXONOFF);
                if (err) return err;
                compressor->ulOnOff = *val.pUlong;
            }
            else
                *val.pUlong = compressor->ulOnOff;
            break;
    }
    return err;
}

/*AutowahEAX function. Process the EAXSet/EAXGet Autowah calls*/
static inline ALenum AutowahEAX(EAXCALLPROPS *props)
{
    EAXAUTOWAHPROPERTIES *autowah;
    LPEAXPROPS            val = {props->value};
    ALCdevice            *device;
    ALenum                err;

    device  = props->context->Device;
    autowah = &device->EAXEffectProps[props->slotID].Autowah;
    err     = AL_NO_ERROR;

    switch (props->property & ~EAXAUTOWAH_DEFERRED)
    {
        case EAXAUTOWAH_NONE:
            break;

        case EAXAUTOWAH_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXAUTOWAHPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckAutowahParamsEAX(props))
                    return err;
                memcpy(autowah, val.pAutowah, 
                    sizeof(EAXAUTOWAHPROPERTIES));
            }
            else
                memcpy(val.pAutowah, autowah,
                    sizeof(EAXAUTOWAHPROPERTIES));
            break;

        case EAXAUTOWAH_ATTACKTIME:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXAUTOWAH_MINATTACKTIME,
                                EAXAUTOWAH_MAXATTACKTIME);
                if (err) return err;

                autowah->flAttackTime = *val.pFloat;
            }
            else
                *val.pFloat = autowah->flAttackTime;
            break;

        case EAXAUTOWAH_RELEASETIME:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXAUTOWAH_MINRELEASETIME,
                                EAXAUTOWAH_MAXRELEASETIME);
                if (err) return err;

                autowah->flReleaseTime = *val.pFloat;
            }
            else
                *val.pFloat = autowah->flReleaseTime;
            break;

        case EAXAUTOWAH_RESONANCE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXAUTOWAH_MINRESONANCE,
                                EAXAUTOWAH_MAXRESONANCE);
                if (err) return err;

                autowah->lResonance = *val.pLong;
            }
            else
                *val.pLong = autowah->lResonance;
            break;

        case EAXAUTOWAH_PEAKLEVEL:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXAUTOWAH_MINPEAKLEVEL,
                                EAXAUTOWAH_MAXPEAKLEVEL);
                if (err) return err;

                autowah->lPeakLevel = *val.pLong;
            }
            else
                *val.pLong = autowah->lPeakLevel;
            break;
    }

    return err;
}

/*ChorusEAX function. Process the EAXSet/EAXGet Autowah calls*/
static inline ALenum ChorusEAX(EAXCALLPROPS *props)
{
    EAXCHORUSPROPERTIES  *chorus;
    LPEAXPROPS            val = {props->value};
    ALCdevice            *device;
    ALenum                err;

    device = props->context->Device;
    chorus = &device->EAXEffectProps[props->slotID].Chorus;
    err    = AL_NO_ERROR;

    switch (props->property & ~EAXCHORUS_DEFERRED)
    {
        case EAXCHORUS_NONE:
            break;

        case EAXCHORUS_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXCHORUSPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckChorusParamsEAX(props))
                    return err;
                memcpy(chorus, val.pChorus,
                    sizeof(EAXCHORUSPROPERTIES));
            }
            else
                memcpy(val.pChorus, chorus,
                    sizeof(EAXCHORUSPROPERTIES));
            break;

         case EAXCHORUS_WAVEFORM:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                                *val.pUlong,
                                EAXCHORUS_MINWAVEFORM,
                                EAXCHORUS_MAXWAVEFORM);
                if (err) return err;

                chorus->ulWaveform = *val.pUlong;
            }
            else
                *val.pUlong = chorus->ulWaveform;
            break;

        case EAXCHORUS_PHASE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                                *val.pLong,
                                EAXCHORUS_MINPHASE,
                                EAXCHORUS_MAXPHASE);
                if (err) return err;

                chorus->lPhase = *val.pLong;
            }
            else
                *val.pLong = chorus->lPhase;
            break;

        case EAXCHORUS_RATE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXCHORUS_MINRATE,
                                EAXCHORUS_MAXRATE);
                if (err) return err;

                chorus->flRate = *val.pFloat;
            }
            else
                *val.pFloat = chorus->flRate;
           break;

        case EAXCHORUS_DEPTH:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXCHORUS_MINDEPTH,
                                EAXCHORUS_MAXDEPTH);
                if (err) return err;

                chorus->flDepth = *val.pFloat;
            }
            else
                *val.pFloat = chorus->flDepth;
            break;

        case EAXCHORUS_FEEDBACK:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXCHORUS_MINFEEDBACK,
                                EAXCHORUS_MAXFEEDBACK);
                if (err) return err;

                chorus->flFeedback = *val.pFloat;
            }
            else
                *val.pFloat = chorus->flFeedback;
            break;

        case EAXCHORUS_DELAY:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                                *val.pFloat,
                                EAXCHORUS_MINDELAY,
                                EAXCHORUS_MAXDELAY);
                if (err) return err;

                chorus->flDelay = *val.pFloat;
            }
            else
                *val.pFloat = chorus->flDelay;
            break;
    }

    return err;
}

/* DistortionEAX function. Process the EAXSet/EAXGet Distortion calls */
static inline ALenum DistortionEAX(EAXCALLPROPS *props)
{
    EAXDISTORTIONPROPERTIES *distortion;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device     = props->context->Device;
    distortion = &device->EAXEffectProps[props->slotID].Distorsion;
    err        = AL_NO_ERROR;

    switch (props->property & ~EAXDISTORTION_DEFERRED)
    {
        case EAXDISTORTION_NONE:
            break;

        case EAXDISTORTION_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXDISTORTIONPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckDistortionParamsEAX(props))
                    return err;
                memcpy(distortion, val.pDistortion,
                     sizeof(EAXDISTORTIONPROPERTIES));
            }
            else
                memcpy(val.pDistortion, distortion,
                     sizeof(EAXDISTORTIONPROPERTIES));
            break;

        case EAXDISTORTION_EDGE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXDISTORTION_MINEDGE,
                       EAXDISTORTION_MAXEDGE);
                if (err) return err;
                distortion->flEdge = *val.pFloat;
            }
            else
                *val.pFloat = distortion->flEdge;
            break;

        case EAXDISTORTION_GAIN:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pLong,
                       EAXDISTORTION_MINGAIN,
                       EAXDISTORTION_MAXGAIN);
                if (err) return err;
                distortion->lGain = *val.pLong;
            }
            else
                *val.pLong = distortion->lGain;
            break;

        case EAXDISTORTION_LOWPASSCUTOFF:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXDISTORTION_MINLOWPASSCUTOFF,
                       EAXDISTORTION_MAXLOWPASSCUTOFF);
                if (err) return err;
                distortion->flLowPassCutOff = *val.pFloat;
            }
            else
                *val.pFloat = distortion->flLowPassCutOff;
            break;

        case EAXDISTORTION_EQCENTER:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXDISTORTION_MINEQCENTER,
                       EAXDISTORTION_MAXEQCENTER);
                if (err) return err;
                distortion->flEQCenter = *val.pFloat;
            }
            else
                *val.pFloat = distortion->flEQCenter;
            break;

        case EAXDISTORTION_EQBANDWIDTH:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXDISTORTION_MINEQBANDWIDTH,
                       EAXDISTORTION_MAXEQBANDWIDTH);
                if (err) return err;
                distortion->flEQBandwidth = *val.pFloat;
            }
            else
                *val.pFloat = distortion->flEQBandwidth;
            break;
    }
    return err;
}

/* EchoEAX function. Process the EAXSet/EAXGet Echo calls */
static inline ALenum EchoEAX(EAXCALLPROPS *props)
{
    EAXECHOPROPERTIES *echo;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device = props->context->Device;
    echo   = &device->EAXEffectProps[props->slotID].Echo;
    err    = AL_NO_ERROR;

    switch (props->property & ~EAXECHO_DEFERRED)
    {
        case EAXECHO_NONE:
            break;

        case EAXECHO_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXECHOPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckEchoParamsEAX(props))
                    return err;
                memcpy(echo, val.pEcho,
                     sizeof(EAXECHOPROPERTIES));
            }
            else
                memcpy(val.pEcho, echo,
                     sizeof(EAXECHOPROPERTIES));
            break;

        case EAXECHO_DELAY:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXECHO_MINDELAY,
                       EAXECHO_MAXDELAY);
                if (err) return err;
                echo->flDelay = *val.pFloat;
            }
            else
                *val.pFloat = echo->flDelay;
            break;

        case EAXECHO_LRDELAY:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXECHO_MINLRDELAY,
                       EAXECHO_MAXLRDELAY);
                if (err) return err;
                echo->flLRDelay = *val.pFloat;
            }
            else
                *val.pFloat = echo->flLRDelay;
            break;

        case EAXECHO_DAMPING:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXECHO_MINDAMPING,
                       EAXECHO_MAXDAMPING);
                if (err) return err;
                echo->flDamping = *val.pFloat;
            }
            else
                *val.pFloat = echo->flDamping;
            break;

        case EAXECHO_FEEDBACK:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXECHO_MINFEEDBACK,
                       EAXECHO_MAXFEEDBACK);
                if (err) return err;
                echo->flFeedback = *val.pFloat;
            }
            else
                *val.pFloat = echo->flFeedback;
            break;

        case EAXECHO_SPREAD:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXECHO_MINSPREAD,
                       EAXECHO_MAXSPREAD);
                if (err) return err;
                echo->flSpread = *val.pFloat;
            }
            else
                *val.pFloat = echo->flSpread;
            break;
    }
    return err;
}

/* EqualizerEAX function. Process the EAXSet/EAXGet Equalizer calls */
static inline ALenum EqualizerEAX(EAXCALLPROPS *props)
{
    EAXEQUALIZERPROPERTIES *equalizer;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device     = props->context->Device;
    equalizer  = &device->EAXEffectProps[props->slotID].Equalizer;
    err        = AL_NO_ERROR;

    switch (props->property & ~EAXEQUALIZER_DEFERRED)
    {
        case EAXEQUALIZER_NONE:
            break;

        case EAXEQUALIZER_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXEQUALIZERPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckEqualizerParamsEAX(props))
                    return err;
                memcpy(equalizer, val.pEqualizer,
                     sizeof(EAXEQUALIZERPROPERTIES));
            }
            else
                memcpy(val.pEqualizer, equalizer,
                     sizeof(EAXEQUALIZERPROPERTIES));
            break;

        case EAXEQUALIZER_LOWGAIN:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pLong,
                       EAXEQUALIZER_MINLOWGAIN,
                       EAXEQUALIZER_MAXLOWGAIN);
                if (err) return err;
                equalizer->lLowGain = *val.pLong;
            }
            else
                *val.pLong = equalizer->lLowGain;
            break;

        case EAXEQUALIZER_LOWCUTOFF:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXEQUALIZER_MINLOWCUTOFF,
                       EAXEQUALIZER_MAXLOWCUTOFF);
                if (err) return err;
                equalizer->flLowCutOff = *val.pFloat;
            }
            else
                *val.pFloat = equalizer->flLowCutOff;
            break;

        case EAXEQUALIZER_MID1GAIN:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pLong,
                       EAXEQUALIZER_MINMID1GAIN,
                       EAXEQUALIZER_MAXMID1GAIN);
                if (err) return err;
                equalizer->lMid1Gain = *val.pLong;
            }
            else
                *val.pLong = equalizer->lMid1Gain;
            break;

        case EAXEQUALIZER_MID1CENTER:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXEQUALIZER_MINMID1CENTER,
                       EAXEQUALIZER_MAXMID1CENTER);
                if (err) return err;
                equalizer->flMid1Center = *val.pFloat;
            }
            else
                *val.pFloat = equalizer->flMid1Center;
            break;

        case EAXEQUALIZER_MID1WIDTH:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXEQUALIZER_MINMID1WIDTH,
                       EAXEQUALIZER_MAXMID1WIDTH);
                if (err) return err;
                equalizer->flMid1Width = *val.pFloat;
            }
            else
                *val.pFloat = equalizer->flMid1Width;
            break;

        case EAXEQUALIZER_MID2GAIN:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pLong,
                       EAXEQUALIZER_MINMID2GAIN,
                       EAXEQUALIZER_MAXMID2GAIN);
                if (err) return err;
                equalizer->lMid2Gain = *val.pLong;
            }
            else
                *val.pLong = equalizer->lMid2Gain;
            break;

        case EAXEQUALIZER_MID2CENTER:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXEQUALIZER_MINMID2CENTER,
                       EAXEQUALIZER_MAXMID2CENTER);
                if (err) return err;
                equalizer->flMid2Center = *val.pFloat;
            }
            else
                *val.pFloat = equalizer->flMid2Center;
            break;


        case EAXEQUALIZER_MID2WIDTH:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXEQUALIZER_MINMID2WIDTH,
                       EAXEQUALIZER_MAXMID2WIDTH);
                if (err) return err;
                equalizer->flMid2Width = *val.pFloat;
            }
            else
                *val.pFloat = equalizer->flMid2Width;
            break;

        case EAXEQUALIZER_HIGHGAIN:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pLong,
                       EAXEQUALIZER_MINHIGHGAIN,
                       EAXEQUALIZER_MAXHIGHGAIN);
                if (err) return err;
                equalizer->lHighGain = *val.pLong;
            }
            else
                *val.pLong = equalizer->lHighGain;
            break;

        case EAXEQUALIZER_HIGHCUTOFF:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXEQUALIZER_MINHIGHCUTOFF,
                       EAXEQUALIZER_MAXHIGHCUTOFF);
                if (err) return err;
                equalizer->flHighCutOff = *val.pFloat;
            }
            else
                *val.pFloat = equalizer->flHighCutOff;
            break;
    }
    return err;
}

/* FlangerEAX function. Process the EAXSet/EAXGet Vocal Morpher calls */
static inline ALenum FlangerEAX(EAXCALLPROPS *props)
{
    EAXFLANGERPROPERTIES *flanger;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device     = props->context->Device;
    flanger    = &device->EAXEffectProps[props->slotID].Flanger;
    err        = AL_NO_ERROR;

    switch (props->property & ~EAXFLANGER_DEFERRED)
    {
        case EAXFLANGER_NONE:
            break;

        case EAXFLANGER_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXFLANGERPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckFlangerParamsEAX(props))
                    return err;
                memcpy(flanger, val.pFlanger,
                     sizeof(EAXFLANGERPROPERTIES));
            }
            else
                memcpy(val.pFlanger, flanger,
                     sizeof(EAXFLANGERPROPERTIES));
            break;

        case EAXFLANGER_WAVEFORM:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                      *val.pUlong,
                       EAXFLANGER_MINWAVEFORM,
                       EAXFLANGER_MAXWAVEFORM);
                if (err) return err;
                flanger->ulWaveform = *val.pUlong;
            }
            else
                *val.pUlong = flanger->ulWaveform;
            break;

        case EAXFLANGER_PHASE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pLong,
                       EAXFLANGER_MINPHASE,
                       EAXFLANGER_MAXPHASE);
                if (err) return err;
                flanger->lPhase = *val.pLong;
            }
            else
                *val.pLong = flanger->lPhase;
            break;

        case EAXFLANGER_RATE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXFLANGER_MINRATE,
                       EAXFLANGER_MAXRATE);
                if (err) return err;
                flanger->flRate = *val.pFloat;
            }
            else
                *val.pFloat = flanger->flRate;
            break;

        case EAXFLANGER_DEPTH:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXFLANGER_MINDEPTH,
                       EAXFLANGER_MAXDEPTH);
                if (err) return err;
                flanger->flDepth = *val.pFloat;
            }
            else
                *val.pFloat = flanger->flDepth;
            break;

        case EAXFLANGER_FEEDBACK:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXFLANGER_MINFEEDBACK,
                       EAXFLANGER_MAXFEEDBACK);
                if (err) return err;
                flanger->flFeedback = *val.pFloat;
            }
            else
                *val.pFloat = flanger->flFeedback;
            break;

        case EAXFLANGER_DELAY:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXFLANGER_MINDELAY,
                       EAXFLANGER_MAXDELAY);
                if (err) return err;
                flanger->flDelay = *val.pFloat;
            }
            else
                *val.pFloat = flanger->flDelay;
            break;
    }
    return err;
}

/* FshifterEAX function. Process the EAXSet/EAXGet Frequency Shifter calls */
static inline ALenum FshifterEAX(EAXCALLPROPS *props)
{
    EAXFREQUENCYSHIFTERPROPERTIES *fshifter;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device     = props->context->Device;
    fshifter   = &device->EAXEffectProps[props->slotID].Fshifter;
    err        = AL_NO_ERROR;

    switch (props->property & ~EAXFREQUENCYSHIFTER_DEFERRED)
    {
        case EAXFREQUENCYSHIFTER_NONE:
            break;

        case EAXFREQUENCYSHIFTER_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXFREQUENCYSHIFTERPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckFshifterParamsEAX(props))
                    return err;
                memcpy(fshifter, val.pFshifter,
                     sizeof(EAXFREQUENCYSHIFTERPROPERTIES));
            }
            else
                memcpy(val.pFshifter, fshifter,
                     sizeof(EAXFREQUENCYSHIFTERPROPERTIES));
            break;

        case EAXFREQUENCYSHIFTER_FREQUENCY:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXFREQUENCYSHIFTER_MINFREQUENCY,
                       EAXFREQUENCYSHIFTER_MAXFREQUENCY);
                if (err) return err;
                fshifter->flFrequency = *val.pFloat;
            }
            else
                *val.pFloat = fshifter->flFrequency;
            break;

        case EAXFREQUENCYSHIFTER_LEFTDIRECTION:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                      *val.pUlong,
                       EAXFREQUENCYSHIFTER_MINLEFTDIRECTION,
                       EAXFREQUENCYSHIFTER_MAXLEFTDIRECTION);
                if (err) return err;
                fshifter->ulLeftDirection = *val.pUlong;
            }
            else
                *val.pUlong = fshifter->ulLeftDirection;
            break;

        case EAXFREQUENCYSHIFTER_RIGHTDIRECTION:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                      *val.pUlong,
                       EAXFREQUENCYSHIFTER_MINRIGHTDIRECTION,
                       EAXFREQUENCYSHIFTER_MAXRIGHTDIRECTION);
                if (err) return err;
                fshifter->ulRightDirection= *val.pUlong;
            }
            else
                *val.pUlong = fshifter->ulRightDirection;
            break;
    }
    return err;
}

/* VmorpherEAX function. Process the EAXSet/EAXGet Vocal Morpher calls */
static inline ALenum VmorpherEAX(EAXCALLPROPS *props)
{
    EAXVOCALMORPHERPROPERTIES *vmorpher;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device     = props->context->Device;
    vmorpher   = &device->EAXEffectProps[props->slotID].Vmorpher;
    err        = AL_NO_ERROR;

    switch (props->property & ~EAXVOCALMORPHER_DEFERRED)
    {
        case EAXVOCALMORPHER_NONE:
            break;

        case EAXVOCALMORPHER_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXVOCALMORPHERPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckVmorpherParamsEAX(props))
                    return err;
                memcpy(vmorpher, val.pVmorpher,
                     sizeof(EAXVOCALMORPHERPROPERTIES));
            }
            else
                memcpy(val.pVmorpher, vmorpher,
                     sizeof(EAXVOCALMORPHERPROPERTIES));
            break;

        case EAXVOCALMORPHER_PHONEMEA:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                      *val.pUlong,
                       EAXVOCALMORPHER_MINPHONEMEA,
                       EAXVOCALMORPHER_MAXPHONEMEA);
                if (err) return err;
                vmorpher->ulPhonemeA = *val.pUlong;
            }
            else
                *val.pUlong = vmorpher->ulPhonemeA;
            break;

        case EAXVOCALMORPHER_PHONEMEACOARSETUNING:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pLong,
                       EAXVOCALMORPHER_MINPHONEMEACOARSETUNING,
                       EAXVOCALMORPHER_MAXPHONEMEACOARSETUNING);
                if (err) return err;
                vmorpher->lPhonemeACoarseTuning = *val.pLong;
            }
            else
                *val.pLong = vmorpher->lPhonemeACoarseTuning;
            break;

        case EAXVOCALMORPHER_PHONEMEB:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                      *val.pUlong,
                       EAXVOCALMORPHER_MINPHONEMEB,
                       EAXVOCALMORPHER_MAXPHONEMEB);
                if (err) return err;
                vmorpher->ulPhonemeB = *val.pUlong;
            }
            else
                *val.pUlong = vmorpher->ulPhonemeB;
            break;

        case EAXVOCALMORPHER_PHONEMEBCOARSETUNING:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pLong,
                       EAXVOCALMORPHER_MINPHONEMEBCOARSETUNING,
                       EAXVOCALMORPHER_MAXPHONEMEBCOARSETUNING);
                if (err) return err;
                vmorpher->lPhonemeBCoarseTuning = *val.pLong;
            }
            else
                *val.pLong = vmorpher->lPhonemeBCoarseTuning;
            break;

        case EAXVOCALMORPHER_WAVEFORM:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                      *val.pUlong,
                       EAXVOCALMORPHER_MINWAVEFORM,
                       EAXVOCALMORPHER_MAXWAVEFORM);
                if (err) return err;
                vmorpher->ulWaveform = *val.pUlong;
            }
            else
                *val.pUlong = vmorpher->ulWaveform;
            break;

        case EAXVOCALMORPHER_RATE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXVOCALMORPHER_MINRATE,
                       EAXVOCALMORPHER_MAXRATE);
                if (err) return err;
                vmorpher->flRate = *val.pFloat;
            }
            else
                *val.pFloat = vmorpher->flRate;
            break;
    }
    return err;
}

/* PshifterEAX function. Process the EAXSet/EAXGet Pitch shifter calls */
static inline ALenum PshifterEAX(EAXCALLPROPS *props)
{
    EAXPITCHSHIFTERPROPERTIES *pshifter;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device     = props->context->Device;
    pshifter   = &device->EAXEffectProps[props->slotID].Pshifter;
    err        = AL_NO_ERROR;

    switch (props->property & ~EAXPITCHSHIFTER_DEFERRED)
    {
        case EAXPITCHSHIFTER_NONE:
            break;

        case EAXPITCHSHIFTER_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXPITCHSHIFTERPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckPshifterParamsEAX(props))
                    return err;
                memcpy(pshifter, val.pPshifter,
                     sizeof(EAXPITCHSHIFTERPROPERTIES));
            }
            else
                memcpy(val.pPshifter, pshifter,
                     sizeof(EAXPITCHSHIFTERPROPERTIES));
            break;

        case EAXPITCHSHIFTER_COARSETUNE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pUlong,
                       EAXPITCHSHIFTER_MINCOARSETUNE,
                       EAXPITCHSHIFTER_MAXCOARSETUNE);
                if (err) return err;
                pshifter->lCoarseTune = *val.pLong;
            }
            else
                *val.pLong = pshifter->lCoarseTune;
            break;

        case EAXPITCHSHIFTER_FINETUNE:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(LONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckIntValueEAX(props->context,
                      *val.pUlong,
                       EAXPITCHSHIFTER_MINFINETUNE,
                       EAXPITCHSHIFTER_MAXFINETUNE);
                if (err) return err;
                pshifter->lFineTune = *val.pLong;
            }
            else
                *val.pLong = pshifter->lFineTune;
            break;
    }
    return err;
}

/* ModulatorEAX function. Process the EAXSet/EAXGet Ring Modulator calls */
static inline ALenum ModulatorEAX(EAXCALLPROPS *props)
{
    EAXRINGMODULATORPROPERTIES *modulator;
    LPEAXPROPS val = { props->value };
    ALCdevice *device;
    ALenum     err;

    device     = props->context->Device;
    modulator  = &device->EAXEffectProps[props->slotID].Modulator;
    err        = AL_NO_ERROR;

    switch (props->property & ~EAXRINGMODULATOR_DEFERRED)
    {
        case EAXRINGMODULATOR_NONE:
            break;

        case EAXRINGMODULATOR_ALLPARAMETERS:

            if (err = CheckSizeEAX(props->context, props->size, sizeof(EAXRINGMODULATORPROPERTIES)))
                return err;
            else if (props->isSet)
            {
                if(err = CheckModulatorParamsEAX(props))
                    return err;
                memcpy(modulator, val.pModulator,
                     sizeof(EAXRINGMODULATORPROPERTIES));
            }
            else
                memcpy(val.pModulator, modulator,
                     sizeof(EAXRINGMODULATORPROPERTIES));
            break;

        case EAXRINGMODULATOR_FREQUENCY:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXRINGMODULATOR_MINFREQUENCY,
                       EAXRINGMODULATOR_MAXFREQUENCY);
                if (err) return err;
                modulator->flFrequency = *val.pFloat;
            }
            else
                *val.pFloat = modulator->flFrequency;
            break;

        case EAXRINGMODULATOR_HIGHPASSCUTOFF:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ALfloat)))
                return err;
            else if (props->isSet)
            {
                err = CheckFloatValueEAX(props->context,
                      *val.pFloat,
                       EAXRINGMODULATOR_MINHIGHPASSCUTOFF,
                       EAXRINGMODULATOR_MAXHIGHPASSCUTOFF);
                if (err) return err;
                modulator->flHighPassCutOff = *val.pFloat;
            }
            else
                *val.pFloat = modulator->flHighPassCutOff;
            break;

        case EAXRINGMODULATOR_WAVEFORM:
            if (err = CheckSizeEAX(props->context, props->size, sizeof(ULONG)))
                return err;
            else if (props->isSet)
            {
                err = CheckUintValueEAX(props->context,
                      *val.pUlong,
                       EAXRINGMODULATOR_MINWAVEFORM,
                       EAXRINGMODULATOR_MAXWAVEFORM);
                if (err) return err;
                modulator->ulWaveform = *val.pUlong;
            }
            else
                *val.pUlong = modulator->ulWaveform;
            break;
    }
    return err;
}

/*EffectEAX function. Process the EAXSet/EAXGet effect calls*/
ALenum EffectEAX(EAXCALLPROPS *props)
{
    ALenum effect_type;
    ALenum eaxerr;

    effect_type = props->context->Device->EAXhw.Slots[props->slotID].EffType;

    switch (effect_type)
    {
        case _EAX_NULL_GUID:

            ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_NO_EFFECT_LOADED);
            return AL_INVALID_OPERATION;

        case _EAX_REVERB_EFFECT:
            if ((props->property & ~EAXREVERB_DEFERRED) >= REVERBLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                ReverbEAX(props);
            break;

        case _EAX_AGCCOMPRESSOR_EFFECT:
            if ((props->property & ~EAXAGCCOMPRESSOR_DEFERRED) >= AGCLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                CompressorEAX(props);
            break;

        case _EAX_AUTOWAH_EFFECT:
            if (props->property >= AUTOWAHLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                AutowahEAX(props);
            break;

        case _EAX_CHORUS_EFFECT:
            if (props->property >= CHORUSLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                ChorusEAX(props);
            break;

        case _EAX_DISTORTION_EFFECT:
            if (props->property >= DISTORSIONLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                DistortionEAX(props);
            break;

        case _EAX_ECHO_EFFECT:
            if (props->property >= ECHOLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                EchoEAX(props);
            break;

        case _EAX_EQUALIZER_EFFECT:
            if (props->property >= EQUALIZERLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                EqualizerEAX(props);
            break;

        case _EAX_FLANGER_EFFECT:
            if (props->property >= FLANGERLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                FlangerEAX(props);
            break;

        case _EAX_FREQUENCYSHIFTER_EFFECT:
            if (props->property >= FSHIFTERLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                FshifterEAX(props);
            break;

        case _EAX_VOCALMORPHER_EFFECT:
            if (props->property >= VMORPHERLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                VmorpherEAX(props);
            break;

        case _EAX_PITCHSHIFTER_EFFECT:
            if (props->property >= PSHIFTERLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                PshifterEAX(props);
            break;

        case _EAX_RINGMODULATOR_EFFECT:
            if (props->property >= MODULATORLIST_SIZE)
            {
                TRACE("Invalid effect property %u.\n", props->property);
                ATOMIC_COMPARE_EXCHANGE_STRONG_SEQ(&props->context->EAXLastError, &eaxerr, EAXERR_INVALID_OPERATION);
                return AL_INVALID_ENUM;
            }
            else
                ModulatorEAX(props);
           break;
    }

    return AL_NO_ERROR;
}
