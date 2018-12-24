/**********************************************************************

Audacity: A Digital Audio Editor

WaveTrackViewGroupData.cpp

Paul Licameli split from class WaveTrack

**********************************************************************/

#include "WaveTrackViewGroupData.h"

#include "../../../../NumberScale.h"
#include "../../../../Prefs.h"
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

// ZoomKind says how to zoom.
// If ZoomStart and ZoomEnd are not equal, this may override
// the zoomKind and cause a drag-zoom-in.
void WaveTrackViewGroupData::DoZoom
   (double rate,
    WaveTrackViewConstants::ZoomActions inZoomKind,
    const wxRect &rect, int zoomStart, int zoomEnd,
    bool fixedMousePoint)
{
   using namespace WaveTrackViewConstants;
   static const float ZOOMLIMIT = 0.001f;

   int height = rect.height;
   int ypos = rect.y;

   // Ensure start and end are in order (swap if not).
   if (zoomEnd < zoomStart)
      std::swap( zoomStart, zoomEnd );

   float min, max, minBand = 0;
   const float halfrate = rate / 2;
   float maxFreq = 8000.0;
   const SpectrogramSettings &specSettings = GetSpectrogramSettings();
   NumberScale scale;
   const bool spectral =
      (GetDisplay() == WaveTrackViewConstants::Spectrum);
   const bool spectrumLinear = spectral &&
      (GetSpectrogramSettings().scaleType == SpectrogramSettings::stLinear);


   bool bDragZoom = IsDragZooming(zoomStart, zoomEnd);
   // Add 100 if spectral to separate the kinds of zoom.
   const int kSpectral = 100;

   // Possibly override the zoom kind.
   if( bDragZoom )
      inZoomKind = kZoomInByDrag;

   // If we are actually zooming a spectrum rather than a wave.
   int ZoomKind = (int)inZoomKind + (spectral ? kSpectral : 0);

   float top=2.0;
   float half=0.5;

   if (spectral) {
      GetSpectrumBounds(rate, &min, &max);
      scale = (specSettings.GetScale(min, max));
      const auto fftLength = specSettings.GetFFTLength();
      const float binSize = rate / fftLength;
      maxFreq = gPrefs->Read(wxT("/Spectrum/MaxFreq"), 8000L);
      // JKC:  Following discussions of Bug 1208 I'm allowing zooming in
      // down to one bin.
      //      const int minBins =
      //         std::min(10, fftLength / 2); //minimum 10 freq bins, unless there are less
      const int minBins = 1;
      minBand = minBins * binSize;
   }
   else{
      GetDisplayBounds(&min, &max);
      const WaveformSettings &waveSettings = GetWaveformSettings();
      const bool linear = waveSettings.isLinear();
      if( !linear ){
         top = (LINEAR_TO_DB(2.0) + waveSettings.dBRange) / waveSettings.dBRange;
         half = (LINEAR_TO_DB(0.5) + waveSettings.dBRange) / waveSettings.dBRange;
      }
   }


   // Compute min and max.
   switch(ZoomKind)
   {
   default:
      // If we have covered all the cases, this won't happen.
      // In release builds Audacity will ignore the zoom.
      wxFAIL_MSG("Zooming Case not implemented by Audacity");
      break;
   case kZoomReset:
   case kZoom1to1:
      {
         // Zoom out full
         min = -1.0;
         max = 1.0;
      }
      break;
   case kZoomDiv2:
      {
         // Zoom out even more than full :-)
         // -2.0..+2.0 (or logarithmic equivalent)
         min = -top;
         max = top;
      }
      break;
   case kZoomTimes2:
      {
         // Zoom in to -0.5..+0.5
         min = -half;
         max = half;
      }
      break;
   case kZoomHalfWave:
      {
         // Zoom to show fractionally more than the top half of the wave.
         min = -0.01f;
         max = 1.0;
      }
      break;
   case kZoomInByDrag:
      {
         const float tmin = min, tmax = max;
         const float p1 = (zoomStart - ypos) / (float)height;
         const float p2 = (zoomEnd - ypos) / (float)height;
         max = (tmax * (1.0 - p1) + tmin * p1);
         min = (tmax * (1.0 - p2) + tmin * p2);

         // Waveform view - allow zooming down to a range of ZOOMLIMIT
         if (max - min < ZOOMLIMIT) {     // if user attempts to go smaller...
            float c = (min + max) / 2;    // ...set centre of view to centre of dragged area and top/bottom to ZOOMLIMIT/2 above/below
            min = c - ZOOMLIMIT / 2.0;
            max = c + ZOOMLIMIT / 2.0;
         }
      }
      break;
   case kZoomIn:
      {
         // Enforce maximum vertical zoom
         const float oldRange = max - min;
         const float l = std::max(ZOOMLIMIT, 0.5f * oldRange);
         const float ratio = l / (max - min);

         const float p1 = (zoomStart - ypos) / (float)height;
         float c = (max * (1.0 - p1) + min * p1);
         if (fixedMousePoint)
            min = c - ratio * (1.0f - p1) * oldRange,
            max = c + ratio * p1 * oldRange;
         else
            min = c - 0.5 * l,
            max = c + 0.5 * l;
      }
      break;
   case kZoomOut:
      {
         // Zoom out
         if (min <= -1.0 && max >= 1.0) {
            min = -top;
            max = top;
         }
         else {
            // limit to +/- 1 range unless already outside that range...
            float minRange = (min < -1) ? -top : -1.0;
            float maxRange = (max > 1) ? top : 1.0;
            // and enforce vertical zoom limits.
            const float p1 = (zoomStart - ypos) / (float)height;
            if (fixedMousePoint) {
               const float oldRange = max - min;
               const float c = (max * (1.0 - p1) + min * p1);
               min = std::min(maxRange - ZOOMLIMIT,
                  std::max(minRange, c - 2 * (1.0f - p1) * oldRange));
               max = std::max(minRange + ZOOMLIMIT,
                  std::min(maxRange, c + 2 * p1 * oldRange));
            }
            else {
               const float c = p1 * min + (1 - p1) * max;
               const float l = (max - min);
               min = std::min(maxRange - ZOOMLIMIT,
                              std::max(minRange, c - l));
               max = std::max(minRange + ZOOMLIMIT,
                              std::min(maxRange, c + l));
            }
         }
      }
      break;

   // VZooming on spectral we don't implement the other zoom presets.
   // They are also not in the menu.
   case kZoomReset + kSpectral:
      {
         // Zoom out to normal level.
         min = spectrumLinear ? 0.0f : 1.0f;
         max = maxFreq;
      }
      break;
   case kZoom1to1 + kSpectral:
   case kZoomDiv2 + kSpectral:
   case kZoomTimes2 + kSpectral:
   case kZoomHalfWave + kSpectral:
      {
         // Zoom out full
         min = spectrumLinear ? 0.0f : 1.0f;
         max = halfrate;
      }
      break;
   case kZoomInByDrag + kSpectral:
      {
         double xmin = 1 - (zoomEnd - ypos) / (float)height;
         double xmax = 1 - (zoomStart - ypos) / (float)height;
         const float middle = (xmin + xmax) / 2;
         const float middleValue = scale.PositionToValue(middle);

         min = std::max(spectrumLinear ? 0.0f : 1.0f,
            std::min(middleValue - minBand / 2,
            scale.PositionToValue(xmin)
            ));
         max = std::min(halfrate,
            std::max(middleValue + minBand / 2,
            scale.PositionToValue(xmax)
            ));
      }
      break;
   case kZoomIn + kSpectral:
      {
         // Center the zoom-in at the click
         const float p1 = (zoomStart - ypos) / (float)height;
         const float middle = 1.0f - p1;
         const float middleValue = scale.PositionToValue(middle);

         if (fixedMousePoint) {
            min = std::max(spectrumLinear ? 0.0f : 1.0f,
               std::min(middleValue - minBand * middle,
               scale.PositionToValue(0.5f * middle)
            ));
            max = std::min(halfrate,
               std::max(middleValue + minBand * p1,
               scale.PositionToValue(middle + 0.5f * p1)
            ));
         }
         else {
            min = std::max(spectrumLinear ? 0.0f : 1.0f,
               std::min(middleValue - minBand / 2,
               scale.PositionToValue(middle - 0.25f)
            ));
            max = std::min(halfrate,
               std::max(middleValue + minBand / 2,
               scale.PositionToValue(middle + 0.25f)
            ));
         }
      }
      break;
   case kZoomOut + kSpectral:
      {
         // Zoom out
         const float p1 = (zoomStart - ypos) / (float)height;
         // (Used to zoom out centered at midline, ignoring the click, if linear view.
         //  I think it is better to be consistent.  PRL)
         // Center zoom-out at the midline
         const float middle = // spectrumLinear ? 0.5f :
            1.0f - p1;

         if (fixedMousePoint) {
            min = std::max(spectrumLinear ? 0.0f : 1.0f, scale.PositionToValue(-middle));
            max = std::min(halfrate, scale.PositionToValue(1.0f + p1));
         }
         else {
            min = std::max(spectrumLinear ? 0.0f : 1.0f, scale.PositionToValue(middle - 1.0f));
            max = std::min(halfrate, scale.PositionToValue(middle + 1.0f));
         }
      }
      break;
   }

   // Now actually apply the zoom.
   if (spectral)
      SetSpectrumBounds(min, max);
   else
      SetDisplayBounds(min, max);

   zoomEnd = zoomStart = 0;
}

bool WaveTrackViewGroupData::IsDragZooming(int zoomStart, int zoomEnd)
{
   const int DragThreshold = 3;// Anything over 3 pixels is a drag, else a click.
   bool bVZoom;
   gPrefs->Read(wxT("/GUI/VerticalZooming"), &bVZoom, false);
   return bVZoom && (abs(zoomEnd - zoomStart) > DragThreshold);
}

void WaveTrackViewGroupData::DoSetMinimized( double rate, bool minimized )
{
#ifdef EXPERIMENTAL_HALF_WAVE
   bool bHalfWave;
   gPrefs->Read(wxT("/GUI/CollapseToHalfWave"), &bHalfWave, false);
   if( bHalfWave )
   {
      using namespace WaveTrackViewConstants;
      DoZoom(
            rate,
            minimized
               ? kZoomHalfWave
               : kZoom1to1,
            wxRect(0,0,0,0), 0,0, true);
   }
#endif
}
