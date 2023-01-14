/**
 * OpenAL cross platform audio library
 * Copyright (C) 2018 by Raul Herraiz.
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
#include <math.h>
#include <stdlib.h>

#include "alMain.h"
#include "alAuxEffectSlot.h"
#include "alError.h"
#include "alu.h"
#include "filters/defs.h"

#define MAX_FORMANTS 5

#define WAVEFORM_FRACBITS  24
#define WAVEFORM_FRACONE   (1<<WAVEFORM_FRACBITS)
#define WAVEFORM_FRACMASK  (WAVEFORM_FRACONE-1)

#define PHONEME_A  {{650.0f,1100.0f,2650.0f,2950.0f,15000.0f},       \
                   {0.316227f,0.501187f,1.0f,1.0f,0.0158f},          \
                   {0.087482f,0.103442f,0.067403f,0.060540f,1.0f}}

#define PHONEME_E  {{400.0f,1700.0f,2600.0f,3250.0f,15000.0f},       \
                   {0.223872f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.108252f,0.059413f,0.055495f,0.044394f,1.0f}}

#define PHONEME_I  {{300.0f,1850.0f,2800.0f,3250.0f,15000.0f},       \
                   {0.316227f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.096215f,0.054594f,0.051530f,0.044394f,1.0f}}

#define PHONEME_O  {{400.0f,800.0f,2600.0f,2850.0f,15000.0f},        \
                   {0.316227f,0.316227f,1.0f,1.0f,0.0158f},          \
                   {0.108252f,0.090197f,0.044394f,0.040499f,1.0f}}

#define PHONEME_U  {{350.0f,600.0f,2700.0f,2950.0f,15000.0f},        \
                   {0.316227f,0.316227f,1.0f,1.0f,0.0158f},          \
                   {0.082462f,0.096215f,0.042749f,0.039126f,1.0f}}

#define PHONEME_AA {{730.0f,1100.0f,2450.0f,3400.0f,15000.0f},       \
                   {0.446683f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.079071f,0.052467f,0.042400f,0.042435f,1.0f}}

#define PHONEME_AE {{650.0f,1730.0f,2450.0f,3350.0f,15000.0f},       \
                   {0.316227f,0.707106f,0.562341f,1.0f,0.0158f},     \
                   {0.088809f,0.050040f,0.047112f,0.034454f,1.0f}}

#define PHONEME_AH {{650.0f,1200.0f,2400.0f,3000.0f,15000.0f},       \
                   {0.446683f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.088809f,0.060120f,0.048094f,0.038474f,1.0f}}

#define PHONEME_AO {{570.0f,850.0f,2420.0f,3450.0f,15000.0f},        \
                   {0.316227f,0.562341f,1.0f,1.0f,0.0158f},          \
                   {0.075948f,0.067904f,0.041733f,0.041820f,1.0f}}

#define PHONEME_EH {{530.0f,1850.0f,2480.0f,3300.0f,15000.0f},       \
                   {0.316227f,1.0f,0.707106f,0.707106f,0.0158f},     \
                   {0.081683f,0.046794f,0.046542f,0.043721f,1.0f}}

#define PHONEME_ER {{500.0f,1350.0f,1700.0f,3400.0f,15000.0f},       \
                   {0.316227f,0.707106f,1.0f,0.707106f,0.0158f},     \
                   {0.057715f,0.042749f,0.033947f,0.033947f,1.0f}}

#define PHONEME_IH {{385.0f,2000.0f,2550.0f,3650.0f,15000.0f},       \
                   {0.316227f,1.0f,0.707106f,1.0f,0.0158f},          \
                   {0.093714f,0.050499f,0.045264f,0.047435f,1.0f}}

#define PHONEME_IY {{270.0f,2300.0f,3050.0f,3800.0f,15000.0f},       \
                   {0.223872f,1.0f,0.707106f,1.0f,0.0158f},          \
                   {0.106915f,0.047048f,0.037843f,0.045562f,1.0f}}

#define PHONEME_UH {{440.0f,1025.0f,2250.0f,3275.0f,15000.0f},       \
                   {0.316227f,0.562341f,1.0f,0.707106f,0.0158f},     \
                   {0.081993f,0.056307f,0.038474f,0.044055f,1.0f}}

#define PHONEME_UW {{300.0f,870.0f,2250.0f,3100.0f,15000.0f},        \
                   {0.223872f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.096215f,0.058047f,0.038474f,0.046542f,1.0f}}

#define PHONEME_B  {{700.0f,1500.0f,2530.0f,3500.0f,15000.0f},       \
                   {0.562341f,0.354813f,1.0f,0.707106f,0.0158f},     \
                   {0.082462f,0.154033f,0.091268f,0.114106f,1.0f}}

#define PHONEME_D  {{200.0f,1500.0f,2500.0f,2970.0f,15000.0f},       \
                   {0.223872f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.144389f,0.052904f,0.046170f,0.038862f,1.0f}}

#define PHONEME_F  {{400.0f,1150.0f,2050.0f,2650.0f,15000.0f},       \
                   {0.223872f,0.707106f,0.354813f,1.0f,0.0158f},     \
                   {0.108252f,0.062735f,0.098563f,0.043556f,1.0f}}

#define PHONEME_G  {{280.0f,2175.0f,2225.0f,3450.0f,15000.0f},       \
                   {0.177827f,1.0f,1.0f,0.707106f,0.0158f},          \
                   {0.123735f,0.046435f,0.045391f,0.041820f,1.0f}}

#define PHONEME_J  {{280.0f,1650.0f,2520.0f,3020.0f,15000.0f},       \
                   {0.223872f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.113412f,0.061214f,0.045803f,0.042997f,1.0f}}

#define PHONEME_K  {{400.0f,1700.0f,1800.0f,3050.0f,15000.0f},       \
                   {0.223872f,1.0f,0.707106f,0.707106f,0.0158f},     \
                   {0.108252f,0.055168f,0.052102f,0.044940f,1.0f}}

#define PHONEME_L  {{300.0f,1200.0f,2300.0f,2950.0f,15000.0f},       \
                   {0.177827f,0.562341f,1.0f,0.398107f,0.01f},       \
                   {0.067337f,0.030057f,0.018818f,0.041572f,1.0f}}

#define PHONEME_M  {{250.0f,1200.0f,2200.0f,2800.0f,15000.0f},       \
                   {0.446683f,0.251188f,0.562341f,1.0f,0.0158f},     \
                   {0.115477f,0.156445f,0.104969f,0.066994f,1.0f}}

#define PHONEME_N  {{200.0f,1450.0f,2070.0f,2550.0f,15000.0f},       \
                   {0.223872f,0.398107f,1.0f,1.0f,0.0158f},          \
                   {0.144389f,0.129431f,0.048791f,0.073565f,1.0f}}

#define PHONEME_P  {{200.0f,970.0f,1950.0f,2500.0f,15000.0f},        \
                   {0.223872f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.144389f,0.074382f,0.066597f,0.069262f,1.0f}}

#define PHONEME_R  {{400.0f,1350.0f,2000.0f,2430.0f,15000.0f},       \
                   {0.446683f,0.707106f,1.0f,1.0f,0.0158f},          \
                   {0.090197f,0.064130f,0.061323f,0.071258f,1.0f}}

#define PHONEME_S  {{200.0f,1600.0f,2620.0f,3650.0f,15000.0f},       \
                   {0.177827f,0.562341f,1.0f,1.0f,0.0158f},          \
                   {0.144389f,0.063127f,0.046809f,0.047435f,1.0f}}

#define PHONEME_T  {{200.0f,2300.0f,3030.0f,3850.0f,15000.0f},       \
                   {0.125892f,1.0f,0.562341f,1.0f,0.0158f},          \
                   {0.144389f,0.043911f,0.047618f,0.052467f,1.0f}}

#define PHONEME_V  {{380.0f,1100.0f,2300.0f,3150.0f,15000.0f},       \
                   {0.177827f,0.446683f,0.707106f,1.0f,0.0158f},     \
                   {0.190102f,0.104969f,0.075288f,0.064130f,1.0f}}

#define PHONEME_Z  {{350.0f,1625.0f,2520.0f,3850.0f,15000.0f},       \
                   {0.177827f,0.446683f,1.0f,1.0f,0.0158f},          \
                   {0.144389f,0.106585f,0.051530f,0.052467f,1.0f}}

typedef struct Phoneme{
    ALfloat Formant_Freq[MAX_FORMANTS];
    ALfloat Formant_Gain[MAX_FORMANTS];
    ALfloat Formant_BW[MAX_FORMANTS];
}Phoneme;

const Phoneme Phoneme_list[30] ={
                           PHONEME_A, PHONEME_E, PHONEME_I, PHONEME_O, PHONEME_U,
                           PHONEME_AA,PHONEME_AE,PHONEME_AH,PHONEME_AO,PHONEME_EH,
                           PHONEME_ER,PHONEME_IH,PHONEME_IY,PHONEME_UH,PHONEME_UW,
                           PHONEME_B, PHONEME_D, PHONEME_F, PHONEME_G, PHONEME_J,
                           PHONEME_J, PHONEME_L, PHONEME_M, PHONEME_N, PHONEME_P,
                           PHONEME_R, PHONEME_S, PHONEME_T, PHONEME_V, PHONEME_Z};

typedef struct ALvmorpherState {
    DERIVE_FROM_TYPE(ALeffectState);

    void (*GetSamples)(ALfloat *restrict, ALsizei, const ALsizei, ALsizei);

    ALsizei index;
    ALsizei step;

    ALfloat frequency;
    /* Effect parameters */
    Phoneme Phoneme_A, Phoneme_B;

    struct {
        /* Effect gains for each output channel */
        ALfloat CurrentGains[MAX_OUTPUT_CHANNELS];
        ALfloat TargetGains[MAX_OUTPUT_CHANNELS];

        /* Effect filters */
        BiquadFilter filter[MAX_FORMANTS];
    }Chans[MAX_EFFECT_CHANNELS];

    /*Effects buffers*/ 
    alignas(16) ALfloat BufferOut[MAX_EFFECT_CHANNELS][MAX_FORMANTS][BUFFERSIZE];
} ALvmorpherState;

