/**********************************************************************

  Audacity: A Digital Audio Editor

  WaveClip.cpp

  ?? Dominic Mazzoni
  ?? Markus Meyer

*******************************************************************//**

\class WaveClip
\brief This allows multiple clips to be a part of one WaveTrack.

*//*******************************************************************/

#include "WaveClip.h"

#include "Experimental.h"

#include <math.h>
#include "MemoryX.h"
#include <functional>
#include <vector>
#include <wx/log.h>

#include "Sequence.h"
#include "Prefs.h"
#include "Envelope.h"
#include "Resample.h"
#include "Project.h"
#include "WaveTrack.h"
#include "FFT.h"
#include "Profiler.h"
#include "InconsistencyException.h"
#include "UserException.h"

#include <wx/listimpl.cpp>

#ifdef _OPENMP
#include <omp.h>
#endif

WaveClipListener::~WaveClipListener()
{
}

WaveClip::WaveClip(const std::shared_ptr<DirManager> &projDirManager,
                   sampleFormat format, int rate, int colourIndex)
{
   mRate = rate;
   mColourIndex = colourIndex;
   mSequence = std::make_unique<Sequence>(projDirManager, format);

   mEnvelope = std::make_unique<Envelope>(true, 1e-7, 2.0, 1.0);
}

WaveClip::WaveClip(const WaveClip& orig,
                   const std::shared_ptr<DirManager> &projDirManager,
                   bool copyCutlines)
{
   // essentially a copy constructor - but you must pass in the
   // current project's DirManager, because we might be copying
   // from one project to another

   mOffset = orig.mOffset;
   mRate = orig.mRate;
   mColourIndex = orig.mColourIndex;
   mSequence = std::make_unique<Sequence>(*orig.mSequence, projDirManager);

   mEnvelope = std::make_unique<Envelope>(*orig.mEnvelope);

   if ( copyCutlines )
      for (const auto &clip: orig.mCutLines)
         mCutLines.push_back
            ( std::make_unique<WaveClip>( *clip, projDirManager, true ) );

   mIsPlaceholder = orig.GetIsPlaceholder();
}

WaveClip::WaveClip(const WaveClip& orig,
                   const std::shared_ptr<DirManager> &projDirManager,
                   bool copyCutlines,
                   double t0, double t1)
{
   // Copy only a range of the other WaveClip

   mOffset = orig.mOffset;
   mRate = orig.mRate;
   mColourIndex = orig.mColourIndex;

   mIsPlaceholder = orig.GetIsPlaceholder();

   sampleCount s0, s1;

   orig.TimeToSamplesClip(t0, &s0);
   orig.TimeToSamplesClip(t1, &s1);

   mSequence = orig.mSequence->Copy(s0, s1);

   mEnvelope = std::make_unique<Envelope>(
      *orig.mEnvelope,
      mOffset + s0.as_double()/mRate,
      mOffset + s1.as_double()/mRate
   );

   if ( copyCutlines )
      // Copy cutline clips that fall in the range
      for (const auto &ppClip : orig.mCutLines)
      {
         const WaveClip* clip = ppClip.get();
         double cutlinePosition = orig.mOffset + clip->GetOffset();
         if (cutlinePosition >= t0 && cutlinePosition <= t1)
         {
            auto newCutLine =
               std::make_unique< WaveClip >( *clip, projDirManager, true );
            newCutLine->SetOffset( cutlinePosition - t0 );
            mCutLines.push_back(std::move(newCutLine));
         }
      }
}


WaveClip::~WaveClip()
{
}

void WaveClip::SetOffset(double offset)
// NOFAIL-GUARANTEE
{
    mOffset = offset;
    mEnvelope->SetOffset(mOffset);
}

bool WaveClip::GetSamples(samplePtr buffer, sampleFormat format,
                   sampleCount start, size_t len, bool mayThrow) const
{
   return mSequence->Get(buffer, format, start, len, mayThrow);
}

