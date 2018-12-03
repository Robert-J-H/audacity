/**********************************************************************

Audacity: A Digital Audio Editor

WaveTrackViewGroupData.h

Paul Licameli split from class WaveTrack

**********************************************************************/

#ifndef __AUDACITY_WAVE_TRACK_VIEW_GROUP_DATA__
#define __AUDACITY_WAVE_TRACK_VIEW_GROUP_DATA__

#include "../../../ui/TrackViewGroupData.h"
#include "WaveTrackViewConstants.h"

class SpectrogramSettings;
class WaveformSettings;
class WaveTrack;

class WaveTrackViewGroupData final : public TrackViewGroupData
{
public:
   WaveTrackViewGroupData( TrackGroupData & );
   WaveTrackViewGroupData( const WaveTrackViewGroupData & );
   ~WaveTrackViewGroupData() override;
   PointerType Clone() const override;

   static const WaveTrackViewGroupData &Get( const WaveTrack& );
   static WaveTrackViewGroupData &Get( WaveTrack& );

   const SpectrogramSettings &GetSpectrogramSettings() const;
   SpectrogramSettings &GetSpectrogramSettings();
   SpectrogramSettings &GetIndependentSpectrogramSettings();
   void SetSpectrogramSettings(std::unique_ptr<SpectrogramSettings> &&pSettings);
   
   const WaveformSettings &GetWaveformSettings() const;
   WaveformSettings &GetWaveformSettings();
   WaveformSettings &GetIndependentWaveformSettings();
   void SetWaveformSettings(std::unique_ptr<WaveformSettings> &&pSettings);
   void UseSpectralPrefs( bool bUse=true );

   int GetLastScaleType() const { return mLastScaleType; }
   void SetLastScaleType() const;

   int GetLastdBRange() const { return mLastdBRange; }
   void SetLastdBRange() const;

   using TrackDisplay = WaveTrackViewConstants::Display;

   TrackDisplay GetDisplay() const { return mDisplay; }
   void SetDisplay(TrackDisplay display) { mDisplay = display; }

   void GetDisplayBounds(float *min, float *max) const;
   void SetDisplayBounds(float min, float max) const;
   void GetSpectrumBounds(double rate, float *min, float *max) const;
   void SetSpectrumBounds(float min, float max);

   // For display purposes, calculate the y coordinate where the midline of
   // the wave should be drawn, if display minimum and maximum map to the
   // bottom and top.  Maybe that is out of bounds.
   int ZeroLevelYCoordinate(wxRect rect) const;

private:
   mutable float         mDisplayMin{ -1.0 };
   mutable float         mDisplayMax{  1.0 };
   float         mSpectrumMin{ -1 };
   float         mSpectrumMax{ -1 };

   TrackDisplay mDisplay;

   mutable int   mLastScaleType{ -1 }; // last scale type choice
   mutable int   mLastdBRange{ -1 };

   std::unique_ptr<SpectrogramSettings> mpSpectrumSettings;
   std::unique_ptr<WaveformSettings> mpWaveformSettings;
};

#endif