static ALvoid ALvmorpherState_Destruct(ALvmorpherState *state);
static ALboolean ALvmorpherState_deviceUpdate(ALvmorpherState *state, ALCdevice *device);
static ALvoid ALvmorpherState_update(ALvmorpherState *state, const ALCcontext *context, const ALeffectslot *slot, const ALeffectProps *props);
static ALvoid ALvmorpherState_process(ALvmorpherState *state, ALsizei SamplesToDo, const ALfloat (*restrict SamplesIn)[BUFFERSIZE], ALfloat (*restrict SamplesOut)[BUFFERSIZE], ALsizei NumChannels);
DECLARE_DEFAULT_ALLOCATORS(ALvmorpherState)

DEFINE_ALEFFECTSTATE_VTABLE(ALvmorpherState);

static inline ALfloat Sin(ALsizei index)
{
    return sinf((ALfloat)index * (F_TAU / WAVEFORM_FRACONE));
}

static inline ALfloat Triangle(ALsizei index)
{
    return -2.0f*fabsf((ALfloat)(index + (WAVEFORM_FRACONE>>2) & WAVEFORM_FRACMASK)*
                       (2.0f/WAVEFORM_FRACONE) - 1.0f) + 1.0f;
}

static inline ALfloat Saw(ALsizei index)
{
    return (ALfloat)index*(-2.0f/WAVEFORM_FRACONE) + 1.0f;
}

