/**********************************************************************

Audacity: A Digital Audio Editor

WaveClipDisplayCache.cpp

Paul Licameli split from WaveClip.cpp

******************************************************************//***

\class WaveCache
\brief Cache used with WaveClip to cache wave information (for drawing).

*//*******************************************************************/

#include "WaveClipDisplayCache.h"

#include "WaveTrackViewGroupData.h"

#include "../../../../BlockFile.h"
#include "../../../../Sequence.h"
#include "../../../../Spectrum.h"
#include "../../../../WaveTrack.h"
#include "../../../../prefs/SpectrogramSettings.h"

#include <float.h>

namespace {

struct MinMaxSumsq
{
   MinMaxSumsq(const float *pv, int count, int divisor)
   {
      min = FLT_MAX, max = -FLT_MAX, sumsq = 0.0f;
      while (count--) {
         float v;
         switch (divisor) {
         default:
         case 1:
            // array holds samples
            v = *pv++;
            if (v < min)
               min = v;
            if (v > max)
               max = v;
            sumsq += v * v;
            break;
         case 256:
         case 65536:
            // array holds triples of min, max, and rms values
            v = *pv++;
            if (v < min)
               min = v;
            v = *pv++;
            if (v > max)
               max = v;
            v = *pv++;
            sumsq += v * v;
            break;
         }
      }
   }

   float min;
   float max;
   float sumsq;
};

}

// where is input, assumed to be nondecreasing, and its size is len + 1.
// min, max, rms, bl are outputs, and their lengths are len.
// Each position in the output arrays corresponds to one column of pixels.
// The column for pixel p covers samples from
// where[p] up to (but excluding) where[p + 1].
// bl is negative wherever data are not yet available.
// Return true if successful.
bool GetWaveDisplay(const Sequence &sequence,
   float *min, float *max, float *rms, int* bl,
   size_t len, const sampleCount *where)
{
   wxASSERT(len > 0);
   const auto s0 = std::max(sampleCount(0), where[0]);
   const auto numSamples = sequence.GetNumSamples();
   if (s0 >= numSamples)
      // None of the samples asked for are in range. Abandon.
      return false;

   // In case where[len - 1] == where[len], raise the limit by one,
   // so we load at least one pixel for column len - 1
   // ... unless the mNumSamples ceiling applies, and then there are other defenses
   const auto s1 =
      std::min(numSamples, std::max(1 + where[len - 1], where[len]));
   const auto maxSamples = sequence.GetMaxBlockSize();
   Floats temp{ maxSamples };

   decltype(len) pixel = 0;

   auto srcX = s0;
   decltype(srcX) nextSrcX = 0;
   int lastRmsDenom = 0;
   int lastDivisor = 0;
   auto whereNow = std::min(s1 - 1, where[0]);
   decltype(whereNow) whereNext = 0;
   // Loop over block files, opening and reading and closing each
   // not more than once
   const auto &blocks = sequence.GetBlockArray();
   unsigned nBlocks = blocks.size();
   const unsigned int block0 = sequence.FindBlock(s0);
   for (unsigned int b = block0; b < nBlocks; ++b) {
      if (b > block0)
         srcX = nextSrcX;
      if (srcX >= s1)
         break;

      // Find the range of sample values for this block that
      // are in the display.
      const SeqBlock &seqBlock = blocks[b];
      const auto start = seqBlock.start;
      nextSrcX = std::min(s1, start + seqBlock.f->GetLength());

      // The column for pixel p covers samples from
      // where[p] up to but excluding where[p + 1].

      // Find the range of pixels covered by the current block file
      // (Their starting samples covered by it, to be exact)
      decltype(len) nextPixel;
      if (nextSrcX >= s1)
         // last pass
         nextPixel = len;
      else {
         nextPixel = pixel;
         // Taking min with s1 - 1, here and elsewhere, is another defense
         // to be sure the last pixel column gets at least one sample
         while (nextPixel < len &&
                (whereNext = std::min(s1 - 1, where[nextPixel])) < nextSrcX)
            ++nextPixel;
      }
      if (nextPixel == pixel)
         // The entire block's samples fall within one pixel column.
         // Either it's a rare odd block at the end, or else,
         // we must be really zoomed out!
         // Omit the entire block's contents from min/max/rms
         // calculation, which is not correct, but correctness might not
         // be worth the compute time if this happens every pixel
         // column. -- PRL
         continue;
      if (nextPixel == len)
         whereNext = s1;

      // Decide the summary level
      const double samplesPerPixel =
         (whereNext - whereNow).as_double() / (nextPixel - pixel);
      const int divisor =
           (samplesPerPixel >= 65536) ? 65536
         : (samplesPerPixel >= 256) ? 256
         : 1;

      int blockStatus = b;

      // How many samples or triples are needed?

      const size_t startPosition =
         // srcX and start are in the same block
         std::max(sampleCount(0), (srcX - start) / divisor).as_size_t();
      const size_t inclusiveEndPosition =
         // nextSrcX - 1 and start are in the same block
         std::min((sampleCount(maxSamples) / divisor) - 1,
                  (nextSrcX - 1 - start) / divisor).as_size_t();
      const auto num = 1 + inclusiveEndPosition - startPosition;
      if (num <= 0) {
         // What?  There was a zero length block file?
         wxASSERT(false);
         // Do some defense against this case anyway
         while (pixel < nextPixel) {
            min[pixel] = max[pixel] = rms[pixel] = 0;
            bl[pixel] = blockStatus;//MC
            ++pixel;
         }
         continue;
      }

      // Read from the block file or its summary
      switch (divisor) {
      default:
      case 1:
         // Read samples
         // no-throw for display operations!
         sequence.Read(
            (samplePtr)temp.get(), floatSample, seqBlock, startPosition, num, false);
         break;
      case 256:
         // Read triples
         //check to see if summary data has been computed
         if (seqBlock.f->IsSummaryAvailable())
            // Ignore the return value.
            // This function fills with zeroes if read fails
            seqBlock.f->Read256(temp.get(), startPosition, num);
         else
            //otherwise, mark the display as not yet computed
            blockStatus = -1 - b;
         break;
      case 65536:
         // Read triples
         //check to see if summary data has been computed
         if (seqBlock.f->IsSummaryAvailable())
            // Ignore the return value.
            // This function fills with zeroes if read fails
            seqBlock.f->Read64K(temp.get(), startPosition, num);
         else
            //otherwise, mark the display as not yet computed
            blockStatus = -1 - b;
         break;
      }
      
      auto filePosition = startPosition;

      // The previous pixel column might straddle blocks.
      // If so, impute some of the data to it.
      if (b > block0 && pixel > 0) {
         // whereNow and start are in the same block
         auto midPosition = ((whereNow - start) / divisor).as_size_t();
         int diff(midPosition - filePosition);
         if (diff > 0) {
            MinMaxSumsq values(temp.get(), diff, divisor);
            const int lastPixel = pixel - 1;
            float &lastMin = min[lastPixel];
            lastMin = std::min(lastMin, values.min);
            float &lastMax = max[lastPixel];
            lastMax = std::max(lastMax, values.max);
            float &lastRms = rms[lastPixel];
            int lastNumSamples = lastRmsDenom * lastDivisor;
            lastRms = sqrt(
               (lastRms * lastRms * lastNumSamples + values.sumsq * divisor) /
               (lastNumSamples + diff * divisor)
            );

            filePosition = midPosition;
         }
      }

      // Loop over file positions
      int rmsDenom = 0;
      for (; filePosition <= inclusiveEndPosition;) {
         // Find range of pixel columns for this file position
         // (normally just one, but maybe more when zoomed very close)
         // and the range of positions for those columns
         // (normally one or more, for that one column)
         auto pixelX = pixel + 1;
         decltype(filePosition) positionX = 0;
         while (pixelX < nextPixel &&
            filePosition ==
               (positionX = (
                  // s1 - 1 or where[pixelX] and start are in the same block
                  (std::min(s1 - 1, where[pixelX]) - start) / divisor).as_size_t() )
         )
            ++pixelX;
         if (pixelX >= nextPixel)
            positionX = 1 + inclusiveEndPosition;

         // Find results to assign
         rmsDenom = (positionX - filePosition);
         wxASSERT(rmsDenom > 0);
         const float *const pv =
            temp.get() + (filePosition - startPosition) * (divisor == 1 ? 1 : 3);
         MinMaxSumsq values(pv, std::max(0, rmsDenom), divisor);

         // Assign results
         std::fill(&min[pixel], &min[pixelX], values.min);
         std::fill(&max[pixel], &max[pixelX], values.max);
         std::fill(&bl[pixel], &bl[pixelX], blockStatus);
         std::fill(&rms[pixel], &rms[pixelX], (float)sqrt(values.sumsq / rmsDenom));

         pixel = pixelX;
         filePosition = positionX;
      }

      wxASSERT(pixel == nextPixel);
      whereNow = whereNext;
      pixel = nextPixel;
      lastDivisor = divisor;
      lastRmsDenom = rmsDenom;
   } // for each block file

   wxASSERT(pixel == len);

   return true;
}

