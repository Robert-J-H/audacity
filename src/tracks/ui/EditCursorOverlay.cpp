/**********************************************************************

Audacity: A Digital Audio Editor

EditCursorOverlay.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../Audacity.h"
#include "EditCursorOverlay.h"

#include "../../AColor.h"
#include "../../AdornedRulerPanel.h"
#include "../../Project.h"
#include "../../Track.h"
#include "../../TrackPanelAx.h"
#include "../../TrackPanel.h"
#include "../../ViewInfo.h"

#include <wx/dc.h>

namespace {
   template < class LOW, class MID, class HIGH >
   bool between_incexc(LOW l, MID m, HIGH h)
   {
      return (m >= l && m < h);
   }
}

EditCursorOverlay::EditCursorOverlay(AudacityProject *project, bool isMaster)
   : mProject(project)
   , mIsMaster(isMaster)
   , mLastCursorX(-1)
   , mCursorTime(-1)
   , mNewCursorX(-1)
{
}

std::pair<wxRect, bool> EditCursorOverlay::DoGetRectangle(wxSize size)
{
   const auto &selection = ViewInfo::Get( *mProject ).selectedRegion;
   if (!selection.isPoint()) {
      mCursorTime = -1.0;
      mNewCursorX = -1;
   }
   else {
      mCursorTime = selection.t0();
      mNewCursorX = ZoomInfo::Get( *mProject ).TimeToPosition
         (mCursorTime, TrackPanel::Get( *mProject ).GetLeftOffset());
   }

   // Excessive height in case of the ruler, but it matters little.
   return std::make_pair(
      mLastCursorX == -1
         ? wxRect()
         : wxRect(mLastCursorX, 0, 1, size.GetHeight()),
      mLastCursorX != mNewCursorX
   );
}


void EditCursorOverlay::Draw(OverlayPanel &panel, wxDC &dc)
{
   if (mIsMaster && !mPartner) {
      auto &ruler = AdornedRulerPanel::Get( *mProject );
      mPartner = std::make_shared<EditCursorOverlay>(mProject, false);
      ruler.AddOverlay( mPartner );
   }

   mLastCursorX = mNewCursorX;
   if (mLastCursorX == -1)
      return;

   const auto &viewInfo = ZoomInfo::Get( *mProject );

   auto &trackPanel = TrackPanel::Get( *mProject );
   const bool
   onScreen = between_incexc(viewInfo.h,
                             mCursorTime,
                             trackPanel.GetScreenEndTime());

   if (!onScreen)
      return;

   if (auto tp = dynamic_cast<TrackPanel*>(&panel)) {
      wxASSERT(mIsMaster);
      AColor::CursorColor(&dc);

      // Draw cursor in all selected tracks
      tp->VisitCells( [&]( const wxRect &rect, TrackPanelCell &cell ) {
         const auto pTrack = dynamic_cast<Track*>(&cell);
         if (!pTrack)
            return;
         if (pTrack->GetSelected() ||
             trackPanel.GetAx().IsFocused(pTrack))
         {
            // AColor::Line includes both endpoints so use GetBottom()
            AColor::Line(dc, mLastCursorX, rect.GetTop(), mLastCursorX, rect.GetBottom());
            // ^^^ The whole point of this routine.

         }
      } );
   }
   else if (auto ruler = dynamic_cast<AdornedRulerPanel*>(&panel)) {
      wxASSERT(!mIsMaster);
      dc.SetPen(*wxBLACK_PEN);
      // AColor::Line includes both endpoints so use GetBottom()
      auto rect = ruler->GetInnerRect();
      AColor::Line(dc, mLastCursorX, rect.GetTop(), mLastCursorX, rect.GetBottom());
   }
   else
      wxASSERT(false);
}
