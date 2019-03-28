/**********************************************************************

Audacity: A Digital Audio Editor

TimeTrackView.h

Paul Licameli split from class TimeTrack

**********************************************************************/

#ifndef __AUDACITY_TIME_TRACK_VIEW__
#define __AUDACITY_TIME_TRACK_VIEW__

#include "../../ui/TrackView.h"

class EnvelopeHandle;

class TimeTrackView final : public TrackView
{
   TimeTrackView( const TimeTrackView& ) = delete;
   TimeTrackView &operator=( const TimeTrackView& ) = delete;

public:
   explicit
   TimeTrackView( const std::shared_ptr<Track> &pTrack )
      : TrackView{ pTrack } {}
   ~TimeTrackView() override;

   std::shared_ptr<TrackVRulerControls> DoGetVRulerControls() override;

private:
   std::vector<UIHandlePtr> DetailedHitTest
      (const TrackPanelMouseState &state,
       const AudacityProject *pProject, int currentTool, bool bMultiTool)
      override;

   std::weak_ptr<EnvelopeHandle> mEnvelopeHandle;
};

#endif
