/**********************************************************************

  Audacity: A Digital Audio Editor

  Invert.cpp

  Mark Phillips


*******************************************************************//**

\class EffectInvert
\brief An Effect that inverts the selected audio.

*//*******************************************************************/

#include "../Audacity.h"
#include "Invert.h"

#include <wx/intl.h>

const ComponentInterfaceSymbol EffectInvert::Symbol
{ XO("Invert") };

EffectInvert::EffectInvert()
{
}

EffectInvert::~EffectInvert()
{
}

// ComponentInterface implementation

ComponentInterfaceSymbol EffectInvert::GetSymbol()
{
   return Symbol;
}

wxString EffectInvert::GetDescription()
{
   return _("Flips the audio samples upside-down, reversing their polarity");
}

// EffectDefinitionInterface implementation

EffectType EffectInvert::GetType()
{
   return EffectTypeProcess;
}

bool EffectInvert::IsInteractive()
{
   return false;
}

// EffectClientInterface implementation

unsigned EffectInvert::GetAudioInCount()
{
   return 1;
}

unsigned EffectInvert::GetAudioOutCount()
{
   return 1;
}

size_t EffectInvert::ProcessBlock(float **inBlock, float **outBlock, size_t blockLen)
{
   float *ibuf = inBlock[0];
   float *obuf = outBlock[0];

   for (decltype(blockLen) i = 0; i < blockLen; i++)
   {
      obuf[i] = -ibuf[i];
   }

   return blockLen;
}