#define DECL_TEMPLATE(func)                                                   \
static void Modulate##func(ALfloat *restrict dst, ALsizei index,              \
                           const ALsizei step, ALsizei todo)                  \
{                                                                             \
    ALsizei i;                                                                \
    for(i = 0;i < todo;i++)                                                   \
    {                                                                         \
        index += step;                                                        \
        index &= WAVEFORM_FRACMASK;                                           \
        dst[i] = func(index);                                                 \
    }                                                                         \
}

DECL_TEMPLATE(Sin)
DECL_TEMPLATE(Triangle)
DECL_TEMPLATE(Saw)

#undef DECL_TEMPLATE

static inline void Phoneme_lerp(Phoneme A, Phoneme B, ALfloat mu, Phoneme *result)
{
    ALsizei i;

    for(i = 0; i < MAX_FORMANTS; i++)
    {
        result->Formant_Gain[i] = lerp(A.Formant_Gain[i]+B.Formant_Gain[i],
                                        2.0f*A.Formant_Gain[i],mu)/2.0f;
        result->Formant_Freq[i] = lerp(A.Formant_Freq[i]+B.Formant_Freq[i],
                                        2.0f*A.Formant_Freq[i], mu)/2.0f;
        result->Formant_BW[i]   = lerp(A.Formant_BW[i]+B.Formant_BW[i],
                                        2.0f*A.Formant_BW[i], mu)/2.0f;
    }
}

