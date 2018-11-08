/**********************************************************************

Audacity: A Digital Audio Editor

LabelTrackView.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "LabelTrackView.h"
#include "../../../LabelTrack.h"

#include "LabelTrackControls.h"
#include "LabelDefaultClickHandle.h"
#include "LabelTrackVRulerControls.h"
#include "LabelGlyphHandle.h"
#include "LabelTextHandle.h"
#include "LabelTrackVRulerControls.h"

#include "../../ui/SelectHandle.h"

#include "../../../HitTestResult.h"
#include "../../../Project.h"
#include "../../../TrackPanelMouseEvent.h"

LabelTrackView::LabelTrackView( const std::shared_ptr<Track> &pTrack )
: TrackView{ pTrack }
{
   ResetFont();
   CreateCustomGlyphs();
   ResetFlags();

   // Events will be emitted by the TrackList, not the track itself, which will
   // make it easier to reparent this view to a different track as needed
   // without rebinding so long as the other parent resides in the same
   // TrackList
   const auto pLabelTrack = FindLabelTrack();
   const auto trackList = pLabelTrack->GetOwner();
   trackList->Bind(
      EVT_LABELTRACK_ADDITION, &LabelTrackView::OnLabelAdded, this );
   trackList->Bind(
      EVT_LABELTRACK_DELETION, &LabelTrackView::OnLabelDeleted, this );
   trackList->Bind(
      EVT_LABELTRACK_PERMUTED, &LabelTrackView::OnLabelPermuted, this );
}

LabelTrackView::~LabelTrackView()
{
   // Do this explicitly because this sink is not wxEvtHandler
   const auto pLabelTrack = FindLabelTrack();
   const auto trackList = pLabelTrack->GetOwner();
   trackList->Unbind(
      EVT_LABELTRACK_ADDITION, &LabelTrackView::OnLabelAdded, this );
   trackList->Unbind(
      EVT_LABELTRACK_DELETION, &LabelTrackView::OnLabelDeleted, this );
   trackList->Unbind(
      EVT_LABELTRACK_PERMUTED, &LabelTrackView::OnLabelPermuted, this );
}

void LabelTrackView::Copy( const TrackView &other )
{
   TrackView::Copy( other );

   if ( const auto pOther = dynamic_cast< const LabelTrackView* >( &other ) ) {
      // only one field is important to preserve in undo/redo history
      mSelIndex = pOther->mSelIndex;
   }
}

LabelTrackView &LabelTrackView::Get( LabelTrack &track )
{
   return static_cast< LabelTrackView& >( TrackView::Get( track ) );
}

const LabelTrackView &LabelTrackView::Get( const LabelTrack &track )
{
   return static_cast< const LabelTrackView& >( TrackView::Get( track ) );
}

std::vector<UIHandlePtr> LabelTrack::DetailedHitTest
(const TrackPanelMouseState &st,
 const AudacityProject *pProject, int currentTool, bool bMultiTool)
{
   return LabelTrackView::Get( *this )
      .DetailedHitTest( st, pProject, currentTool, bMultiTool );
}

std::vector<UIHandlePtr> LabelTrackView::DetailedHitTest
(const TrackPanelMouseState &st,
 const AudacityProject *WXUNUSED(pProject), int, bool)
{
   UIHandlePtr result;
   std::vector<UIHandlePtr> results;
   const wxMouseState &state = st.state;

   const auto pTrack = FindLabelTrack();
   result = LabelGlyphHandle::HitTest(
      mGlyphHandle, state, pTrack, st.rect);
   if (result)
      results.push_back(result);

   result = LabelTextHandle::HitTest(
      mTextHandle, state, pTrack );
   if (result)
      results.push_back(result);

   return results;
}

std::shared_ptr<TrackView> LabelTrack::DoGetView()
{
   return std::make_shared<LabelTrackView>( SharedPointer() );
}

std::shared_ptr<TrackControls> LabelTrack::DoGetControls()
{
   return std::make_shared<LabelTrackControls>( SharedPointer() );
}

std::shared_ptr<TrackVRulerControls> LabelTrackView::DoGetVRulerControls()
{
   return
      std::make_shared<LabelTrackVRulerControls>( shared_from_this() );
}
