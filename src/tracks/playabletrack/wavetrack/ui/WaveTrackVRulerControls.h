/**********************************************************************

Audacity: A Digital Audio Editor

WaveTrackVRulerControls.h

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#ifndef __AUDACITY_WAVE_TRACK_VRULER_CONTROLS__
#define __AUDACITY_WAVE_TRACK_VRULER_CONTROLS__

#include "../../../ui/TrackVRulerControls.h"

class WaveTrackVZoomHandle;

class WaveTrackVRulerControls final : public TrackVRulerControls
{
   WaveTrackVRulerControls(const WaveTrackVRulerControls&) = delete;
   WaveTrackVRulerControls &operator=(const WaveTrackVRulerControls&) = delete;

public:
   explicit
   WaveTrackVRulerControls( std::shared_ptr<TrackView> pTrackView )
      : TrackVRulerControls( pTrackView ) {}
   ~WaveTrackVRulerControls();

   std::vector<UIHandlePtr> HitTest
      (const TrackPanelMouseState &state,
       const AudacityProject *) override;

   unsigned HandleWheelRotation
      (const TrackPanelMouseEvent &event,
       AudacityProject *pProject) override;

private:
   // TrackPanelDrawable implementation
   void Draw(
      TrackPanelDrawingContext &context,
      const wxRect &rect, unsigned iPass ) override;

   // TrackVRulerControls implementation
   void UpdateRuler( const wxRect &rect ) override;

   std::weak_ptr<WaveTrackVZoomHandle> mVZoomHandle;
};

#endif
