/**********************************************************************

Audacity: A Digital Audio Editor

TrackView.h

Paul Licameli split from class Track

**********************************************************************/

#ifndef __AUDACITY_TRACK_VIEW__
#define __AUDACITY_TRACK_VIEW__

#include "CommonTrackPanelCell.h"

class Track;
class TrackVRulerControls;
class SelectHandle;
class TimeShiftHandle;

class TrackView /* not final */ : public CommonTrackPanelCell
   , public std::enable_shared_from_this<TrackView>
{
   TrackView( const TrackView& ) = delete;
   TrackView &operator=( const TrackView& ) = delete;

public:
   enum : unsigned { DefaultHeight = 150 };

   explicit
   TrackView( const std::shared_ptr<Track> &pTrack );
   virtual ~TrackView() = 0;

   static TrackView &Get( Track & );
   static const TrackView &Get( const Track & );

   // Copy view state, for undo/redo purposes
   virtual void Copy( const TrackView &other );

   // Return another, associated TrackPanelCell object that implements the
   // mouse actions for the vertical ruler
   std::shared_ptr<TrackVRulerControls> GetVRulerControls();
   std::shared_ptr<const TrackVRulerControls> GetVRulerControls() const;
   
   // Delegates the handling to the related TCP cell
   std::shared_ptr<TrackPanelCell> ContextMenuDelegate() override;

   // Cause certain overriding tool modes (Zoom; future ones?) to behave
   // uniformly in all tracks, disregarding track contents.
   // Do not further override this...
   std::vector<UIHandlePtr> HitTest
      (const TrackPanelMouseState &, const AudacityProject *pProject)
      final override;

protected:
   // Rather override this for subclasses:
   virtual std::vector<UIHandlePtr> DetailedHitTest
      (const TrackPanelMouseState &,
       const AudacityProject *pProject, int currentTool, bool bMultiTool)
      = 0;

   // Private factory to make appropriate object; class TrackView handles
   // memory management thereafter
   virtual std::shared_ptr<TrackVRulerControls> DoGetVRulerControls() = 0;

public:
   void Reparent( Track &parent );

protected:
   std::shared_ptr<Track> DoFindTrack() override;

public:
   int GetY() const { return mY; }
   int GetActualHeight() const { return mHeight; }
   int GetMinimizedHeight() const;
   int GetHeight() const;

   void SetY(int y) { DoSetY( y ); }
   void SetHeight(int height);

protected:
   // No need yet to make this virtual
   void DoSetY(int y);

   virtual void DoSetHeight(int h);

   std::shared_ptr<TrackVRulerControls> mpVRulerControls;

   // back-pointer to parent is weak to avoid a cycle
   std::weak_ptr<Track> mwTrack;

   std::weak_ptr<SelectHandle> mSelectHandle;
   std::weak_ptr<TimeShiftHandle> mTimeShiftHandle;

   int            mY{ 0 };
   int            mHeight{ DefaultHeight };
};

#endif