static inline void Phoneme_pitch(Phoneme *phoneme, ALint semitones)
{
    ALfloat tuning, BW_2, inv;
    ALsizei i;

    tuning = powf(2.0f,(ALfloat)semitones/12.0f);

    for(i = 0; i < MAX_FORMANTS-1; i++)
    {
        inv  = powf(2.0f,phoneme->Formant_BW[i]);
        BW_2 = phoneme->Formant_Freq[i]*((inv-1)/(inv+1));

        phoneme->Formant_Freq[i] *= tuning;
        phoneme->Formant_BW[i]    = log2f((phoneme->Formant_Freq[i]+BW_2)/
                                          (phoneme->Formant_Freq[i]-BW_2));
    }
}

static void ALvmorpherState_Construct(ALvmorpherState *state)
{
    ALeffectState_Construct(STATIC_CAST(ALeffectState, state));
    SET_VTABLE2(ALvmorpherState, ALeffectState, state);

    state->index = 0;
    state->step = 1;
}

static ALvoid ALvmorpherState_Destruct(ALvmorpherState *state)
{
    ALeffectState_Destruct(STATIC_CAST(ALeffectState,state));
}

static ALboolean ALvmorpherState_deviceUpdate(ALvmorpherState *state, ALCdevice *device)
{
    /* (Re-)initializing parameters and clear the buffers. */
    ALsizei i,j;

    state->frequency = (ALfloat)device->Frequency;

    for(i = 0;i < MAX_EFFECT_CHANNELS;i++)
    {
        for(j = 0;j < MAX_FORMANTS;j++)
            BiquadFilter_clear(&state->Chans[i].filter[j]);

        for(j = 0;j < MAX_OUTPUT_CHANNELS;j++)
            state->Chans[i].CurrentGains[j] = 0.0f;
    }

    return AL_TRUE;
}

static ALvoid ALvmorpherState_update(ALvmorpherState *state, const ALCcontext *context, const ALeffectslot *slot, const ALeffectProps *props)
{
    const ALCdevice *device = context->Device;
    ALuint i;

    state->step = fastf2i(props->Vmorpher.Rate / (ALfloat)device->Frequency *
                          WAVEFORM_FRACONE);
    state->step = clampi(state->step, 0, WAVEFORM_FRACONE-1);

    if(props->Vmorpher.Waveform == AL_VOCAL_MORPHER_WAVEFORM_SINUSOID)
        state->GetSamples = ModulateSin;
    else if(props->Vmorpher.Waveform == AL_VOCAL_MORPHER_WAVEFORM_TRIANGLE)
        state->GetSamples = ModulateTriangle;
    else /*if(props->Vmorpher.Waveform == AL_VOCAL_MORPHER_WAVEFORM_SAWTOOTH)*/
        state->GetSamples = ModulateSaw;

    state->Phoneme_A = Phoneme_list[props->Vmorpher.PhonemeA];
    state->Phoneme_B = Phoneme_list[props->Vmorpher.PhonemeB];

    Phoneme_pitch(&state->Phoneme_A, props->Vmorpher.PhonemeACoarse);
    Phoneme_pitch(&state->Phoneme_B, props->Vmorpher.PhonemeBCoarse);

    STATIC_CAST(ALeffectState,state)->OutBuffer = device->FOAOut.Buffer;
    STATIC_CAST(ALeffectState,state)->OutChannels = device->FOAOut.NumChannels;
    for(i = 0;i < MAX_EFFECT_CHANNELS;i++)
        ComputePanGains(&device->FOAOut, IdentityMatrixf.m[i],
                               slot->Params.Gain, state->Chans[i].TargetGains);
}

