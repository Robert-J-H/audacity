/**********************************************************************

Audacity: A Digital Audio Editor

WaveTrackVRulerControls.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../../../Audacity.h"
#include "WaveTrackViewGroupData.h"
#include "WaveTrackVRulerControls.h"

#include "WaveTrackVZoomHandle.h"

#include "../../../../Experimental.h"

#include "../../../../HitTestResult.h"
#include "../../../../NumberScale.h"
#include "../../../../prefs/SpectrogramSettings.h"
#include "../../../../prefs/WaveformSettings.h"
#include "../../../../Project.h"
#include "../../../../RefreshCode.h"
#include "../../../../TrackPanelMouseEvent.h"
#include "../../../../WaveTrack.h"

#include "../../../../AColor.h"
#include "../../../../AllThemeResources.h"
#include "../../../../TrackArtist.h"
#include "../../../../TrackPanelDrawingContext.h"
#include "../../../../widgets/Ruler.h"

///////////////////////////////////////////////////////////////////////////////
WaveTrackVRulerControls::~WaveTrackVRulerControls()
{
}

std::vector<UIHandlePtr> WaveTrackVRulerControls::HitTest
(const TrackPanelMouseState &st,
 const AudacityProject *pProject)
{
   std::vector<UIHandlePtr> results;

   if ( st.state.GetX() <= st.rect.GetRight() - kGuard ) {
      auto pTrack = FindTrack()->SharedPointer<WaveTrack>(  );
      if (pTrack) {
         auto result = std::make_shared<WaveTrackVZoomHandle>(
            pTrack, st.rect, st.state.m_y );
         result = AssignUIHandlePtr(mVZoomHandle, result);
         results.push_back(result);
      }
   }

   auto more = TrackVRulerControls::HitTest(st, pProject);
   std::copy(more.begin(), more.end(), std::back_inserter(results));

   return results;
}

unsigned WaveTrackVRulerControls::HandleWheelRotation
(const TrackPanelMouseEvent &evt, AudacityProject *pProject)
{
   using namespace RefreshCode;
   const wxMouseEvent &event = evt.event;

   if (!(event.ShiftDown() || event.CmdDown()))
      return RefreshNone;

   // Always stop propagation even if the ruler didn't change.  The ruler
   // is a narrow enough target.
   evt.event.Skip(false);

   const auto pTrack = FindTrack();
   if (!pTrack)
      return RefreshNone;
   const auto wt = static_cast<WaveTrack*>(pTrack.get());
   auto &data = WaveTrackViewGroupData::Get( *wt );
   auto steps = evt.steps;

   const bool isDB =
      data.GetDisplay() == WaveTrackViewConstants::Waveform &&
      data.GetWaveformSettings().scaleType == WaveformSettings::stLogarithmic;
   // Special cases for Waveform dB only.
   // Set the bottom of the dB scale but only if it's visible
   if (isDB && event.ShiftDown() && event.CmdDown()) {
      float min, max;
      data.GetDisplayBounds(&min, &max);
      if (!(min < 0.0 && max > 0.0))
         return RefreshNone;

      WaveformSettings &settings =
         data.GetIndependentWaveformSettings();
      float olddBRange = settings.dBRange;
      WaveformSettings &channelSettings =
         data.GetIndependentWaveformSettings();
      if (steps < 0)
         // Zoom out
         channelSettings.NextLowerDBRange();
      else
         channelSettings.NextHigherDBRange();

      float newdBRange = settings.dBRange;

      // Is y coordinate within the rectangle half-height centered about
      // the zero level?
      const auto &rect = evt.rect;
      const auto zeroLevel = data.ZeroLevelYCoordinate(rect);
      const bool fixedMagnification =
      (4 * std::abs(event.GetY() - zeroLevel) < rect.GetHeight());

      if (fixedMagnification) {
         // Vary the db limit without changing
         // magnification; that is, peaks and troughs move up and down
         // rigidly, as parts of the wave near zero are exposed or hidden.
         const float extreme = (LINEAR_TO_DB(2) + newdBRange) / newdBRange;
         max = std::min(extreme, max * olddBRange / newdBRange);
         min = std::max(-extreme, min * olddBRange / newdBRange);
         data.SetLastdBRange();
         data.SetDisplayBounds(min, max);
      }
   }
   else if (event.CmdDown() && !event.ShiftDown()) {
      const int yy = event.m_y;
      using namespace WaveTrackViewConstants;
      WaveTrackViewGroupData::Get( *wt ).DoZoom(
         wt->GetRate(),
         (steps < 0)
            ? kZoomOut
            : kZoomIn,
         evt.rect, yy, yy, true);
      if( pProject )
         pProject->ModifyState(true);
   }
   else if (!event.CmdDown() && event.ShiftDown()) {
      // Scroll some fixed number of pixels, independent of zoom level or track height:
      static const float movement = 10.0f;
      const int height = evt.rect.GetHeight();
      const bool spectral =
         (data.GetDisplay() == WaveTrackViewConstants::Spectrum);
      if (spectral) {
         const float delta = steps * movement / height;
         SpectrogramSettings &settings = data.GetIndependentSpectrogramSettings();
         const bool isLinear = settings.scaleType == SpectrogramSettings::stLinear;
         float bottom, top;
         data.GetSpectrumBounds(wt->GetRate(), &bottom, &top);
         const double rate = wt->GetRate();
         const float bound = rate / 2;
         const NumberScale numberScale(settings.GetScale(bottom, top));
         float newTop =
            std::min(bound, numberScale.PositionToValue(1.0f + delta));
         const float newBottom =
            std::max((isLinear ? 0.0f : 1.0f),
                     numberScale.PositionToValue(numberScale.ValueToPosition(newTop) - 1.0f));
         newTop =
            std::min(bound,
                     numberScale.PositionToValue(numberScale.ValueToPosition(newBottom) + 1.0f));

         data.SetSpectrumBounds(newBottom, newTop);
      }
      else {
         float topLimit = 2.0;
         if (isDB) {
            const float dBRange = data.GetWaveformSettings().dBRange;
            topLimit = (LINEAR_TO_DB(topLimit) + dBRange) / dBRange;
         }
         const float bottomLimit = -topLimit;
         float top, bottom;
         data.GetDisplayBounds(&bottom, &top);
         const float range = top - bottom;
         const float delta = range * steps * movement / height;
         float newTop = std::min(topLimit, top + delta);
         const float newBottom = std::max(bottomLimit, newTop - range);
         newTop = std::min(topLimit, newBottom + range);
         data.SetDisplayBounds(newBottom, newTop);
      }
   }
   else
      return RefreshNone;

   pProject->ModifyState(true);

   return RefreshCell | UpdateVRuler;
}

namespace {
   Ruler &ruler()
   {
      static Ruler theRuler;
      return theRuler;
   }
}

void WaveTrackVRulerControls::Draw(
   TrackPanelDrawingContext &context,
   const wxRect &rect_, unsigned iPass )
{
   TrackVRulerControls::Draw( context, rect_, iPass );

   // Draw on a later pass because the bevel overpaints one pixel
   // out of bounds on the bottom

   if ( iPass == TrackArtist::PassControls ) {
      auto rect = rect_;
      --rect.width;
      --rect.height;

      auto dc = &context.dc;

      // All waves have a ruler in the info panel
      // The ruler needs a bevelled surround.
      wxRect bev = rect;
      bev.Inflate(-1, 0);
      bev.width += 1;

      bool highlight = false;
#ifdef EXPERIMENTAL_TRACK_PANEL_HIGHLIGHTING
      highlight = rect.Contains(context.lastState.GetPosition());
#endif
   
      AColor::BevelTrackInfo(*dc, true, bev, highlight);
      
      // Right align the ruler
      wxRect rr = rect;
      rr.width--;
      
      auto t = FindTrack();
      if ( !t )
         return;

      if ( t->vrulerSize.GetWidth() < rect.GetWidth()) {
         int adj = rr.GetWidth() - t->vrulerSize.GetWidth();
         rr.x += adj;
         rr.width -= adj;
      }
      
      UpdateRuler(rr);
      
      auto vruler = &ruler();

      vruler->SetTickColour( theTheme.Colour( clrTrackPanelText ));
      vruler->Draw(*dc);
   }
}

void WaveTrackVRulerControls::UpdateRuler( const wxRect &rect )
{
   const auto wt = std::static_pointer_cast< WaveTrack >( FindTrack() );
   if (!wt)
      return;
   auto &data = WaveTrackViewGroupData::Get( *wt );
   auto vruler = &ruler();

   // All waves have a ruler in the info panel
   // The ruler needs a bevelled surround.
   const float dBRange =
      data.GetWaveformSettings().dBRange;
   
   const int display = data.GetDisplay();
   
   if (display == WaveTrackViewConstants::Waveform) {
      WaveformSettings::ScaleType scaleType =
      data.GetWaveformSettings().scaleType;
      
      if (scaleType == WaveformSettings::stLinear) {
         // Waveform
         
         float min, max;
         data.GetDisplayBounds(&min, &max);
         if (data.GetLastScaleType() != scaleType &&
             data.GetLastScaleType() != -1)
         {
            // do a translation into the linear space
            data.SetLastScaleType();
            data.SetLastdBRange();
            float sign = (min >= 0 ? 1 : -1);
            if (min != 0.) {
               min = DB_TO_LINEAR(fabs(min) * dBRange - dBRange);
               if (min < 0.0)
                  min = 0.0;
               min *= sign;
            }
            sign = (max >= 0 ? 1 : -1);
            
            if (max != 0.) {
               max = DB_TO_LINEAR(fabs(max) * dBRange - dBRange);
               if (max < 0.0)
                  max = 0.0;
               max *= sign;
            }
            data.SetDisplayBounds(min, max);
         }
         
         vruler->SetDbMirrorValue( 0.0 );
         vruler->SetBounds(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height - 1);
         vruler->SetOrientation(wxVERTICAL);
         vruler->SetRange(max, min);
         vruler->SetFormat(Ruler::RealFormat);
         vruler->SetUnits(wxT(""));
         vruler->SetLabelEdges(false);
         vruler->SetLog(false);
      }
      else {
         wxASSERT(scaleType == WaveformSettings::stLogarithmic);
         scaleType = WaveformSettings::stLogarithmic;
         
         vruler->SetUnits(wxT(""));
         
         float min, max;
         data.GetDisplayBounds(&min, &max);
         float lastdBRange;
         
         if (data.GetLastScaleType() != scaleType &&
             data.GetLastScaleType() != -1)
         {
            // do a translation into the dB space
            data.SetLastScaleType();
            data.SetLastdBRange();
            float sign = (min >= 0 ? 1 : -1);
            if (min != 0.) {
               min = (LINEAR_TO_DB(fabs(min)) + dBRange) / dBRange;
               if (min < 0.0)
                  min = 0.0;
               min *= sign;
            }
            sign = (max >= 0 ? 1 : -1);
            
            if (max != 0.) {
               max = (LINEAR_TO_DB(fabs(max)) + dBRange) / dBRange;
               if (max < 0.0)
                  max = 0.0;
               max *= sign;
            }
            data.SetDisplayBounds(min, max);
         }
         else if (dBRange != (lastdBRange = data.GetLastdBRange())) {
            data.SetLastdBRange();
            // Remap the max of the scale
            float newMax = max;
            
            // This commented out code is problematic.
            // min and max may be correct, and this code cause them to change.
#ifdef ONLY_LABEL_POSITIVE
            const float sign = (max >= 0 ? 1 : -1);
            if (max != 0.) {
               
               // Ugh, duplicating from TrackPanel.cpp
#define ZOOMLIMIT 0.001f
               
               const float extreme = LINEAR_TO_DB(2);
               // recover dB value of max
               const float dB = std::min(extreme, (float(fabs(max)) * lastdBRange - lastdBRange));
               // find NEW scale position, but old max may get trimmed if the db limit rises
               // Don't trim it to zero though, but leave max and limit distinct
               newMax = sign * std::max(ZOOMLIMIT, (dBRange + dB) / dBRange);
               // Adjust the min of the scale if we can,
               // so the db Limit remains where it was on screen, but don't violate extremes
               if (min != 0.)
                  min = std::max(-extreme, newMax * min / max);
            }
#endif
            data.SetDisplayBounds(min, newMax);
         }
         
         // Old code was if ONLY_LABEL_POSITIVE were defined.
         // it uses the +1 to 0 range only.
         // the enabled code uses +1 to -1, and relies on set ticks labelling knowing about
         // the dB scale.
#ifdef ONLY_LABEL_POSITIVE
         if (max > 0) {
#endif
            int top = 0;
            float topval = 0;
            int bot = rect.height;
            float botval = -dBRange;
            
#ifdef ONLY_LABEL_POSITIVE
            if (min < 0) {
               bot = top + (int)((max / (max - min))*(bot - top));
               min = 0;
            }
            
            if (max > 1) {
               top += (int)((max - 1) / (max - min) * (bot - top));
               max = 1;
            }
            
            if (max < 1 && max > 0)
               topval = -((1 - max) * dBRange);
            
            if (min > 0) {
               botval = -((1 - min) * dBRange);
            }
#else
            topval = -((1 - max) * dBRange);
            botval = -((1 - min) * dBRange);
            vruler->SetDbMirrorValue( dBRange );
#endif
            vruler->SetBounds(rect.x, rect.y + top, rect.x + rect.width, rect.y + bot - 1);
            vruler->SetOrientation(wxVERTICAL);
            vruler->SetRange(topval, botval);
#ifdef ONLY_LABEL_POSITIVE
         }
         else
            vruler->SetBounds(0.0, 0.0, 0.0, 0.0); // A.C.H I couldn't find a way to just disable it?
#endif
         vruler->SetFormat(Ruler::RealLogFormat);
         vruler->SetLabelEdges(true);
         vruler->SetLog(false);
      }
   }
   else {
      wxASSERT(display == WaveTrackViewConstants::Spectrum);
      const SpectrogramSettings &settings = data.GetSpectrogramSettings();
      float minFreq, maxFreq;
      data.GetSpectrumBounds(wt->GetRate(), &minFreq, &maxFreq);
      vruler->SetDbMirrorValue( 0.0 );
      
      switch (settings.scaleType) {
         default:
            wxASSERT(false);
         case SpectrogramSettings::stLinear:
         {
            // Spectrum
            
            if (rect.height < 60)
               return;
            
            /*
             draw the ruler
             we will use Hz if maxFreq is < 2000, otherwise we represent kHz,
             and append to the numbers a "k"
             */
            vruler->SetBounds(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height - 1);
            vruler->SetOrientation(wxVERTICAL);
            vruler->SetFormat(Ruler::RealFormat);
            vruler->SetLabelEdges(true);
            // use kHz in scale, if appropriate
            if (maxFreq >= 2000) {
               vruler->SetRange((maxFreq / 1000.), (minFreq / 1000.));
               vruler->SetUnits(wxT("k"));
            }
            else {
               // use Hz
               vruler->SetRange((int)(maxFreq), (int)(minFreq));
               vruler->SetUnits(wxT(""));
            }
            vruler->SetLog(false);
         }
            break;
         case SpectrogramSettings::stLogarithmic:
         case SpectrogramSettings::stMel:
         case SpectrogramSettings::stBark:
         case SpectrogramSettings::stErb:
         case SpectrogramSettings::stPeriod:
         {
            // SpectrumLog
            
            if (rect.height < 10)
               return;
            
            /*
             draw the ruler
             we will use Hz if maxFreq is < 2000, otherwise we represent kHz,
             and append to the numbers a "k"
             */
            vruler->SetBounds(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height - 1);
            vruler->SetOrientation(wxVERTICAL);
            vruler->SetFormat(Ruler::IntFormat);
            vruler->SetLabelEdges(true);
            vruler->SetRange(maxFreq, minFreq);
            vruler->SetUnits(wxT(""));
            vruler->SetLog(true);
            NumberScale scale(
               data.GetSpectrogramSettings().GetScale( minFreq, maxFreq )
                  .Reversal() );
            vruler->SetNumberScale(&scale);
         }
            break;
      }
   }

   vruler->GetMaxSize( &wt->vrulerSize.x, &wt->vrulerSize.y );
}