class WaveCache {
public:
   WaveCache()
      : dirty(-1)
      , start(-1)
      , pps(0)
      , rate(-1)
      , where(0)
      , min(0)
      , max(0)
      , rms(0)
      , bl(0)
      , numODPixels(0)
   {
   }

   WaveCache(size_t len_, double pixelsPerSecond, double rate_, double t0, int dirty_)
      : dirty(dirty_)
      , len(len_)
      , start(t0)
      , pps(pixelsPerSecond)
      , rate(rate_)
      , where(1 + len)
      , min(len)
      , max(len)
      , rms(len)
      , bl(len)
      , numODPixels(0)
   {

      //find the number of OD pixels - the only way to do this is by recounting since we've lost some old cache.
      numODPixels = CountODPixels(0, len);
   }

   ~WaveCache()
   {
      ClearInvalidRegions();
   }

   int          dirty;
   const size_t len { 0 }; // counts pixels, not samples
   const double start;
   const double pps;
   const int    rate;
   std::vector<sampleCount> where;
   std::vector<float> min;
   std::vector<float> max;
   std::vector<float> rms;
   std::vector<int> bl;
   int         numODPixels;

   class InvalidRegion
   {
   public:
     InvalidRegion(size_t s, size_t e)
         : start(s), end(e)
     {}
     //start and end pixel count.  (not samples)
     size_t start;
     size_t end;
   };