static ALvoid ALvmorpherState_process(ALvmorpherState *state, ALsizei SamplesToDo, const ALfloat (*restrict SamplesIn)[BUFFERSIZE], ALfloat (*restrict SamplesOut)[BUFFERSIZE], ALsizei NumChannels)
{
    ALfloat (*restrict BufferOut)[MAX_FORMANTS][BUFFERSIZE] = state->BufferOut;
    ALsizei c,i,j;
    const ALsizei step = state->step;
    alignas(16) ALfloat Out[MAX_EFFECT_CHANNELS][BUFFERSIZE]={0.0f};
    alignas(16) ALfloat modsamples[BUFFERSIZE];

    state->GetSamples(modsamples, state->index, step, SamplesToDo);
    state->index += (step*SamplesToDo) & WAVEFORM_FRACMASK;
    state->index &= WAVEFORM_FRACMASK;

    for(c = 0;c < MAX_EFFECT_CHANNELS; c++)
    {
        for(i = 0; i < SamplesToDo; i++)
        {
            for(j = 0; j < MAX_FORMANTS; j++)
            {
                ALfloat gain, f0norm, BW;
                Phoneme result;

                Phoneme_lerp(state->Phoneme_A, state->Phoneme_B, modsamples[i], &result);

                gain   = result.Formant_Gain[j];
                f0norm = result.Formant_Freq[j]/state->frequency;
                BW     = result.Formant_BW[j];

                BiquadFilter_setParams(&state->Chans[c].filter[j], BiquadType_BandPassPeakGain,
                    gain, f0norm, calc_rcpQ_from_bandwidth(f0norm, BW));

                BiquadFilter_process(&state->Chans[c].filter[j], &BufferOut[c][j][i], &SamplesIn[c][i], 1);
        }

        for(j = 0;j < MAX_FORMANTS; j++)
            Out[c][i] += 3.162277f*BufferOut[c][j][i];/*+10dB Boost*/
        }

    /* Now, mix the processed sound data to the output. */
        MixSamples(Out[c], NumChannels, SamplesOut, state->Chans[c].CurrentGains,
                   state->Chans[c].TargetGains, SamplesToDo, 0, SamplesToDo);
    }

}

typedef struct VmorpherStateFactory {
    DERIVE_FROM_TYPE(EffectStateFactory);
} VmorpherStateFactory;

static ALeffectState *VmorpherStateFactory_create(VmorpherStateFactory *UNUSED(factory))
{
    ALvmorpherState *state;

    NEW_OBJ0(state, ALvmorpherState)();
    if(!state) return NULL;

    return STATIC_CAST(ALeffectState, state);
}

DEFINE_EFFECTSTATEFACTORY_VTABLE(VmorpherStateFactory);

EffectStateFactory *VmorpherStateFactory_getFactory(void)
{
    static VmorpherStateFactory VmorpherFactory = { { GET_VTABLE2(VmorpherStateFactory, EffectStateFactory) } };

    return STATIC_CAST(EffectStateFactory, &VmorpherFactory);
}

void ALvmorpher_setParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat val)
{
    ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_VOCAL_MORPHER_RATE:
            if(!(val >= AL_VOCAL_MORPHER_MIN_RATE && val <= AL_VOCAL_MORPHER_MAX_RATE))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Vocal morpher rate out of range");
            props->Vmorpher.Rate = val;
            break;

        default:
            alSetError(context, AL_INVALID_ENUM, "Invalid vocal morpher float property 0x%04x", param);
    }
}

