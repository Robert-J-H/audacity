/**********************************************************************

Audacity: A Digital Audio Editor

TrackView.h

Paul Licameli split from class Track

**********************************************************************/

#ifndef __AUDACITY_TRACK_VIEW__
#define __AUDACITY_TRACK_VIEW__

#include "CommonTrackPanelCell.h"

class Track;

class TrackView /* not final */ : public CommonTrackPanelCell
{
   TrackView( const TrackView& ) = delete;
   TrackView &operator=( const TrackView& ) = delete;

public:
   explicit
   TrackView( const std::shared_ptr<Track> &pTrack ) : mwTrack{ pTrack } {}
   virtual ~TrackView() = 0;

   static TrackView &Get( Track & );
   static const TrackView &Get( const Track & );

   std::vector<UIHandlePtr> HitTest
      (const TrackPanelMouseState &, const AudacityProject *pProject)
      final override;

   // Delegates the handling to the related TCP cell
   std::shared_ptr<TrackPanelCell> ContextMenuDelegate() override;

   std::shared_ptr<Track> DoFindTrack() override;

protected:
   Track *GetTrack() const;

   // back-pointer to parent is weak to avoid a cycle
   std::weak_ptr<Track> mwTrack;
};

#endif