void WaveClip::SetSamples(samplePtr buffer, sampleFormat format,
                   sampleCount start, size_t len)
// STRONG-GUARANTEE
{
   // use STRONG-GUARANTEE
   mSequence->SetSamples(buffer, format, start, len);

   // use NOFAIL-GUARANTEE
   MarkChanged();
}

BlockArray* WaveClip::GetSequenceBlockArray()
{
   return &mSequence->GetBlockArray();
}

void WaveClip::MarkChanged() // NOFAIL-GUARANTEE
{
   Caches::ForEach( std::mem_fn( &WaveClipListener::MarkChanged ) );
}

double WaveClip::GetStartTime() const
{
   // JS: mOffset is the minimum value and it is returned; no clipping to 0
   return mOffset;
}

double WaveClip::GetEndTime() const
{
   auto numSamples = mSequence->GetNumSamples();

   double maxLen = mOffset + (numSamples+mAppendBufferLen).as_double()/mRate;
   // JS: calculated value is not the length;
   // it is a maximum value and can be negative; no clipping to 0

   return maxLen;
}

sampleCount WaveClip::GetStartSample() const
{
   return sampleCount( floor(mOffset * mRate + 0.5) );
}

sampleCount WaveClip::GetEndSample() const
{
   return GetStartSample() + mSequence->GetNumSamples();
}

sampleCount WaveClip::GetNumSamples() const
{
   return mSequence->GetNumSamples();
}

bool WaveClip::WithinClip(double t) const
{
   auto ts = (sampleCount)floor(t * mRate + 0.5);
   return ts > GetStartSample() && ts < GetEndSample() + mAppendBufferLen;
}

bool WaveClip::BeforeClip(double t) const
{
   auto ts = (sampleCount)floor(t * mRate + 0.5);
   return ts <= GetStartSample();
}

bool WaveClip::AfterClip(double t) const
{
   auto ts = (sampleCount)floor(t * mRate + 0.5);
   return ts >= GetEndSample() + mAppendBufferLen;
}

std::pair<float, float> WaveClip::GetMinMax(
   double t0, double t1, bool mayThrow) const
{
   if (t0 > t1) {
      if (mayThrow)
         THROW_INCONSISTENCY_EXCEPTION;
      return {
         0.f,  // harmless, but unused since Sequence::GetMinMax does not use these values
         0.f   // harmless, but unused since Sequence::GetMinMax does not use these values
      };
   }

   if (t0 == t1)
      return{ 0.f, 0.f };

   sampleCount s0, s1;

   TimeToSamplesClip(t0, &s0);
   TimeToSamplesClip(t1, &s1);

   return mSequence->GetMinMax(s0, s1-s0, mayThrow);
}

float WaveClip::GetRMS(double t0, double t1, bool mayThrow) const
{
   if (t0 > t1) {
      if (mayThrow)
         THROW_INCONSISTENCY_EXCEPTION;
      return 0.f;
   }

   if (t0 == t1)
      return 0.f;

   sampleCount s0, s1;

   TimeToSamplesClip(t0, &s0);
   TimeToSamplesClip(t1, &s1);

   return mSequence->GetRMS(s0, s1-s0, mayThrow);
}

void WaveClip::ConvertToSampleFormat(sampleFormat format)
{
   // Note:  it is not necessary to do this recursively to cutlines.
   // They get converted as needed when they are expanded.

   auto bChanged = mSequence->ConvertToSampleFormat(format);
   if (bChanged)
      MarkChanged();
}

void WaveClip::UpdateEnvelopeTrackLen()
// NOFAIL-GUARANTEE
{
   mEnvelope->SetTrackLen
      ((mSequence->GetNumSamples().as_double()) / mRate, 1.0 / GetRate());
}

void WaveClip::TimeToSamplesClip(double t0, sampleCount *s0) const
{
   if (t0 < mOffset)
      *s0 = 0;
   else if (t0 > mOffset + mSequence->GetNumSamples().as_double()/mRate)
      *s0 = mSequence->GetNumSamples();
   else
      *s0 = sampleCount( floor(((t0 - mOffset) * mRate) + 0.5) );
}

