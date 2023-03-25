#ifndef EAXEFFECTS_H
#define EAXEFFECTS_H

#include "alMain.h"

ALenum EffectEAX(EAXCALLPROPS *props);
ALenum MapEffectEAX(ALenum eEffect);
ALenum CopyDefPropsEffectEAX(EAXEFFECTPROPS *props, ALenum EffType);
ALenum ApplyEffectParamsEAX(EAXCALLPROPS *props);

#endif /* EAXEFFECTS_H */
