/**********************************************************************

Audacity: A Digital Audio Editor

WaveTrackVRulerControls.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../../../Audacity.h"
#include "WaveTrackViewGroupData.h"
#include "WaveTrackVRulerControls.h"

#include "WaveTrackVZoomHandle.h"

#include "../../../../HitTestResult.h"
#include "../../../../NumberScale.h"
#include "../../../../prefs/SpectrogramSettings.h"
#include "../../../../prefs/WaveformSettings.h"
#include "../../../../Project.h"
#include "../../../../RefreshCode.h"
#include "../../../../TrackPanelMouseEvent.h"
#include "../../../../WaveTrack.h"

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