void WaveClip::Append(samplePtr buffer, sampleFormat format,
                      size_t len, unsigned int stride /* = 1 */,
                      XMLWriter* blockFileLog /*=NULL*/)
// PARTIAL-GUARANTEE in case of exceptions:
// Some prefix (maybe none) of the buffer is appended, and no content already
// flushed to disk is lost.
{
   //wxLogDebug(wxT("Append: len=%lli"), (long long) len);

   auto maxBlockSize = mSequence->GetMaxBlockSize();
   auto blockSize = mSequence->GetIdealAppendLen();
   sampleFormat seqFormat = mSequence->GetSampleFormat();

   if (!mAppendBuffer.ptr())
      mAppendBuffer.Allocate(maxBlockSize, seqFormat);

   auto cleanup = finally( [&] {
      // use NOFAIL-GUARANTEE
      UpdateEnvelopeTrackLen();
      MarkChanged();
   } );

   for(;;) {
      if (mAppendBufferLen >= blockSize) {
         // flush some previously appended contents
         // use STRONG-GUARANTEE
         mSequence->Append(mAppendBuffer.ptr(), seqFormat, blockSize,
                           blockFileLog);

         // use NOFAIL-GUARANTEE for rest of this "if"
         memmove(mAppendBuffer.ptr(),
                 mAppendBuffer.ptr() + blockSize * SAMPLE_SIZE(seqFormat),
                 (mAppendBufferLen - blockSize) * SAMPLE_SIZE(seqFormat));
         mAppendBufferLen -= blockSize;
         blockSize = mSequence->GetIdealAppendLen();
      }

      if (len == 0)
         break;

      // use NOFAIL-GUARANTEE for rest of this "for"
      wxASSERT(mAppendBufferLen <= maxBlockSize);
      auto toCopy = std::min(len, maxBlockSize - mAppendBufferLen);

      CopySamples(buffer, format,
                  mAppendBuffer.ptr() + mAppendBufferLen * SAMPLE_SIZE(seqFormat),
                  seqFormat,
                  toCopy,
                  true, // high quality
                  stride);

      mAppendBufferLen += toCopy;
      buffer += toCopy * SAMPLE_SIZE(format) * stride;
      len -= toCopy;
   }
}

void WaveClip::AppendAlias(const FilePath &fName, sampleCount start,
                            size_t len, int channel,bool useOD)
// STRONG-GUARANTEE
{
   // use STRONG-GUARANTEE
   mSequence->AppendAlias(fName, start, len, channel,useOD);

   // use NOFAIL-GUARANTEE
   UpdateEnvelopeTrackLen();
   MarkChanged();
}

void WaveClip::AppendCoded(const FilePath &fName, sampleCount start,
                            size_t len, int channel, int decodeType)
// STRONG-GUARANTEE
{
   // use STRONG-GUARANTEE
   mSequence->AppendCoded(fName, start, len, channel, decodeType);

   // use NOFAIL-GUARANTEE
   UpdateEnvelopeTrackLen();
   MarkChanged();
}

void WaveClip::Flush()
// NOFAIL-GUARANTEE that the clip will be in a flushed state.
// PARTIAL-GUARANTEE in case of exceptions:
// Some initial portion (maybe none) of the append buffer of the
// clip gets appended; no previously flushed contents are lost.
{
   //wxLogDebug(wxT("WaveClip::Flush"));
   //wxLogDebug(wxT("   mAppendBufferLen=%lli"), (long long) mAppendBufferLen);
   //wxLogDebug(wxT("   previous sample count %lli"), (long long) mSequence->GetNumSamples());

   if (mAppendBufferLen > 0) {

      auto cleanup = finally( [&] {
         // Blow away the append buffer even in case of failure.  May lose some
         // data but don't leave the track in an un-flushed state.

         // Use NOFAIL-GUARANTEE of these steps.
         mAppendBufferLen = 0;
         UpdateEnvelopeTrackLen();
         MarkChanged();
      } );

      mSequence->Append(mAppendBuffer.ptr(), mSequence->GetSampleFormat(),
         mAppendBufferLen);
   }

   //wxLogDebug(wxT("now sample count %lli"), (long long) mSequence->GetNumSamples());
}

