/**********************************************************************

Audacity: A Digital Audio Editor

WaveClipDisplayCache.h

Paul Licameli split from WaveClip.h

**********************************************************************/

#ifndef __AUDACITY_WAVE_CLIP_DISPLAY_CACHE__
#define __AUDACITY_WAVE_CLIP_DISPLAY_CACHE__

#include "../../../../WaveClip.h" // for WaveClipListener

class SpecPxCache;
class WaveCache;

class SpecCache {
public:

   // Make invalid cache
   SpecCache()
      : algorithm(-1)
      , pps(-1.0)
      , start(-1.0)
      , windowType(-1)
      , frequencyGain(-1)
      , dirty(-1)
   {
   }

   ~SpecCache()
   {
   }

   bool Matches(int dirty_, double pixelsPerSecond,
      const SpectrogramSettings &settings, double rate) const;

   // Calculate one column of the spectrum
   bool CalculateOneSpectrum
      (const SpectrogramSettings &settings,
       WaveTrackCache &waveTrackCache,
       const int xx, sampleCount numSamples,
       double offset, double rate, double pixelsPerSecond,
       int lowerBoundX, int upperBoundX,
       const std::vector<float> &gainFactors,
       float* __restrict scratch,
       float* __restrict out) const;

   // Grow the cache while preserving the (possibly now invalid!) contents
   void Grow(size_t len_, const SpectrogramSettings& settings,
               double pixelsPerSecond, double start_);

   // Calculate the dirty columns at the begin and end of the cache
   void Populate
      (const SpectrogramSettings &settings, WaveTrackCache &waveTrackCache,
       int copyBegin, int copyEnd, size_t numPixels,
       sampleCount numSamples,
       double offset, double rate, double pixelsPerSecond);

   size_t       len { 0 }; // counts pixels, not samples
   int          algorithm;
   double       pps;
   double       start;
   int          windowType;
   size_t       windowSize { 0 };
   unsigned     zeroPaddingFactor { 0 };
   int          frequencyGain;
   std::vector<float> freq;
   std::vector<sampleCount> where;

   int          dirty;
};

struct WaveClipDisplayCache final : WaveClipListener
{
   WaveClipDisplayCache();
   ~WaveClipDisplayCache() override;

   // Cache of values to colour pixels of Spectrogram - used by TrackArtist
   std::unique_ptr<SpecPxCache> mSpecPxCache;
   std::unique_ptr<WaveCache> mWaveCache;
   ODLock       mWaveCacheMutex {};
   std::unique_ptr<SpecCache> mSpecCache;
   wxRect mDisplayRect {};
   int mDirty { 0 };

   static WaveClipDisplayCache &Get( const WaveClip &clip );

   void MarkChanged() override; // NOFAIL-GUARANTEE
   void Invalidate() override; // NOFAIL-GUARANTEE

   ///Delete the wave cache - force redraw.  Thread-safe
   void Clear();

   ///Adds an invalid region to the wavecache so it redraws that portion only.
   void AddInvalidRegion(sampleCount startSample, sampleCount endSample);

   /** Getting high-level data for screen display and clipping
    * calculations and Contrast */
   bool GetWaveDisplay(const WaveClip &clip, WaveDisplay &display,
                       double t0, double pixelsPerSecond, bool &isLoadingOD);
   bool GetSpectrogram(const WaveClip &clip, WaveTrackCache &cache,
                       const float *& spectrogram,
                       const sampleCount *& where,
                       size_t numPixels,
                       double t0, double pixelsPerSecond);

   // Set/clear/get rectangle that this WaveClip fills on screen. This is
   // called by TrackArtist while actually drawing the tracks and clips.
   void ClearDisplayRect();
   void SetDisplayRect(const wxRect& r);
   void GetDisplayRect(wxRect* r);
};

#endif
