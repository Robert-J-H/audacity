/**********************************************************************

Audacity: A Digital Audio Editor

TimeTrackView.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "TimeTrackView.h"
#include "../../../TimeTrack.h"

#include "TimeTrackVRulerControls.h"
#include "../../../HitTestResult.h"
#include "../../../TrackPanelMouseEvent.h"
#include "../../../Project.h"

#include "../../ui/EnvelopeHandle.h"

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