bool WaveClip::HandleXMLTag(const wxChar *tag, const wxChar **attrs)
{
   if (!wxStrcmp(tag, wxT("waveclip")))
   {
      double dblValue;
      long longValue;
      while (*attrs)
      {
         const wxChar *attr = *attrs++;
         const wxChar *value = *attrs++;

         if (!value)
            break;

         const wxString strValue = value;
         if (!wxStrcmp(attr, wxT("offset")))
         {
            if (!XMLValueChecker::IsGoodString(strValue) ||
                  !Internat::CompatibleToDouble(strValue, &dblValue))
               return false;
            SetOffset(dblValue);
         }
         if (!wxStrcmp(attr, wxT("colorindex")))
         {
            if (!XMLValueChecker::IsGoodString(strValue) ||
                  !strValue.ToLong( &longValue))
               return false;
            SetColourIndex(longValue);
         }
      }
      return true;
   }

   return false;
}

void WaveClip::HandleXMLEndTag(const wxChar *tag)
{
   if (!wxStrcmp(tag, wxT("waveclip")))
      UpdateEnvelopeTrackLen();
}

XMLTagHandlerPtr WaveClip::HandleXMLChild(const wxChar *tag)
{
   if (!wxStrcmp(tag, wxT("sequence")))
      return mSequence.get();
   else if (!wxStrcmp(tag, wxT("envelope")))
      return mEnvelope.get();
   else if (!wxStrcmp(tag, wxT("waveclip")))
   {
      // Nested wave clips are cut lines
      mCutLines.push_back(
         std::make_unique<WaveClip>(mSequence->GetDirManager(),
            mSequence->GetSampleFormat(), mRate, 0 /*colourindex*/));
      return mCutLines.back().get();
   }
   else
      return NULL;
}

void WaveClip::WriteXML(XMLWriter &xmlFile) const
// may throw
{
   xmlFile.StartTag(wxT("waveclip"));
   xmlFile.WriteAttr(wxT("offset"), mOffset, 8);
   xmlFile.WriteAttr(wxT("colorindex"), mColourIndex );

   mSequence->WriteXML(xmlFile);
   mEnvelope->WriteXML(xmlFile);

   for (const auto &clip: mCutLines)
      clip->WriteXML(xmlFile);

   xmlFile.EndTag(wxT("waveclip"));
}

void WaveClip::Paste(double t0, const WaveClip* other)
// STRONG-GUARANTEE
{
   const bool clipNeedsResampling = other->mRate != mRate;
   const bool clipNeedsNewFormat =
      other->mSequence->GetSampleFormat() != mSequence->GetSampleFormat();
   std::unique_ptr<WaveClip> newClip;
   const WaveClip* pastedClip;

   if (clipNeedsResampling || clipNeedsNewFormat)
   {
      newClip =
         std::make_unique<WaveClip>(*other, mSequence->GetDirManager(), true);
      if (clipNeedsResampling)
         // The other clip's rate is different from ours, so resample
         newClip->Resample(mRate);
      if (clipNeedsNewFormat)
         // Force sample formats to match.
         newClip->ConvertToSampleFormat(mSequence->GetSampleFormat());
      pastedClip = newClip.get();
   }
   else
   {
      // No resampling or format change needed, just use original clip without making a copy
      pastedClip = other;
   }

   // Paste cut lines contained in pasted clip
   WaveClipHolders newCutlines;
   for (const auto &cutline: pastedClip->mCutLines)
   {
      newCutlines.push_back(
         std::make_unique<WaveClip>
            ( *cutline, mSequence->GetDirManager(),
              // Recursively copy cutlines of cutlines.  They don't need
              // their offsets adjusted.
              true));
      newCutlines.back()->Offset(t0 - mOffset);
   }

   sampleCount s0;
   TimeToSamplesClip(t0, &s0);

   // Assume STRONG-GUARANTEE from Sequence::Paste
   mSequence->Paste(s0, pastedClip->mSequence.get());

   // Assume NOFAIL-GUARANTEE in the remaining
   MarkChanged();
   auto sampleTime = 1.0 / GetRate();
   mEnvelope->PasteEnvelope
      (s0.as_double()/mRate + mOffset, pastedClip->mEnvelope.get(), sampleTime);
   OffsetCutLines(t0, pastedClip->GetEndTime() - pastedClip->GetStartTime());

   for (auto &holder : newCutlines)
      mCutLines.push_back(std::move(holder));
}