   //Thread safe call to add a NEW region to invalidate.  If it overlaps with other regions, it unions the them.
   void AddInvalidRegion(sampleCount sampleStart, sampleCount sampleEnd)
   {
      //use pps to figure out where we are.  (pixels per second)
      if(pps ==0)
         return;
      double samplesPerPixel = rate/pps;
      //rate is SR, start is first time of the waveform (in second) on cache
      long invalStart = (sampleStart.as_double() - start*rate) / samplesPerPixel ;

      long invalEnd = (sampleEnd.as_double() - start*rate)/samplesPerPixel +1; //we should cover the end..

      //if they are both off the cache boundary in the same direction, the cache is missed,
      //so we are safe, and don't need to track this one.
      if((invalStart<0 && invalEnd <0) || (invalStart>=(long)len && invalEnd >= (long)len))
         return;

      //in all other cases, we need to clip the boundries so they make sense with the cache.
      //for some reason, the cache is set up to access up to array[len], not array[len-1]
      if(invalStart <0)
         invalStart =0;
      else if(invalStart > (long)len)
         invalStart = len;

      if(invalEnd <0)
         invalEnd =0;
      else if(invalEnd > (long)len)
         invalEnd = len;


      ODLocker locker(&mRegionsMutex);

      //look thru the region array for a place to insert.  We could make this more spiffy than a linear search
      //but right now it is not needed since there will usually only be one region (which grows) for OD loading.
      bool added=false;
      if(mRegions.size())
      {
         for(size_t i=0;i<mRegions.size();i++)
         {
            //if the regions intersect OR are pixel adjacent
            InvalidRegion &region = mRegions[i];
            if((long)region.start <= (invalEnd+1)
               && ((long)region.end + 1) >= invalStart)
            {
               //take the union region
               if((long)region.start > invalStart)
                  region.start = invalStart;
               if((long)region.end < invalEnd)
                  region.end = invalEnd;
               added=true;
               break;
            }

            //this bit doesn't make sense because it assumes we add in order - now we go backwards after the initial OD finishes
//            //this array is sorted by start/end points and has no overlaps.   If we've passed all possible intersections, insert.  The array will remain sorted.
//            if(region.end < invalStart)
//            {
//               mRegions.insert(
//                  mRegions.begin() + i,
//                  InvalidRegion{ invalStart, invalEnd }
//               );
//               break;
//            }
         }
      }

      if(!added)
      {
         InvalidRegion newRegion(invalStart, invalEnd);
         mRegions.insert(mRegions.begin(), newRegion);
      }


      //now we must go and patch up all the regions that overlap.  Overlapping regions will be adjacent.
      for(size_t i=1;i<mRegions.size();i++)
      {
         //if the regions intersect OR are pixel adjacent
         InvalidRegion &region = mRegions[i];
         InvalidRegion &prevRegion = mRegions[i - 1];
         if(region.start <= prevRegion.end+1
            && region.end + 1 >= prevRegion.start)
         {
            //take the union region
            if(region.start > prevRegion.start)
               region.start = prevRegion.start;
            if(region.end < prevRegion.end)
               region.end = prevRegion.end;

            mRegions.erase(mRegions.begin()+i-1);
               //musn't forget to reset cursor
               i--;
         }

         //if we are past the end of the region we added, we are past the area of regions that might be oversecting.
         if(invalEnd < 0 || (long)region.start > invalEnd)
         {
            break;
         }
      }
   }

   //lock before calling these in a section.  unlock after finished.
   int GetNumInvalidRegions() const {return mRegions.size();}
   size_t GetInvalidRegionStart(int i) const {return mRegions[i].start;}
   size_t GetInvalidRegionEnd(int i) const {return mRegions[i].end;}

   void ClearInvalidRegions()
   {
      mRegions.clear();
   }

   void LoadInvalidRegion(int ii, const Sequence *sequence, bool updateODCount)
   {
      const auto invStart = GetInvalidRegionStart(ii);
      const auto invEnd = GetInvalidRegionEnd(ii);

      //before check number of ODPixels
      int regionODPixels = 0;
      if (updateODCount)
         regionODPixels = CountODPixels(invStart, invEnd);

      ::GetWaveDisplay(*sequence, &min[invStart],
         &max[invStart],
         &rms[invStart],
         &bl[invStart],
         invEnd - invStart,
         &where[invStart]);

      //after check number of ODPixels
      if (updateODCount)
      {
         const int regionODPixelsAfter = CountODPixels(invStart, invEnd);
         numODPixels -= (regionODPixels - regionODPixelsAfter);
      }
   }

   void LoadInvalidRegions(const Sequence *sequence, bool updateODCount)
   {
      //invalid regions are kept in a sorted array.
      for (int i = 0; i < GetNumInvalidRegions(); i++)
         LoadInvalidRegion(i, sequence, updateODCount);
   }

   int CountODPixels(size_t startIn, size_t endIn)
   {
      using namespace std;
      const int *begin = &bl[0];
      return count_if(begin + startIn, begin + endIn, bind2nd(less<int>(), 0));
   }

protected:
   std::vector<InvalidRegion> mRegions;
   ODLock mRegionsMutex;

};

bool SpecCache::Matches
   (int dirty_, double pixelsPerSecond,
    const SpectrogramSettings &settings, double rate) const
{
   // Make a tolerant comparison of the pps values in this wise:
   // accumulated difference of times over the number of pixels is less than
   // a sample period.
   const double tstep = 1.0 / pixelsPerSecond;
   const bool ppsMatch =
      (fabs(tstep - 1.0 / pps) * len < (1.0 / rate));

   return
      ppsMatch &&
      dirty == dirty_ &&
      windowType == settings.windowType &&
      windowSize == settings.WindowSize() &&
      zeroPaddingFactor == settings.ZeroPaddingFactor() &&
      frequencyGain == settings.frequencyGain &&
      algorithm == settings.algorithm;
}

static void ComputeSpectrumUsingRealFFTf
   (float * __restrict buffer, const FFTParam *hFFT,
    const float * __restrict window, size_t len, float * __restrict out)
{
   size_t i;
   if(len > hFFT->Points * 2)
      len = hFFT->Points * 2;
   for(i = 0; i < len; i++)
      buffer[i] *= window[i];
   for( ; i < (hFFT->Points * 2); i++)
      buffer[i] = 0; // zero pad as needed
   RealFFTf(buffer, hFFT);
   // Handle the (real-only) DC
   float power = buffer[0] * buffer[0];
   if(power <= 0)
      out[0] = -160.0;
   else
      out[0] = 10.0 * log10f(power);
   for(i = 1; i < hFFT->Points; i++) {
      const int index = hFFT->BitReversed[i];
      const float re = buffer[index], im = buffer[index + 1];
      power = re * re + im * im;
      if(power <= 0)
         out[i] = -160.0;
      else
         out[i] = 10.0*log10f(power);
   }
}

