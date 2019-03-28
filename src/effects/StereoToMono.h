/**********************************************************************

  Audacity: A Digital Audio Editor

  StereoToMono.h

  Lynn Allan

**********************************************************************/

#ifndef __AUDACITY_EFFECT_STEREO_TO_MONO__
#define __AUDACITY_EFFECT_STEREO_TO_MONO__

#include "Effect.h"

class EffectStereoToMono final : public Effect
{
public:
   static const ComponentInterfaceSymbol Symbol;

   EffectStereoToMono();
   virtual ~EffectStereoToMono();

   // ComponentInterface implementation

   ComponentInterfaceSymbol GetSymbol() override;
   wxString GetDescription() override;

   // EffectDefinitionInterface implementation

   EffectType GetType() override;
   bool IsInteractive() override;

   // EffectClientInterface implementation

   unsigned GetAudioInCount() override;
   unsigned GetAudioOutCount() override;

   // Effect implementation

   bool Process() override;
   bool IsHidden() override;

private:
   // EffectStereoToMono implementation

   bool ProcessOne(int count);

private:
   sampleCount mStart;
   sampleCount mEnd;
   WaveTrack *mLeftTrack;
   WaveTrack *mRightTrack;
};

#endif

