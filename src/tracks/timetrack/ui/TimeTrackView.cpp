/**********************************************************************

Audacity: A Digital Audio Editor

TimeTrackView.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "TimeTrackView.h"
#include "../../../TimeTrack.h"

#include "../../../Experimental.h"

#include "TimeTrackVRulerControls.h"
#include "../../../AColor.h"
#include "../../../AllThemeResources.h"
#include "../../../Envelope.h"
#include "../../../HitTestResult.h"
#include "../../../TrackArtist.h"
#include "../../../TrackPanelDrawingContext.h"
#include "../../../TrackPanelMouseEvent.h"
#include "../../../Project.h"
#include "../../../ViewInfo.h"
#include "../../../widgets/Ruler.h"

#include "../../ui/EnvelopeHandle.h"

TimeTrackView::TimeTrackView( const std::shared_ptr<Track> &pTrack )
   : TrackView{ pTrack }
{
   mHeight = 100;
}

TimeTrackView::~TimeTrackView()
{
}

std::vector<UIHandlePtr> TimeTrackView::DetailedHitTest
(const TrackPanelMouseState &st,
 const AudacityProject *pProject, int, bool)
{
   std::vector<UIHandlePtr> results;
   auto result = EnvelopeHandle::TimeTrackHitTest
      ( mEnvelopeHandle, st.state, st.rect, pProject,
        std::static_pointer_cast< TimeTrack >( FindTrack() ) );
   if (result)
      results.push_back(result);
   return results;
}

std::shared_ptr<TrackVRulerControls> TimeTrackView::DoGetVRulerControls()
{
   return
      std::make_shared<TimeTrackVRulerControls>( shared_from_this() );
}

namespace {
void DrawHorzRulerAndCurve
( TrackPanelDrawingContext &context, const wxRect & r,
  const TimeTrack &track, Ruler &ruler )
{
   auto &dc = context.dc;
   const auto artist = TrackArtist::Get( context );
   const auto &zoomInfo = *artist->pZoomInfo;
   
   bool highlight = false;
#ifdef EXPERIMENTAL_TRACK_PANEL_HIGHLIGHTING
   auto target = dynamic_cast<EnvelopeHandle*>(context.target.get());
   highlight = target && target->GetEnvelope() == this->GetEnvelope();
#endif
   
   double min = zoomInfo.PositionToTime(0);
   double max = zoomInfo.PositionToTime(r.width);
   if (min > max)
   {
      wxASSERT(false);
      min = max;
   }
   
   AColor::UseThemeColour( &dc, clrUnselected );
   dc.DrawRectangle(r);
   
   //copy this rectangle away for future use.
   wxRect mid = r;
   
   // Draw the Ruler
   ruler.SetBounds(r.x, r.y, r.x + r.width - 1, r.y + r.height - 1);
   ruler.SetRange(min, max);
   ruler.SetFlip(false);  // If we don't do this, the Ruler doesn't redraw itself when the envelope is modified.
   // I have no idea why!
   //
   // LL:  It's because the ruler only Invalidate()s when the NEW value is different
   //      than the current value.
   ruler.SetFlip(TrackView::Get( track ).GetHeight() > 75 ? true : true); // MB: so why don't we just call Invalidate()? :)
   ruler.SetTickColour( theTheme.Colour( clrTrackPanelText ));
   ruler.Draw(dc, &track);
   
   Doubles envValues{ size_t(mid.width) };
   track.GetEnvelope()->GetValues
   ( 0, 0, envValues.get(), mid.width, 0, zoomInfo );
   
   wxPen &pen = highlight ? AColor::uglyPen : AColor::envelopePen;
   dc.SetPen( pen );
   
   auto rangeLower = track.GetRangeLower(), rangeUpper = track.GetRangeUpper();
   double logLower = log(std::max(1.0e-7, rangeLower)),
      logUpper = log(std::max(1.0e-7, rangeUpper));

   for (int x = 0; x < mid.width; x++)
   {
      double y;
      if ( track.GetDisplayLog() )
         y = (double)mid.height * (logUpper - log(envValues[x])) / (logUpper - logLower);
      else
         y = (double)mid.height * (rangeUpper - envValues[x]) / (rangeUpper - rangeLower);
      int thisy = r.y + (int)y;
      AColor::Line(dc, mid.x + x, thisy - 1, mid.x + x, thisy+2);
   }
}

void DrawTimeTrack(TrackPanelDrawingContext &context,
                                const TimeTrack &track, Ruler &ruler,
                                const wxRect & rect)
{
   // Ruler and curve...
   DrawHorzRulerAndCurve( context, rect, track, ruler );

   // ... then the control points
   wxRect envRect = rect;
   envRect.height -= 2;
   double lower = track.GetRangeLower(), upper = track.GetRangeUpper();
   const auto artist = TrackArtist::Get( context );
   const auto dbRange = artist->mdBrange;
   if(track.GetDisplayLog()) {
      // MB: silly way to undo the work of GetWaveYPos while still getting a logarithmic scale
      lower = LINEAR_TO_DB(std::max(1.0e-7, lower)) / dbRange + 1.0;
      upper = LINEAR_TO_DB(std::max(1.0e-7, upper)) / dbRange + 1.0;
   }
   track.GetEnvelope()->DrawPoints
      ( context, envRect,
        track.GetDisplayLog(), dbRange, lower, upper, false );
}
}

void TimeTrackView::Draw(
   TrackPanelDrawingContext &context,
   const wxRect &rect, unsigned iPass )
{
   if ( iPass == TrackArtist::PassTracks ) {
      const auto tt = std::static_pointer_cast<TimeTrack>(FindTrack());
      DrawTimeTrack( context, *tt, *tt->mRuler, rect );
   }
}