void WaveClip::InsertSilence( double t, double len, double *pEnvelopeValue )
// STRONG-GUARANTEE
{
   sampleCount s0;
   TimeToSamplesClip(t, &s0);
   auto slen = (sampleCount)floor(len * mRate + 0.5);

   // use STRONG-GUARANTEE
   GetSequence()->InsertSilence(s0, slen);

   // use NOFAIL-GUARANTEE
   OffsetCutLines(t, len);

   const auto sampleTime = 1.0 / GetRate();
   auto pEnvelope = GetEnvelope();
   if ( pEnvelopeValue ) {

      // Preserve limit value at the end
      auto oldLen = pEnvelope->GetTrackLen();
      auto newLen = oldLen + len;
      pEnvelope->Cap( sampleTime );

      // Ramp across the silence to the given value
      pEnvelope->SetTrackLen( newLen, sampleTime );
      pEnvelope->InsertOrReplace
         ( pEnvelope->GetOffset() + newLen, *pEnvelopeValue );
   }
   else
      pEnvelope->InsertSpace( t, len );

   MarkChanged();
}

void WaveClip::AppendSilence( double len, double envelopeValue )
// STRONG-GUARANTEE
{
   auto t = GetEndTime();
   InsertSilence( t, len, &envelopeValue );
}

void WaveClip::Clear(double t0, double t1)
// STRONG-GUARANTEE
{
   sampleCount s0, s1;

   TimeToSamplesClip(t0, &s0);
   TimeToSamplesClip(t1, &s1);

   // use STRONG-GUARANTEE
   GetSequence()->Delete(s0, s1-s0);

   // use NOFAIL-GUARANTEE in the remaining

   // msmeyer
   //
   // Delete all cutlines that are within the given area, if any.
   //
   // Note that when cutlines are active, two functions are used:
   // Clear() and ClearAndAddCutLine(). ClearAndAddCutLine() is called
   // whenever the user directly calls a command that removes some audio, e.g.
   // "Cut" or "Clear" from the menu. This command takes care about recursive
   // preserving of cutlines within clips. Clear() is called when internal
   // operations want to remove audio. In the latter case, it is the right
   // thing to just remove all cutlines within the area.
   //
   double clip_t0 = t0;
   double clip_t1 = t1;
   if (clip_t0 < GetStartTime())
      clip_t0 = GetStartTime();
   if (clip_t1 > GetEndTime())
      clip_t1 = GetEndTime();

   // May DELETE as we iterate, so don't use range-for
   for (auto it = mCutLines.begin(); it != mCutLines.end();)
   {
      WaveClip* clip = it->get();
      double cutlinePosition = mOffset + clip->GetOffset();
      if (cutlinePosition >= t0 && cutlinePosition <= t1)
      {
         // This cutline is within the area, DELETE it
         it = mCutLines.erase(it);
      }
      else
      {
         if (cutlinePosition >= t1)
         {
            clip->Offset(clip_t0 - clip_t1);
         }
         ++it;
      }
   }

   // Collapse envelope
   auto sampleTime = 1.0 / GetRate();
   GetEnvelope()->CollapseRegion( t0, t1, sampleTime );
   if (t0 < GetStartTime())
      Offset(-(GetStartTime() - t0));

   MarkChanged();
}

