/**********************************************************************

Audacity: A Digital Audio Editor

WaveTrackView.h

Paul Licameli split from class WaveTrack

**********************************************************************/

#ifndef __AUDACITY_WAVE_TRACK_VIEW__
#define __AUDACITY_WAVE_TRACK_VIEW__

#include "../../../ui/TrackView.h"

class CutlineHandle;
class SampleHandle;
class EnvelopeHandle;
class WaveTrack;

class WaveTrackView final : public TrackView
{
   WaveTrackView( const WaveTrackView& ) = delete;
   WaveTrackView &operator=( const WaveTrackView& ) = delete;

public:
   explicit
   WaveTrackView( const std::shared_ptr<Track> &pTrack );
   ~WaveTrackView() override;

   std::shared_ptr<TrackVRulerControls> DoGetVRulerControls() override;

private:
   // TrackPanelDrawable implementation
   void Draw(
      TrackPanelDrawingContext &context,
      const wxRect &rect, unsigned iPass ) override;

   friend WaveTrack;

   // Preserve some view state too for undo/redo purposes
   void Copy( const TrackView &other ) override;

   std::vector<UIHandlePtr> DetailedHitTest
      (const TrackPanelMouseState &state,
       const AudacityProject *pProject, int currentTool, bool bMultiTool)
      override;

   std::weak_ptr<CutlineHandle> mCutlineHandle;
   std::weak_ptr<SampleHandle> mSampleHandle;
   std::weak_ptr<EnvelopeHandle> mEnvelopeHandle;
};

#endif