bool SpecCache::CalculateOneSpectrum
   (const SpectrogramSettings &settings,
    WaveTrackCache &waveTrackCache,
    const int xx, const sampleCount numSamples,
    double offset, double rate, double pixelsPerSecond,
    int lowerBoundX, int upperBoundX,
    const std::vector<float> &gainFactors,
    float* __restrict scratch, float* __restrict out) const
{
   bool result = false;
   const bool reassignment =
      (settings.algorithm == SpectrogramSettings::algReassignment);
   const size_t windowSizeSetting = settings.WindowSize();

   sampleCount from;

   // xx may be for a column that is out of the visible bounds, but only
   // when we are calculating reassignment contributions that may cross into
   // the visible area.

   if (xx < 0)
      from = sampleCount(
         where[0].as_double() + xx * (rate / pixelsPerSecond)
      );
   else if (xx > (int)len)
      from = sampleCount(
         where[len].as_double() + (xx - len) * (rate / pixelsPerSecond)
      );
   else
      from = where[xx];

   const bool autocorrelation =
      settings.algorithm == SpectrogramSettings::algPitchEAC;
   const size_t zeroPaddingFactorSetting = settings.ZeroPaddingFactor();
   const size_t padding = (windowSizeSetting * (zeroPaddingFactorSetting - 1)) / 2;
   const size_t fftLen = windowSizeSetting * zeroPaddingFactorSetting;
   auto nBins = settings.NBins();

   if (from < 0 || from >= numSamples) {
      if (xx >= 0 && xx < (int)len) {
         // Pixel column is out of bounds of the clip!  Should not happen.
         float *const results = &out[nBins * xx];
         std::fill(results, results + nBins, 0.0f);
      }
   }
   else {


      // We can avoid copying memory when ComputeSpectrum is used below
      bool copy = !autocorrelation || (padding > 0) || reassignment;
      float *useBuffer = 0;
      float *adj = scratch + padding;

      {
         auto myLen = windowSizeSetting;
         // Take a window of the track centered at this sample.
         from -= windowSizeSetting >> 1;
         if (from < 0) {
            // Near the start of the clip, pad left with zeroes as needed.
            // from is at least -windowSize / 2
            for (auto ii = from; ii < 0; ++ii)
               *adj++ = 0;
            myLen += from.as_long_long(); // add a negative
            from = 0;
            copy = true;
         }

         if (from + myLen >= numSamples) {
            // Near the end of the clip, pad right with zeroes as needed.
            // newlen is bounded by myLen:
            auto newlen = ( numSamples - from ).as_size_t();
            for (decltype(myLen) ii = newlen; ii < myLen; ++ii)
               adj[ii] = 0;
            myLen = newlen;
            copy = true;
         }

         if (myLen > 0) {
            useBuffer = (float*)(waveTrackCache.Get(
               floatSample, sampleCount(
                  floor(0.5 + from.as_double() + offset * rate)
               ),
               myLen,
               // Don't throw in this drawing operation
               false)
            );

            if (copy) {
               if (useBuffer)
                  memcpy(adj, useBuffer, myLen * sizeof(float));
               else
                  memset(adj, 0, myLen * sizeof(float));
            }
         }
      }

      if (copy || !useBuffer)
         useBuffer = scratch;

      if (autocorrelation) {
         // not reassignment, xx is surely within bounds.
         wxASSERT(xx >= 0);
         float *const results = &out[nBins * xx];
         // This function does not mutate useBuffer
         ComputeSpectrum(useBuffer, windowSizeSetting, windowSizeSetting,
            rate, results,
            autocorrelation, settings.windowType);
      }
      else if (reassignment) {
         static const double epsilon = 1e-16;
         const auto hFFT = settings.hFFT.get();

         float *const scratch2 = scratch + fftLen;
         std::copy(scratch, scratch2, scratch2);

         float *const scratch3 = scratch + 2 * fftLen;
         std::copy(scratch, scratch2, scratch3);

         {
            const float *const window = settings.window.get();
            for (size_t ii = 0; ii < fftLen; ++ii)
               scratch[ii] *= window[ii];
            RealFFTf(scratch, hFFT);
         }

         {
            const float *const dWindow = settings.dWindow.get();
            for (size_t ii = 0; ii < fftLen; ++ii)
               scratch2[ii] *= dWindow[ii];
            RealFFTf(scratch2, hFFT);
         }

         {
            const float *const tWindow = settings.tWindow.get();
            for (size_t ii = 0; ii < fftLen; ++ii)
               scratch3[ii] *= tWindow[ii];
            RealFFTf(scratch3, hFFT);
         }

         for (size_t ii = 0; ii < hFFT->Points; ++ii) {
            const int index = hFFT->BitReversed[ii];
            const float
               denomRe = scratch[index],
               denomIm = ii == 0 ? 0 : scratch[index + 1];
            const double power = denomRe * denomRe + denomIm * denomIm;
            if (power < epsilon)
               // Avoid dividing by near-zero below
               continue;

            double freqCorrection;
            {
               const double multiplier = -(fftLen / (2.0f * M_PI));
               const float
                  numRe = scratch2[index],
                  numIm = ii == 0 ? 0 : scratch2[index + 1];
               // Find complex quotient --
               // Which means, multiply numerator by conjugate of denominator,
               // then divide by norm squared of denominator --
               // Then just take its imaginary part.
               const double
                  quotIm = (-numRe * denomIm + numIm * denomRe) / power;
               // With appropriate multiplier, that becomes the correction of
               // the frequency bin.
               freqCorrection = multiplier * quotIm;
            }

            const int bin = (int)((int)ii + freqCorrection + 0.5f);
            // Must check if correction takes bin out of bounds, above or below!
            // bin is signed!
            if (bin >= 0 && bin < (int)hFFT->Points) {
               double timeCorrection;
               {
                  const float
                     numRe = scratch3[index],
                     numIm = ii == 0 ? 0 : scratch3[index + 1];
                  // Find another complex quotient --
                  // Then just take its real part.
                  // The result has sample interval as unit.
                  timeCorrection =
                     (numRe * denomRe + numIm * denomIm) / power;
               }

               int correctedX = (floor(0.5 + xx + timeCorrection * pixelsPerSecond / rate));
               if (correctedX >= lowerBoundX && correctedX < upperBoundX)
               {
                  result = true;

                  // This is non-negative, because bin and correctedX are
                  auto ind = (int)nBins * correctedX + bin;
#ifdef _OPENMP
                  // This assignment can race if index reaches into another thread's bins.
                  // The probability of a race very low, so this carries little overhead,
                  // about 5% slower vs allowing it to race.
                  #pragma omp atomic update
#endif
                  out[ind] += power;
               }
            }
         }
      }
      else {
         // not reassignment, xx is surely within bounds.
         wxASSERT(xx >= 0);
         float *const results = &out[nBins * xx];

         // Do the FFT.  Note that useBuffer is multiplied by the window,
         // and the window is initialized with leading and trailing zeroes
         // when there is padding.  Therefore we did not need to reinitialize
         // the part of useBuffer in the padding zones.

         // This function mutates useBuffer
         ComputeSpectrumUsingRealFFTf
            (useBuffer, settings.hFFT.get(), settings.window.get(), fftLen, results);
         if (!gainFactors.empty()) {
            // Apply a frequency-dependant gain factor
            for (size_t ii = 0; ii < nBins; ++ii)
               results[ii] += gainFactors[ii];
         }
      }
   }

   return result;
}