void WaveClip::ClearAndAddCutLine(double t0, double t1)
// WEAK-GUARANTEE
// this WaveClip remains destructible in case of AudacityException.
// But some cutlines may be deleted
{
   if (t0 > GetEndTime() || t1 < GetStartTime())
      return; // time out of bounds

   const double clip_t0 = std::max( t0, GetStartTime() );
   const double clip_t1 = std::min( t1, GetEndTime() );

   auto newClip = std::make_unique< WaveClip >
      (*this, mSequence->GetDirManager(), true, clip_t0, clip_t1);

   newClip->SetOffset( clip_t0 - mOffset );

   // Remove cutlines from this clip that were in the selection, shift
   // left those that were after the selection
   // May DELETE as we iterate, so don't use range-for
   for (auto it = mCutLines.begin(); it != mCutLines.end();)
   {
      WaveClip* clip = it->get();
      double cutlinePosition = mOffset + clip->GetOffset();
      if (cutlinePosition >= t0 && cutlinePosition <= t1)
         it = mCutLines.erase(it);
      else
      {
         if (cutlinePosition >= t1)
         {
            clip->Offset(clip_t0 - clip_t1);
         }
         ++it;
      }
   }

   // Clear actual audio data
   sampleCount s0, s1;

   TimeToSamplesClip(t0, &s0);
   TimeToSamplesClip(t1, &s1);

   // use WEAK-GUARANTEE
   GetSequence()->Delete(s0, s1-s0);

   // Collapse envelope
   auto sampleTime = 1.0 / GetRate();
   GetEnvelope()->CollapseRegion( t0, t1, sampleTime );
   if (t0 < GetStartTime())
      Offset(-(GetStartTime() - t0));

   MarkChanged();

   mCutLines.push_back(std::move(newClip));
}

bool WaveClip::FindCutLine(double cutLinePosition,
                           double* cutlineStart /* = NULL */,
                           double* cutlineEnd /* = NULL */) const
{
   for (const auto &cutline: mCutLines)
   {
      if (fabs(mOffset + cutline->GetOffset() - cutLinePosition) < 0.0001)
      {
         if (cutlineStart)
            *cutlineStart = mOffset+cutline->GetStartTime();
         if (cutlineEnd)
            *cutlineEnd = mOffset+cutline->GetEndTime();
         return true;
      }
   }

   return false;
}

void WaveClip::ExpandCutLine(double cutLinePosition)
// STRONG-GUARANTEE
{
   auto end = mCutLines.end();
   auto it = std::find_if( mCutLines.begin(), end,
      [&](const WaveClipHolder &cutline) {
         return fabs(mOffset + cutline->GetOffset() - cutLinePosition) < 0.0001;
      } );

   if ( it != end ) {
      auto cutline = it->get();
      // assume STRONG-GUARANTEE from Paste

      // Envelope::Paste takes offset into account, WaveClip::Paste doesn't!
      // Do this to get the right result:
      cutline->mEnvelope->SetOffset(0);

      Paste(mOffset+cutline->GetOffset(), cutline);
      // Now erase the cutline,
      // but be careful to find it again, because Paste above may
      // have modified the array of cutlines (if our cutline contained
      // another cutline!), invalidating the iterator we had.
      end = mCutLines.end();
      it = std::find_if(mCutLines.begin(), end,
         [=](const WaveClipHolder &p) { return p.get() == cutline; });
      if (it != end)
         mCutLines.erase(it); // deletes cutline!
      else {
         wxASSERT(false);
      }
   }
}

bool WaveClip::RemoveCutLine(double cutLinePosition)
{
   for (auto it = mCutLines.begin(); it != mCutLines.end(); ++it)
   {
      const auto &cutline = *it;
      if (fabs(mOffset + cutline->GetOffset() - cutLinePosition) < 0.0001)
      {
         mCutLines.erase(it); // deletes cutline!
         return true;
      }
   }

   return false;
}

