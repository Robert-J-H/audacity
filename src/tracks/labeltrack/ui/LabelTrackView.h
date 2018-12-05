/**********************************************************************

Audacity: A Digital Audio Editor

LabelTrackView.h

Paul Licameli split from class LabelTrack

**********************************************************************/

#ifndef __AUDACITY_LABEL_TRACK_VIEW__
#define __AUDACITY_LABEL_TRACK_VIEW__

#include "../../ui/TrackView.h"

class LabelTrack;
class SelectedRegion;
struct TrackListEvent;

class LabelTrackView final : public TrackView
{
   LabelTrackView( const LabelTrackView& ) = delete;
   LabelTrackView &operator=( const LabelTrackView& ) = delete;

public:
   explicit
   LabelTrackView( const std::shared_ptr<Track> &pTrack )
      : TrackView{ pTrack } {}
   ~LabelTrackView() override;

   static LabelTrackView &Get( LabelTrack& );
   static const LabelTrackView &Get( const LabelTrack& );

   //This returns the index of the label we just added.
   int AddLabel(const SelectedRegion &region,
      const wxString &title = {},
      int restoreFocus = -1);

private:
   std::shared_ptr<TrackVRulerControls> DoGetVRulerControls() override;

   void OnSelectionChange( TrackListEvent& );

   std::shared_ptr<LabelTrack> FindLabelTrack();
   std::shared_ptr<const LabelTrack> FindLabelTrack() const;
};

#endif
