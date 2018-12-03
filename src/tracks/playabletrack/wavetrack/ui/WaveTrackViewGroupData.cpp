/**********************************************************************

Audacity: A Digital Audio Editor

WaveTrackViewGroupData.cpp

Paul Licameli split from class WaveTrack

**********************************************************************/

#include "WaveTrackViewGroupData.h"

#include "../../../../WaveTrack.h"
#include "../../../../prefs/SpectrumPrefs.h"
#include "../../../../prefs/TracksPrefs.h"
#include "../../../../prefs/WaveformPrefs.h"

WaveTrackViewGroupData::WaveTrackViewGroupData( TrackGroupData &host )
   : TrackViewGroupData( host )
{
   mDisplay = TracksPrefs::ViewModeChoice();
   
   // Force creation always:
   auto &settings = GetIndependentWaveformSettings();
   if (mDisplay == WaveTrackViewConstants::obsoleteWaveformDBDisplay)
   {
      mDisplay = WaveTrackViewConstants::Waveform;
      settings.scaleType = WaveformSettings::stLogarithmic;
   }
}

WaveTrackViewGroupData::WaveTrackViewGroupData(
   const WaveTrackViewGroupData &other )
   : TrackViewGroupData( other )
   , mDisplayMin{ -1.0 } // not copied
   , mDisplayMax{ 1.0 } // not copied
   , mSpectrumMin{ other.mSpectrumMin }
   , mSpectrumMax{ other.mSpectrumMax }
   , mDisplay{ other.mDisplay }
   , mLastScaleType{ other.mLastScaleType }
   , mLastdBRange{ other.mLastdBRange }
   , mpSpectrumSettings{ other.mpSpectrumSettings
      ? std::make_unique<SpectrogramSettings>( *other.mpSpectrumSettings )
      : nullptr }
   , mpWaveformSettings{ other.mpWaveformSettings
      ? std::make_unique<WaveformSettings>( *other.mpWaveformSettings )
      : nullptr }
{
}

WaveTrackViewGroupData::~WaveTrackViewGroupData()
{
}

auto WaveTrackViewGroupData::Clone() const -> PointerType
{
   return std::make_unique<WaveTrackViewGroupData>( *this );
}

// override the view group data factory
using CreateWaveViewGroupData =
   CreateViewGroupData::Override< WaveTrack::GroupData >;
template<> template<>
auto CreateWaveViewGroupData::Implementation() -> Function {
   return []( WaveTrack::GroupData &host ) {
      return std::make_unique< WaveTrackViewGroupData >( host );
   };
}
static CreateWaveViewGroupData registerMe;

WaveTrackViewGroupData &WaveTrackViewGroupData::Get( WaveTrack &track )
{
   return static_cast< WaveTrackViewGroupData& >(
      TrackViewGroupData::Get( track )
   );
}

const WaveTrackViewGroupData
&WaveTrackViewGroupData::Get( const WaveTrack &track )
{
   // May create the data structure on demend but not change it if present
   return Get( const_cast< WaveTrack& >( track ) );
}

void WaveTrackViewGroupData::SetLastScaleType() const
{
   mLastScaleType = GetWaveformSettings().scaleType;
}

void WaveTrackViewGroupData::SetLastdBRange() const
{
   mLastdBRange = GetWaveformSettings().dBRange;
}

void WaveTrackViewGroupData::GetDisplayBounds(float *min, float *max) const
{
   *min = mDisplayMin;
   *max = mDisplayMax;
}

void WaveTrackViewGroupData::SetDisplayBounds(float min, float max) const
{
   mDisplayMin = min;
   mDisplayMax = max;
}

void WaveTrackViewGroupData::GetSpectrumBounds(
   double rate, float *min, float *max) const
{
   const SpectrogramSettings &settings = GetSpectrogramSettings();
   const SpectrogramSettings::ScaleType type = settings.scaleType;

   const float top = (rate / 2.);

   float bottom;
   if (type == SpectrogramSettings::stLinear)
      bottom = 0.0f;
   else if (type == SpectrogramSettings::stPeriod) {
      // special case
      const auto half = settings.GetFFTLength() / 2;
      // EAC returns no data for below this frequency:
      const float bin2 = rate / half;
      bottom = bin2;
   }
   else
      // logarithmic, etc.
      bottom = 1.0f;

   {
      float spectrumMax = mSpectrumMax;
      if (spectrumMax < 0)
         spectrumMax = settings.maxFreq;
      if (spectrumMax < 0)
         *max = top;
      else
         *max = std::max(bottom, std::min(top, spectrumMax));
   }

   {
      float spectrumMin = mSpectrumMin;
      if (spectrumMin < 0)
         spectrumMin = settings.minFreq;
      if (spectrumMin < 0)
         *min = std::max(bottom, top / 1000.0f);
      else
         *min = std::max(bottom, std::min(top, spectrumMin));
   }
}

void WaveTrackViewGroupData::SetSpectrumBounds(float min, float max)
{
   mSpectrumMin = min;
   mSpectrumMax = max;
}

int WaveTrackViewGroupData::ZeroLevelYCoordinate(wxRect rect) const
{
   return rect.GetTop() +
      (int)((mDisplayMax / (mDisplayMax - mDisplayMin)) * rect.height);
}

const SpectrogramSettings &WaveTrackViewGroupData::GetSpectrogramSettings() const
{
   if (mpSpectrumSettings)
      return *mpSpectrumSettings;
   else
      return SpectrogramSettings::defaults();
}

SpectrogramSettings &WaveTrackViewGroupData::GetSpectrogramSettings()
{
   if (mpSpectrumSettings)
      return *mpSpectrumSettings;
   else
      return SpectrogramSettings::defaults();
}

SpectrogramSettings &WaveTrackViewGroupData::GetIndependentSpectrogramSettings()
{
   if (!mpSpectrumSettings)
      mpSpectrumSettings =
      std::make_unique<SpectrogramSettings>(SpectrogramSettings::defaults());
   return *mpSpectrumSettings;
}

void WaveTrackViewGroupData::SetSpectrogramSettings(
   std::unique_ptr<SpectrogramSettings> &&pSettings)
{
   if (mpSpectrumSettings != pSettings) {
      mpSpectrumSettings = std::move(pSettings);
   }
}

void WaveTrackViewGroupData::UseSpectralPrefs( bool bUse )
{  
   if( bUse ){
      if( !mpSpectrumSettings )
         return;
      // reset it, and next we will be getting the defaults.
      mpSpectrumSettings.reset();
   }
   else {
      if( mpSpectrumSettings )
         return;
      GetIndependentSpectrogramSettings();
   }
}

const WaveformSettings &WaveTrackViewGroupData::GetWaveformSettings() const
{
   if (mpWaveformSettings)
      return *mpWaveformSettings;
   else
      return WaveformSettings::defaults();
}

WaveformSettings &WaveTrackViewGroupData::GetWaveformSettings()
{
   if (mpWaveformSettings)
      return *mpWaveformSettings;
   else
      return WaveformSettings::defaults();
}

WaveformSettings &WaveTrackViewGroupData::GetIndependentWaveformSettings()
{
   if (!mpWaveformSettings)
      mpWaveformSettings = std::make_unique<WaveformSettings>(WaveformSettings::defaults());
   return *mpWaveformSettings;
}

void WaveTrackViewGroupData::SetWaveformSettings(
   std::unique_ptr<WaveformSettings> &&pSettings)
{
   if (mpWaveformSettings != pSettings) {
      mpWaveformSettings = std::move(pSettings);
   }
}