void SpecCache::Grow(size_t len_, const SpectrogramSettings& settings,
                       double pixelsPerSecond, double start_)
{
   settings.CacheWindows();

   // len columns, and so many rows, column-major.
   // Don't take column literally -- this isn't pixel data yet, it's the
   // raw data to be mapped onto the display.
   freq.resize(len_ * settings.NBins());

   // Sample counts corresponding to the columns, and to one past the end.
   where.resize(len_ + 1);

   len = len_;
   algorithm = settings.algorithm;
   pps = pixelsPerSecond;
   start = start_;
   windowType = settings.windowType;
   windowSize = settings.WindowSize();
   zeroPaddingFactor = settings.ZeroPaddingFactor();
   frequencyGain = settings.frequencyGain;
}

namespace {

void ComputeSpectrogramGainFactors
   (size_t fftLen, double rate, int frequencyGain, std::vector<float> &gainFactors)
{
   if (frequencyGain > 0) {
      // Compute a frequency-dependent gain factor
      // scaled such that 1000 Hz gets a gain of 0dB

      // This is the reciprocal of the bin number of 1000 Hz:
      const double factor = ((double)rate / (double)fftLen) / 1000.0;

      auto half = fftLen / 2;
      gainFactors.reserve(half);
      // Don't take logarithm of zero!  Let bin 0 replicate the gain factor for bin 1.
      gainFactors.push_back(frequencyGain*log10(factor));
      for (decltype(half) x = 1; x < half; x++) {
         gainFactors.push_back(frequencyGain*log10(factor * x));
      }
   }
}

}

void SpecCache::Populate
   (const SpectrogramSettings &settings, WaveTrackCache &waveTrackCache,
    int copyBegin, int copyEnd, size_t numPixels,
    sampleCount numSamples,
    double offset, double rate, double pixelsPerSecond)
{
   const int &frequencyGainSetting = settings.frequencyGain;
   const size_t windowSizeSetting = settings.WindowSize();
   const bool autocorrelation =
      settings.algorithm == SpectrogramSettings::algPitchEAC;
   const bool reassignment =
      settings.algorithm == SpectrogramSettings::algReassignment;
#ifdef EXPERIMENTAL_ZERO_PADDED_SPECTROGRAMS
   const size_t zeroPaddingFactorSetting = settings.ZeroPaddingFactor();
#else
   const size_t zeroPaddingFactorSetting = 1;
#endif

   // FFT length may be longer than the window of samples that affect results
   // because of zero padding done for increased frequency resolution
   const size_t fftLen = windowSizeSetting * zeroPaddingFactorSetting;
   const auto nBins = settings.NBins();

   const size_t bufferSize = fftLen;
   const size_t scratchSize = reassignment ? 3 * bufferSize : bufferSize;
   std::vector<float> scratch(scratchSize);

   std::vector<float> gainFactors;
   if (!autocorrelation)
      ComputeSpectrogramGainFactors(fftLen, rate, frequencyGainSetting, gainFactors);

   // Loop over the ranges before and after the copied portion and compute anew.
   // One of the ranges may be empty.
   for (int jj = 0; jj < 2; ++jj) {
      const int lowerBoundX = jj == 0 ? 0 : copyEnd;
      const int upperBoundX = jj == 0 ? copyBegin : numPixels;

#ifdef _OPENMP
      // Storage for mutable per-thread data.
      // private clause ensures one copy per thread
      struct ThreadLocalStorage {
         ThreadLocalStorage()  { }
         ~ThreadLocalStorage() { }

         void init(WaveTrackCache &waveTrackCache, size_t scratchSize) {
            if (!cache) {
               cache = std::make_unique<WaveTrackCache>(waveTrackCache.GetTrack());
               scratch.resize(scratchSize);
            }
         }
         std::unique_ptr<WaveTrackCache> cache;
         std::vector<float> scratch;
      } tls;

      #pragma omp parallel for private(tls)
#endif
      for (auto xx = lowerBoundX; xx < upperBoundX; ++xx)
      {
#ifdef _OPENMP
         tls.init(waveTrackCache, scratchSize);
         WaveTrackCache& cache = *tls.cache;
         float* buffer = &tls.scratch[0];
#else
         WaveTrackCache& cache = waveTrackCache;
         float* buffer = &scratch[0];
#endif
         CalculateOneSpectrum(
            settings, cache, xx, numSamples,
            offset, rate, pixelsPerSecond,
            lowerBoundX, upperBoundX,
            gainFactors, buffer, &freq[0]);
      }

      if (reassignment) {
         // Need to look beyond the edges of the range to accumulate more
         // time reassignments.
         // I'm not sure what's a good stopping criterion?
         auto xx = lowerBoundX;
         const double pixelsPerSample = pixelsPerSecond / rate;
         const int limit = std::min((int)(0.5 + fftLen * pixelsPerSample), 100);
         for (int ii = 0; ii < limit; ++ii)
         {
            const bool result =
               CalculateOneSpectrum(
                  settings, waveTrackCache, --xx, numSamples,
                  offset, rate, pixelsPerSecond,
                  lowerBoundX, upperBoundX,
                  gainFactors, &scratch[0], &freq[0]);
            if (!result)
               break;
         }

         xx = upperBoundX;
         for (int ii = 0; ii < limit; ++ii)
         {
            const bool result =
               CalculateOneSpectrum(
                  settings, waveTrackCache, xx++, numSamples,
                  offset, rate, pixelsPerSecond,
                  lowerBoundX, upperBoundX,
                  gainFactors, &scratch[0], &freq[0]);
            if (!result)
               break;
         }

         // Now Convert to dB terms.  Do this only after accumulating
         // power values, which may cross columns with the time correction.
#ifdef _OPENMP
         #pragma omp parallel for
#endif
         for (xx = lowerBoundX; xx < upperBoundX; ++xx) {
            float *const results = &freq[nBins * xx];
            for (size_t ii = 0; ii < nBins; ++ii) {
               float &power = results[ii];
               if (power <= 0)
                  power = -160.0;
               else
                  power = 10.0*log10f(power);
            }
            if (!gainFactors.empty()) {
               // Apply a frequency-dependant gain factor
               for (size_t ii = 0; ii < nBins; ++ii)
                  results[ii] += gainFactors[ii];
            }
         }
      }
   }
}