void WaveClip::OffsetCutLines(double t0, double len)
// NOFAIL-GUARANTEE
{
   for (const auto &cutLine : mCutLines)
   {
      if (mOffset + cutLine->GetOffset() >= t0)
         cutLine->Offset(len);
   }
}

void WaveClip::Lock()
{
   GetSequence()->Lock();
   for (const auto &cutline: mCutLines)
      cutline->Lock();
}

void WaveClip::CloseLock()
{
   GetSequence()->CloseLock();
   for (const auto &cutline: mCutLines)
      cutline->CloseLock();
}

void WaveClip::Unlock()
{
   GetSequence()->Unlock();
   for (const auto &cutline: mCutLines)
      cutline->Unlock();
}

void WaveClip::SetRate(int rate)
{
   mRate = rate;
   auto newLength = mSequence->GetNumSamples().as_double() / mRate;
   mEnvelope->RescaleTimes( newLength );
   MarkChanged();
}

void WaveClip::Resample(int rate, ProgressDialog *progress)
// STRONG-GUARANTEE
{
   // Note:  it is not necessary to do this recursively to cutlines.
   // They get resampled as needed when they are expanded.

   if (rate == mRate)
      return; // Nothing to do

   double factor = (double)rate / (double)mRate;
   ::Resample resample(true, factor, factor); // constant rate resampling

   const size_t bufsize = 65536;
   Floats inBuffer{ bufsize };
   Floats outBuffer{ bufsize };
   sampleCount pos = 0;
   bool error = false;
   int outGenerated = 0;
   auto numSamples = mSequence->GetNumSamples();

   auto newSequence =
      std::make_unique<Sequence>(mSequence->GetDirManager(), mSequence->GetSampleFormat());

   /**
    * We want to keep going as long as we have something to feed the resampler
    * with OR as long as the resampler spews out samples (which could continue
    * for a few iterations after we stop feeding it)
    */
   while (pos < numSamples || outGenerated > 0)
   {
      const auto inLen = limitSampleBufferSize( bufsize, numSamples - pos );

      bool isLast = ((pos + inLen) == numSamples);

      if (!mSequence->Get((samplePtr)inBuffer.get(), floatSample, pos, inLen, true))
      {
         error = true;
         break;
      }

      const auto results = resample.Process(factor, inBuffer.get(), inLen, isLast,
                                            outBuffer.get(), bufsize);
      outGenerated = results.second;

      pos += results.first;

      if (outGenerated < 0)
      {
         error = true;
         break;
      }

      newSequence->Append((samplePtr)outBuffer.get(), floatSample,
                          outGenerated);

      if (progress)
      {
         auto updateResult = progress->Update(
            pos.as_long_long(),
            numSamples.as_long_long()
         );
         error = (updateResult != ProgressResult::Success);
         if (error)
            throw UserException{};
      }
   }

   if (error)
      throw SimpleMessageBoxException{
         _("Resampling failed.")
      };
   else
   {
      // Use NOFAIL-GUARANTEE in these steps
      mSequence = std::move(newSequence);
      mRate = rate;
      Caches::ForEach( std::mem_fn( &WaveClipListener::Invalidate ) );
   }
}

// Used by commands which interact with clips using the keyboard.
// When two clips are immediately next to each other, the GetEndTime()
// of the first clip and the GetStartTime() of the second clip may not
// be exactly equal due to rounding errors.
bool WaveClip::SharesBoundaryWithNextClip(const WaveClip* next) const
{
   double endThis = GetRate() * GetOffset() + GetNumSamples().as_double();
   double startNext = next->GetRate() * next->GetOffset();

   // given that a double has about 15 significant digits, using a criterion
   // of half a sample should be safe in all normal usage.
   return fabs(startNext - endThis) < 0.5;
}