void ALvmorpher_setParamfv(ALeffect *effect, ALCcontext *context, ALenum param, const ALfloat *vals)
{
    ALvmorpher_setParamf(effect, context, param, vals[0]);
}

void ALvmorpher_setParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint val)
{
    ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_VOCAL_MORPHER_PHONEMEA:
            if(!(val >= AL_VOCAL_MORPHER_MIN_PHONEMEA && val <= AL_VOCAL_MORPHER_MAX_PHONEMEA))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Vocal morpher phoneme A out of range");
            props->Vmorpher.PhonemeA = val;
            break;

        case AL_VOCAL_MORPHER_PHONEMEA_COARSE_TUNING:
            if(!(val >= AL_VOCAL_MORPHER_MIN_PHONEMEA_COARSE_TUNING && val <= AL_VOCAL_MORPHER_MAX_PHONEMEA_COARSE_TUNING))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Vocal morpher phoneme A coarse tunning out of range");
            props->Vmorpher.PhonemeACoarse = val;
            break;

        case AL_VOCAL_MORPHER_PHONEMEB:
            if(!(val >= AL_VOCAL_MORPHER_MIN_PHONEMEB && val <= AL_VOCAL_MORPHER_MAX_PHONEMEB))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Vocal morpher phoneme B out of range");
            props->Vmorpher.PhonemeB = val;
            break;

        case AL_VOCAL_MORPHER_PHONEMEB_COARSE_TUNING:
            if(!(val >= AL_VOCAL_MORPHER_MIN_PHONEMEB_COARSE_TUNING && val <= AL_VOCAL_MORPHER_MAX_PHONEMEB_COARSE_TUNING))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Vocal morpher phoneme B coarse tunning out of range");
            props->Vmorpher.PhonemeBCoarse = val;
            break;

        case AL_VOCAL_MORPHER_WAVEFORM:
            if(!(val >= AL_VOCAL_MORPHER_MIN_WAVEFORM && val <= AL_VOCAL_MORPHER_MAX_WAVEFORM))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Invalid vocal morpher waveform");
            props->Vmorpher.Waveform = val;
            break;

        default:
            alSetError(context, AL_INVALID_ENUM, "Invalid vocal morpher integer property 0x%04x", param);
    }
}

void ALvmorpher_setParamiv(ALeffect *effect, ALCcontext *context, ALenum param, const ALint *vals)
{
    ALvmorpher_setParami(effect, context, param, vals[0]);
}

void ALvmorpher_getParami(const ALeffect *effect, ALCcontext *context, ALenum param, ALint *val)
{
    const ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_VOCAL_MORPHER_PHONEMEA:
            *val = props->Vmorpher.PhonemeA;
            break;
        case AL_VOCAL_MORPHER_PHONEMEA_COARSE_TUNING:
            *val = props->Vmorpher.PhonemeACoarse;
            break;
        case AL_VOCAL_MORPHER_PHONEMEB:
            *val = props->Vmorpher.PhonemeB;
            break;
        case AL_VOCAL_MORPHER_PHONEMEB_COARSE_TUNING:
            *val = props->Vmorpher.PhonemeBCoarse;
            break;
        case AL_VOCAL_MORPHER_WAVEFORM:
            *val = props->Vmorpher.Waveform;
            break;
        default:
            alSetError(context, AL_INVALID_ENUM, "Invalid vocal morpher integer property 0x%04x", param);
    }
}
void ALvmorpher_getParamiv(const ALeffect *effect, ALCcontext *context, ALenum param, ALint *vals)
{
    ALvmorpher_getParami(effect, context, param, vals);
}

void ALvmorpher_getParamf(const ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *val)
{

    const ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_VOCAL_MORPHER_RATE:
            *val = props->Vmorpher.Rate;
            break;

        default:
            alSetError(context, AL_INVALID_ENUM, "Invalid vocal morpher float property 0x%04x", param);
    }

}

void ALvmorpher_getParamfv(const ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *vals)
{
    ALvmorpher_getParamf(effect, context, param, vals);
}

DEFINE_ALEFFECT_VTABLE(ALvmorpher);