///Delete the wave cache - force redraw.  Thread-safe
void WaveClipDisplayCache::Clear()
{
   ODLocker locker(&mWaveCacheMutex);
   mWaveCache = std::make_unique<WaveCache>();
}

///Adds an invalid region to the wavecache so it redraws that portion only.
void WaveClipDisplayCache::AddInvalidRegion(
   sampleCount startSample, sampleCount endSample)
{
   ODLocker locker(&mWaveCacheMutex);
   if(mWaveCache!=NULL)
      mWaveCache->AddInvalidRegion(startSample,endSample);
}

namespace {

inline
void findCorrection(const std::vector<sampleCount> &oldWhere, size_t oldLen,
         size_t newLen,
         double t0, double rate, double samplesPerPixel,
         int &oldX0, double &correction)
{
   // Mitigate the accumulation of location errors
   // in copies of copies of ... of caches.
   // Look at the loop that populates "where" below to understand this.

   // Find the sample position that is the origin in the old cache.
   const double oldWhere0 = oldWhere[1].as_double() - samplesPerPixel;
   const double oldWhereLast = oldWhere0 + oldLen * samplesPerPixel;
   // Find the length in samples of the old cache.
   const double denom = oldWhereLast - oldWhere0;

   // What sample would go in where[0] with no correction?
   const double guessWhere0 = t0 * rate;

   if ( // Skip if old and NEW are disjoint:
      oldWhereLast <= guessWhere0 ||
      guessWhere0 + newLen * samplesPerPixel <= oldWhere0 ||
      // Skip unless denom rounds off to at least 1.
      denom < 0.5)
   {
      // The computation of oldX0 in the other branch
      // may underflow and the assertion would be violated.
      oldX0 =  oldLen;
      correction = 0.0;
   }
   else
   {
      // What integer position in the old cache array does that map to?
      // (even if it is out of bounds)
      oldX0 = floor(0.5 + oldLen * (guessWhere0 - oldWhere0) / denom);
      // What sample count would the old cache have put there?
      const double where0 = oldWhere0 + double(oldX0) * samplesPerPixel;
      // What correction is needed to align the NEW cache with the old?
      const double correction0 = where0 - guessWhere0;
      correction = std::max(-samplesPerPixel, std::min(samplesPerPixel, correction0));
      wxASSERT(correction == correction0);
   }
}

inline void
fillWhere(std::vector<sampleCount> &where, size_t len, double bias, double correction,
          double t0, double rate, double samplesPerPixel)
{
   // Be careful to make the first value non-negative
   const double w0 = 0.5 + correction + bias + t0 * rate;
   where[0] = sampleCount( std::max(0.0, floor(w0)) );
   for (decltype(len) x = 1; x < len + 1; x++)
      where[x] = sampleCount( floor(w0 + double(x) * samplesPerPixel) );
}

}

//
// Getting high-level data from the track for screen display and
// clipping calculations
//

