/**********************************************************************

Audacity: A Digital Audio Editor

TrackControls.h

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#ifndef __AUDACITY_TRACK_CONTROLS__
#define __AUDACITY_TRACK_CONTROLS__

#include "CommonTrackPanelCell.h"
#include "../../MemoryX.h"

class PopupMenuTable;
class Track;

class CloseButtonHandle;
class MenuButtonHandle;
class MinimizeButtonHandle;
class SelectButtonHandle;
struct TrackPanelDrawingContext;
class TrackSelectHandle;

class TrackControls /* not final */ : public CommonTrackPanelCell
   , public std::enable_shared_from_this< TrackControls >
{
public:
   explicit
   TrackControls( std::shared_ptr<Track> pTrack );

   virtual ~TrackControls() = 0;

   static TrackControls &Get( Track &track );
   static const TrackControls &Get( const Track &track );

   // This is passed to the InitMenu() methods of the PopupMenuTable
   // objects returned by GetMenuExtension:
   struct InitMenuData
   {
   public:
      Track *pTrack;
      wxWindow *pParent;
      unsigned result;
   };

   void Reparent( Track &parent );

   struct TCPLine {
      using DrawFunction = void (*)(
         TrackPanelDrawingContext &context,
         const wxRect &rect,
         const Track *maybeNULL
      );

      unsigned items; // a bitwise OR of values of the enum above
      int height;
      int extraSpace;
      DrawFunction drawFunction;
   };
   using TCPLines = std::vector< TCPLine >;

protected:
   std::shared_ptr<Track> DoFindTrack() override;

   // An override is supplied for derived classes to call through but it is
   // still marked pure virtual
   virtual std::vector<UIHandlePtr> HitTest
      (const TrackPanelMouseState &state,
       const AudacityProject *) override = 0;

   unsigned DoContextMenu
      (const wxRect &rect, wxWindow *pParent, wxPoint *pPosition) override;
   virtual PopupMenuTable *GetMenuExtension(Track *pTrack) = 0;

   Track *GetTrack() const;

   std::weak_ptr<Track> mwTrack;

   std::weak_ptr<CloseButtonHandle> mCloseHandle;
   std::weak_ptr<MenuButtonHandle> mMenuHandle;
   std::weak_ptr<MinimizeButtonHandle> mMinimizeHandle;
   std::weak_ptr<SelectButtonHandle> mSelectButtonHandle;
   std::weak_ptr<TrackSelectHandle> mSelectHandle;
};

#endif