bool WaveClipDisplayCache::GetWaveDisplay(
   const WaveClip &clip, WaveDisplay &display, double t0,
   double pixelsPerSecond, bool &isLoadingOD)
{
   const bool allocated = (display.where != 0);

   const size_t numPixels = (int)display.width;

   size_t p0 = 0;         // least column requiring computation
   size_t p1 = numPixels; // greatest column requiring computation, plus one

   float *min;
   float *max;
   float *rms;
   int *bl;
   std::vector<sampleCount> *pWhere;

   if (allocated) {
      // assume ownWhere is filled.
      min = &display.min[0];
      max = &display.max[0];
      rms = &display.rms[0];
      bl = &display.bl[0];
      pWhere = &display.ownWhere;
   }
   else {
      // Lock the list of invalid regions
      ODLocker locker(&mWaveCacheMutex);

      const auto sequence = clip.GetSequence();
      const double tstep = 1.0 / pixelsPerSecond;
      const auto rate = clip.GetRate();
      const double samplesPerPixel = rate * tstep;

      // Make a tolerant comparison of the pps values in this wise:
      // accumulated difference of times over the number of pixels is less than
      // a sample period.
      const bool ppsMatch = mWaveCache &&
         (fabs(tstep - 1.0 / mWaveCache->pps) * numPixels < (1.0 / rate));

      const bool match =
         mWaveCache &&
         ppsMatch &&
         mWaveCache->len > 0 &&
         mWaveCache->dirty == mDirty;

      if (match &&
         mWaveCache->start == t0 &&
         mWaveCache->len >= numPixels) {
         mWaveCache->LoadInvalidRegions(sequence, true);
         mWaveCache->ClearInvalidRegions();

         // Satisfy the request completely from the cache
         display.min = &mWaveCache->min[0];
         display.max = &mWaveCache->max[0];
         display.rms = &mWaveCache->rms[0];
         display.bl = &mWaveCache->bl[0];
         display.where = &mWaveCache->where[0];
         isLoadingOD = mWaveCache->numODPixels > 0;
         return true;
      }

      std::unique_ptr<WaveCache> oldCache(std::move(mWaveCache));

      int oldX0 = 0;
      double correction = 0.0;
      size_t copyBegin = 0, copyEnd = 0;
      if (match) {
         findCorrection(oldCache->where, oldCache->len, numPixels,
            t0, rate, samplesPerPixel,
            oldX0, correction);
         // Remember our first pixel maps to oldX0 in the old cache,
         // possibly out of bounds.
         // For what range of pixels can data be copied?
         copyBegin = std::min<size_t>(numPixels, std::max(0, -oldX0));
         copyEnd = std::min<size_t>(numPixels, std::max(0,
            (int)oldCache->len - oldX0
         ));
      }
      if (!(copyEnd > copyBegin))
         oldCache.reset(0);

      mWaveCache = std::make_unique<WaveCache>(numPixels, pixelsPerSecond, rate, t0, mDirty);
      min = &mWaveCache->min[0];
      max = &mWaveCache->max[0];
      rms = &mWaveCache->rms[0];
      bl = &mWaveCache->bl[0];
      pWhere = &mWaveCache->where;

      fillWhere(*pWhere, numPixels, 0.0, correction,
         t0, rate, samplesPerPixel);

      // The range of pixels we must fetch from the Sequence:
      p0 = (copyBegin > 0) ? 0 : copyEnd;
      p1 = (copyEnd >= numPixels) ? copyBegin : numPixels;

      // Optimization: if the old cache is good and overlaps
      // with the current one, re-use as much of the cache as
      // possible

      if (oldCache) {

         //TODO: only load inval regions if
         //necessary.  (usually is the case, so no rush.)
         //also, we should be updating the NEW cache, but here we are patching the old one up.
         oldCache->LoadInvalidRegions(sequence, false);
         oldCache->ClearInvalidRegions();

         // Copy what we can from the old cache.
         const int length = copyEnd - copyBegin;
         const size_t sizeFloats = length * sizeof(float);
         const int srcIdx = (int)copyBegin + oldX0;
         memcpy(&min[copyBegin], &oldCache->min[srcIdx], sizeFloats);
         memcpy(&max[copyBegin], &oldCache->max[srcIdx], sizeFloats);
         memcpy(&rms[copyBegin], &oldCache->rms[srcIdx], sizeFloats);
         memcpy(&bl[copyBegin], &oldCache->bl[srcIdx], length * sizeof(int));
      }
   }

   if (p1 > p0) {
      // Cache was not used or did not satisfy the whole request
      std::vector<sampleCount> &where = *pWhere;

      /* handle values in the append buffer */

      const auto sequence = clip.GetSequence();
      auto numSamples = sequence->GetNumSamples();
      auto a = p0;

      // Not all of the required columns might be in the sequence.
      // Some might be in the append buffer.
      for (; a < p1; ++a) {
         if (where[a + 1] > numSamples)
            break;
      }

      // Handle the columns that land in the append buffer.
      //compute the values that are outside the overlap from scratch.
      if (a < p1) {
         const auto appendBufferLen = clip.GetAppendBufferLen();
         const auto &appendBuffer = clip.GetAppendBuffer();
         sampleFormat seqFormat = sequence->GetSampleFormat();
         bool didUpdate = false;
         for(auto i = a; i < p1; i++) {
            auto left = std::max(sampleCount{ 0 },
                                 where[i] - numSamples);
            auto right = std::min(sampleCount{ appendBufferLen },
                                  where[i + 1] - numSamples);

            //wxCriticalSectionLocker locker(mAppendCriticalSection);

            if (right > left) {
               Floats b;
               const float *pb{};
               // left is nonnegative and at most mAppendBufferLen:
               auto sLeft = left.as_size_t();
               // The difference is at most mAppendBufferLen:
               size_t len = ( right - left ).as_size_t();

               if (seqFormat == floatSample)
                  pb = &((const float *)appendBuffer.ptr())[sLeft];
               else {
                  b.reinit(len);
                  pb = b.get();
                  CopySamples(appendBuffer.ptr() + sLeft * SAMPLE_SIZE(seqFormat),
                              seqFormat,
                              (samplePtr)pb, floatSample, len);
               }

               float theMax, theMin, sumsq;
               {
                  const float val = pb[0];
                  theMax = theMin = val;
                  sumsq = val * val;
               }
               for(decltype(len) j = 1; j < len; j++) {
                  const float val = pb[j];
                  theMax = std::max(theMax, val);
                  theMin = std::min(theMin, val);
                  sumsq += val * val;
               }

               min[i] = theMin;
               max[i] = theMax;
               rms[i] = (float)sqrt(sumsq / len);
               bl[i] = 1; //for now just fake it.

               didUpdate=true;
            }
         }

         // Shrink the right end of the range to fetch from Sequence
         if(didUpdate)
            p1 = a;
      }

      // Done with append buffer, now fetch the rest of the cache miss
      // from the sequence
      if (p1 > p0) {
         if (!::GetWaveDisplay(*sequence, &min[p0],
                                        &max[p0],
                                        &rms[p0],
                                        &bl[p0],
                                        p1-p0,
                                        &where[p0]))
         {
            isLoadingOD=false;
            return false;
         }
      }
   }

   //find the number of OD pixels - the only way to do this is by recounting
   if (!allocated) {
      // Now report the results
      display.min = min;
      display.max = max;
      display.rms = rms;
      display.bl = bl;
      display.where = &(*pWhere)[0];
      isLoadingOD = mWaveCache->numODPixels > 0;
   }
   else {
      using namespace std;
      isLoadingOD =
         count_if(display.ownBl.begin(), display.ownBl.end(),
                  bind2nd(less<int>(), 0)) > 0;
   }

   return true;
}

bool WaveClipDisplayCache::GetSpectrogram(const WaveClip &clip,
                              WaveTrackCache &waveTrackCache,
                              const float *& spectrogram,
                              const sampleCount *& where,
                              size_t numPixels,
                              double t0, double pixelsPerSecond)
{
   const WaveTrack *const track = waveTrackCache.GetTrack().get();
   auto &data = WaveTrackViewGroupData::Get( *track );
   const SpectrogramSettings &settings = data.GetSpectrogramSettings();
   const auto rate = clip.GetRate();

   bool match =
      mSpecCache &&
      mSpecCache->len > 0 &&
      mSpecCache->Matches
      (mDirty, pixelsPerSecond, settings, rate);

   if (match &&
       mSpecCache->start == t0 &&
       mSpecCache->len >= numPixels) {
      spectrogram = &mSpecCache->freq[0];
      where = &mSpecCache->where[0];

      return false;  //hit cache completely
   }

   // Caching is not implemented for reassignment, unless for
   // a complete hit, because of the complications of time reassignment
   if (settings.algorithm == SpectrogramSettings::algReassignment)
      match = false;

   // Free the cache when it won't cause a major stutter.
   // If the window size changed, we know there is nothing to be copied
   // If we zoomed out, or resized, we can give up memory. But not too much -
   // up to 2x extra is needed at the end of the clip to prevent stutter.
   if (mSpecCache->freq.capacity() > 2.1 * mSpecCache->freq.size() ||
       mSpecCache->windowSize*mSpecCache->zeroPaddingFactor <
       settings.WindowSize()*settings.ZeroPaddingFactor())
   {
      match = false;
      mSpecCache = std::make_unique<SpecCache>();
   }

   const double tstep = 1.0 / pixelsPerSecond;
   const double samplesPerPixel = rate * tstep;

   int oldX0 = 0;
   double correction = 0.0;

   int copyBegin = 0, copyEnd = 0;
   if (match) {
      findCorrection(mSpecCache->where, mSpecCache->len, numPixels,
         t0, rate, samplesPerPixel,
         oldX0, correction);
      // Remember our first pixel maps to oldX0 in the old cache,
      // possibly out of bounds.
      // For what range of pixels can data be copied?
      copyBegin = std::min((int)numPixels, std::max(0, -oldX0));
      copyEnd = std::min((int)numPixels, std::max(0,
         (int)mSpecCache->len - oldX0
      ));
   }

   // Resize the cache, keep the contents unchanged.
   mSpecCache->Grow(numPixels, settings, pixelsPerSecond, t0);
   auto nBins = settings.NBins();

   // Optimization: if the old cache is good and overlaps
   // with the current one, re-use as much of the cache as
   // possible
   if (copyEnd > copyBegin)
   {
      // memmove is required since dst/src overlap
      memmove(&mSpecCache->freq[nBins * copyBegin],
               &mSpecCache->freq[nBins * (copyBegin + oldX0)],
               nBins * (copyEnd - copyBegin) * sizeof(float));
   }

   // Reassignment accumulates, so it needs a zeroed buffer
   if (settings.algorithm == SpectrogramSettings::algReassignment)
   {
      // The cache could theoretically copy from the middle, resulting
      // in two regions to update. This won't happen in zoom, since
      // old cache doesn't match. It won't happen in resize, since the
      // spectrum view is pinned to left side of window.
      wxASSERT(
         (copyBegin >= 0 && copyEnd == (int)numPixels) || // copied the end
         (copyBegin == 0 && copyEnd <= (int)numPixels)    // copied the beginning
      );

      int zeroBegin = copyBegin > 0 ? 0 : copyEnd-copyBegin;
      int zeroEnd = copyBegin > 0 ? copyBegin : numPixels;

      memset(&mSpecCache->freq[nBins*zeroBegin], 0, nBins*(zeroEnd-zeroBegin)*sizeof(float));
   }

   // purposely offset the display 1/2 sample to the left (as compared
   // to waveform display) to properly center response of the FFT
   fillWhere(mSpecCache->where, numPixels, 0.5, correction,
      t0, rate, samplesPerPixel);

   mSpecCache->Populate
      (settings, waveTrackCache, copyBegin, copyEnd, numPixels,
       clip.GetSequence()->GetNumSamples(),
       clip.GetOffset(), rate, pixelsPerSecond);

   mSpecCache->dirty = mDirty;
   spectrogram = &mSpecCache->freq[0];
   where = &mSpecCache->where[0];

   return true;
}

void WaveClipDisplayCache::ClearDisplayRect()
{
   mDisplayRect.x = mDisplayRect.y = -1;
   mDisplayRect.width = mDisplayRect.height = -1;
}

void WaveClipDisplayCache::SetDisplayRect(const wxRect& r)
{
   mDisplayRect = r;
}

void WaveClipDisplayCache::GetDisplayRect(wxRect* r)
{
   *r = mDisplayRect;
}

WaveClipDisplayCache::WaveClipDisplayCache()
: mWaveCache{ std::make_unique<WaveCache>() }
, mSpecCache{ std::make_unique<SpecCache>() }
, mSpecPxCache{ std::make_unique<SpecPxCache>(1) }
{
}

WaveClipDisplayCache::~WaveClipDisplayCache()
{
}

static WaveClip::Caches::RegisteredFactory sKey{ []( WaveClip& ){
   return std::make_unique< WaveClipDisplayCache >();
} };

WaveClipDisplayCache &WaveClipDisplayCache::Get( const WaveClip &clip )
{
   return const_cast< WaveClip& >( clip )
      .Caches::Get< WaveClipDisplayCache >( sKey );
}

void WaveClipDisplayCache::MarkChanged()
{
   ++mDirty;
}

void WaveClipDisplayCache::Invalidate()
{
   // Invalidate wave display cache
   mWaveCache = std::make_unique<WaveCache>();
   // Invalidate the spectrum display cache
   mSpecCache = std::make_unique<SpecCache>();
}
