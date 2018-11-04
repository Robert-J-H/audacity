/**********************************************************************

Audacity: A Digital Audio Editor

WaveTrackVZoomHandle.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../../../Audacity.h"
#include "WaveTrackVZoomHandle.h"

#include "../../../../Experimental.h"

#include "WaveTrackViewGroupData.h"
#include "WaveTrackVRulerControls.h"

#include "../../../../HitTestResult.h"
#include "../../../../prefs/SpectrogramSettings.h"
#include "../../../../prefs/WaveformSettings.h"
#include "../../../../Project.h"
#include "../../../../RefreshCode.h"
#include "../../../../TrackArtist.h"
#include "../../../../TrackPanelMouseEvent.h"
#include "../../../../WaveTrack.h"
#include "../../../../widgets/PopupMenuTable.h"
#include "../../../../../images/Cursors.h"
#include "../../../../Prefs.h"

namespace
{

struct InitMenuData
{
public:
   WaveTrack *pTrack;
   wxRect rect;
   unsigned result;
   int yy;
};

}

WaveTrackVZoomHandle::WaveTrackVZoomHandle
(const std::shared_ptr<WaveTrack> &pTrack, const wxRect &rect, int y)
   : mpTrack{ pTrack } , mZoomStart(y), mZoomEnd(y), mRect(rect)
{
}

void WaveTrackVZoomHandle::Enter(bool)
{
#ifdef EXPERIMENTAL_TRACK_PANEL_HIGHLIGHTING
   mChangeHighlight = RefreshCode::RefreshCell;
#endif
}

enum {
   OnZoomFitVerticalID = 20000,
   OnZoomResetID,
   OnZoomDiv2ID,
   OnZoomTimes2ID,
   OnZoomHalfWaveID,
   OnZoomInVerticalID,
   OnZoomOutVerticalID,

   // Reserve an ample block of ids for waveform scale types
   OnFirstWaveformScaleID,
   OnLastWaveformScaleID = OnFirstWaveformScaleID + 9,

   // Reserve an ample block of ids for spectrum scale types
   OnFirstSpectrumScaleID,
   OnLastSpectrumScaleID = OnFirstSpectrumScaleID + 19,
};

///////////////////////////////////////////////////////////////////////////////
// Table class

class WaveTrackVRulerMenuTable : public PopupMenuTable
{
protected:
   WaveTrackVRulerMenuTable() {}

   void InitMenu(Menu *pMenu, void *pUserData) override;

private:
   void DestroyMenu() override
   {
      mpData = nullptr;
   }

protected:
   InitMenuData *mpData {};

   void OnZoom( WaveTrackViewConstants::ZoomActions iZoomCode );
   void OnZoomFitVertical(wxCommandEvent&)
      { OnZoom( WaveTrackViewConstants::kZoom1to1 );};
   void OnZoomReset(wxCommandEvent&)
      { OnZoom( WaveTrackViewConstants::kZoomReset );};
   void OnZoomDiv2Vertical(wxCommandEvent&)
      { OnZoom( WaveTrackViewConstants::kZoomDiv2 );};
   void OnZoomTimes2Vertical(wxCommandEvent&)
      { OnZoom( WaveTrackViewConstants::kZoomTimes2 );};
   void OnZoomHalfWave(wxCommandEvent&)
      { OnZoom( WaveTrackViewConstants::kZoomHalfWave );};
   void OnZoomInVertical(wxCommandEvent&)
      { OnZoom( WaveTrackViewConstants::kZoomIn );};
   void OnZoomOutVertical(wxCommandEvent&)
      { OnZoom( WaveTrackViewConstants::kZoomOut );};
};

void WaveTrackVRulerMenuTable::InitMenu(Menu *, void *pUserData)
{
   mpData = static_cast<InitMenuData*>(pUserData);
}


void WaveTrackVRulerMenuTable::OnZoom(
   WaveTrackViewConstants::ZoomActions iZoomCode )
{
   WaveTrackViewGroupData::Get( *mpData->pTrack ).DoZoom
      (mpData->pTrack->GetRate(),
       iZoomCode, mpData->rect, mpData->yy, mpData->yy, false);
   auto pProject = ::GetActiveProject();
   if( pProject )
      pProject->ModifyState(true);

   using namespace RefreshCode;
   mpData->result = UpdateVRuler | RefreshAll;
}

///////////////////////////////////////////////////////////////////////////////
// Table class

class WaveformVRulerMenuTable : public WaveTrackVRulerMenuTable
{
   WaveformVRulerMenuTable() : WaveTrackVRulerMenuTable() {}
   virtual ~WaveformVRulerMenuTable() {}
   DECLARE_POPUP_MENU(WaveformVRulerMenuTable);

public:
   static WaveformVRulerMenuTable &Instance();

private:
   virtual void InitMenu(Menu *pMenu, void *pUserData) override;

   void OnWaveformScaleType(wxCommandEvent &evt);
};

WaveformVRulerMenuTable &WaveformVRulerMenuTable::Instance()
{
   static WaveformVRulerMenuTable instance;
   return instance;
}

void WaveformVRulerMenuTable::InitMenu(Menu *pMenu, void *pUserData)
{
   WaveTrackVRulerMenuTable::InitMenu(pMenu, pUserData);

// DB setting is already on track drop down.
#if 0 
   WaveTrack *const wt = mpData->pTrack;
   const int id =
      OnFirstWaveformScaleID + (int)(wt->GetWaveformSettings().scaleType);
   pMenu->Check(id, true);
#endif
}

BEGIN_POPUP_MENU(WaveformVRulerMenuTable)

   POPUP_MENU_ITEM(OnZoomFitVerticalID, _("Zoom Reset\tShift-Right-Click"), OnZoomReset)
   POPUP_MENU_ITEM(OnZoomDiv2ID,        _("Zoom x1/2"),                     OnZoomDiv2Vertical)
   POPUP_MENU_ITEM(OnZoomTimes2ID,      _("Zoom x2"),                       OnZoomTimes2Vertical)

#ifdef EXPERIMENTAL_HALF_WAVE
   POPUP_MENU_ITEM(OnZoomHalfWaveID,    _("Half Wave"),                     OnZoomHalfWave)
#endif

   POPUP_MENU_SEPARATOR()
   POPUP_MENU_ITEM(OnZoomInVerticalID,  _("Zoom In\tLeft-Click/Left-Drag"),  OnZoomInVertical)
   POPUP_MENU_ITEM(OnZoomOutVerticalID, _("Zoom Out\tShift-Left-Click"),     OnZoomOutVertical)
// The log and linear options are already available as waveform db.
// So don't repeat them here.
#if 0
   POPUP_MENU_SEPARATOR()
   {
      const auto & names = WaveformSettings::GetScaleNames();
      for (int ii = 0, nn = names.size(); ii < nn; ++ii) {
         POPUP_MENU_RADIO_ITEM(OnFirstWaveformScaleID + ii, names[ii],
            OnWaveformScaleType);
      }
   }
#endif
END_POPUP_MENU()

void WaveformVRulerMenuTable::OnWaveformScaleType(wxCommandEvent &evt)
{
   WaveTrack *const wt = mpData->pTrack;
   // Assume linked track is wave or null
   const WaveformSettings::ScaleType newScaleType =
      WaveformSettings::ScaleType(
         std::max(0,
            std::min((int)(WaveformSettings::stNumScaleTypes) - 1,
               evt.GetId() - OnFirstWaveformScaleID
      )));

   auto &data = WaveTrackViewGroupData::Get( *wt );
   if (data.GetWaveformSettings().scaleType != newScaleType) {
      data.GetIndependentWaveformSettings().scaleType = newScaleType;

      ::GetActiveProject()->ModifyState(true);

      using namespace RefreshCode;
      mpData->result = UpdateVRuler | RefreshAll;
   }
}

///////////////////////////////////////////////////////////////////////////////
// Table class

class SpectrumVRulerMenuTable : public WaveTrackVRulerMenuTable
{
   SpectrumVRulerMenuTable() : WaveTrackVRulerMenuTable() {}
   virtual ~SpectrumVRulerMenuTable() {}
   DECLARE_POPUP_MENU(SpectrumVRulerMenuTable);

public:
   static SpectrumVRulerMenuTable &Instance();

private:
   void InitMenu(Menu *pMenu, void *pUserData) override;

   void OnSpectrumScaleType(wxCommandEvent &evt);
};

SpectrumVRulerMenuTable &SpectrumVRulerMenuTable::Instance()
{
   static SpectrumVRulerMenuTable instance;
   return instance;
}

void SpectrumVRulerMenuTable::InitMenu(Menu *pMenu, void *pUserData)
{
   WaveTrackVRulerMenuTable::InitMenu(pMenu, pUserData);

   WaveTrack *const wt = mpData->pTrack;
   auto &data = WaveTrackViewGroupData::Get( *wt );
   const int id =
      OnFirstSpectrumScaleID + (int)(data.GetSpectrogramSettings().scaleType);
   pMenu->Check(id, true);
}

BEGIN_POPUP_MENU(SpectrumVRulerMenuTable)

   {
      const auto & names = SpectrogramSettings::GetScaleNames();
      for (int ii = 0, nn = names.size(); ii < nn; ++ii) {
         POPUP_MENU_RADIO_ITEM(OnFirstSpectrumScaleID + ii, names[ii],
            OnSpectrumScaleType);
      }
   }

POPUP_MENU_SEPARATOR()
   POPUP_MENU_ITEM(OnZoomResetID,         _("Zoom Reset"),                     OnZoomReset)
   POPUP_MENU_ITEM(OnZoomFitVerticalID,   _("Zoom to Fit\tShift-Right-Click"), OnZoomFitVertical)
   POPUP_MENU_ITEM(OnZoomInVerticalID,    _("Zoom In\tLeft-Click/Left-Drag"),  OnZoomInVertical)
   POPUP_MENU_ITEM(OnZoomOutVerticalID,   _("Zoom Out\tShift-Left-Click"),     OnZoomOutVertical)
END_POPUP_MENU()

void SpectrumVRulerMenuTable::OnSpectrumScaleType(wxCommandEvent &evt)
{
   WaveTrack *const wt = mpData->pTrack;
   auto &data = WaveTrackViewGroupData::Get( *wt );

   const SpectrogramSettings::ScaleType newScaleType =
      SpectrogramSettings::ScaleType(
         std::max(0,
            std::min((int)(SpectrogramSettings::stNumScaleTypes) - 1,
               evt.GetId() - OnFirstSpectrumScaleID
      )));
   if (data.GetSpectrogramSettings().scaleType != newScaleType) {
      data.GetIndependentSpectrogramSettings().scaleType = newScaleType;

      ::GetActiveProject()->ModifyState(true);

      using namespace RefreshCode;
      mpData->result = UpdateVRuler | RefreshAll;
   }
}

///////////////////////////////////////////////////////////////////////////////

HitTestPreview WaveTrackVZoomHandle::HitPreview(const wxMouseState &state)
{
   static auto zoomInCursor =
      ::MakeCursor(wxCURSOR_MAGNIFIER, ZoomInCursorXpm, 19, 15);
   static auto zoomOutCursor =
      ::MakeCursor(wxCURSOR_MAGNIFIER, ZoomOutCursorXpm, 19, 15);
   static  wxCursor arrowCursor{ wxCURSOR_ARROW };
   bool bVZoom;
   gPrefs->Read(wxT("/GUI/VerticalZooming"), &bVZoom, false);
   bVZoom &= !state.RightIsDown();
   const auto message = bVZoom ? 
      _("Click to vertically zoom in. Shift-click to zoom out. Drag to specify a zoom region.") :
      _("Right-click for menu.");

   return {
      message,
      bVZoom ? (state.ShiftDown() ? &*zoomOutCursor : &*zoomInCursor) : &arrowCursor
      // , message
   };
}

WaveTrackVZoomHandle::~WaveTrackVZoomHandle()
{
}

UIHandle::Result WaveTrackVZoomHandle::Click
(const TrackPanelMouseEvent &, AudacityProject *)
{
   return RefreshCode::RefreshNone;
}

UIHandle::Result WaveTrackVZoomHandle::Drag
(const TrackPanelMouseEvent &evt, AudacityProject *pProject)
{
   using namespace RefreshCode;
   auto pTrack = TrackList::Get( *pProject ).Lock(mpTrack);
   if (!pTrack)
      return Cancelled;

   const wxMouseEvent &event = evt.event;
   if ( event.RightIsDown() )
      return RefreshNone;
   mZoomEnd = event.m_y;
   if (WaveTrackViewGroupData::IsDragZooming(mZoomStart, mZoomEnd))
      return RefreshAll;
   return RefreshNone;
}

HitTestPreview WaveTrackVZoomHandle::Preview
(const TrackPanelMouseState &st, const AudacityProject *)
{
   return HitPreview(st.state);
}

UIHandle::Result WaveTrackVZoomHandle::Release
(const TrackPanelMouseEvent &evt, AudacityProject *pProject,
 wxWindow *pParent)
{
   using namespace RefreshCode;
   auto pTrack = TrackList::Get( *pProject ).Lock(mpTrack);
   if (!pTrack)
      return RefreshNone;

   const wxMouseEvent &event = evt.event;
   const bool shiftDown = event.ShiftDown();
   const bool rightUp = event.RightUp();


   bool bVZoom;
   gPrefs->Read(wxT("/GUI/VerticalZooming"), &bVZoom, false);

   // Popup menu... 
   if (
       rightUp &&
       !(event.ShiftDown() || event.CmdDown()))
   {
      InitMenuData data {
         pTrack.get(), mRect, RefreshCode::RefreshNone, event.m_y
      };

      auto &groupData = WaveTrackViewGroupData::Get( *pTrack );
      PopupMenuTable *const pTable =
         (groupData.GetDisplay() == WaveTrackViewConstants::Spectrum)
         ? (PopupMenuTable *) &SpectrumVRulerMenuTable::Instance()
         : (PopupMenuTable *) &WaveformVRulerMenuTable::Instance();
      std::unique_ptr<PopupMenuTable::Menu>
         pMenu(PopupMenuTable::BuildMenu(pParent, pTable, &data));

      // Accelerators only if zooming enabled.
      if( !bVZoom )
      {
         wxMenuItemList & L = pMenu->GetMenuItems();
         // let's iterate over the list in STL syntax
         wxMenuItemList::iterator iter;
         for (iter = L.begin(); iter != L.end(); ++iter)
         {
             wxMenuItem *pItem = *iter;
             // Remove accelerator, if any.
             pItem->SetItemLabel( (pItem->GetItemLabel() + "\t" ).BeforeFirst('\t') );
         }
      }


      pParent->PopupMenu(pMenu.get(), event.m_x, event.m_y);

      return data.result;
   }
   else{
      // Ignore Capture Lost event 
      bVZoom &= event.GetId() != kCaptureLostEventId;
      // shiftDown | rightUp | ZoomKind
      //    T      |    T    | 1to1
      //    T      |    F    | Out
      //    F      |    -    | In
      if( bVZoom ){
         using namespace WaveTrackViewConstants;
         if( shiftDown )
            mZoomStart=mZoomEnd;
         WaveTrackViewGroupData::Get( *pTrack ).DoZoom(
            pTrack->GetRate(),
            shiftDown
               ? (rightUp ? kZoom1to1 : kZoomOut)
               : kZoomIn,
            mRect, mZoomStart, mZoomEnd, !shiftDown);
         if( pProject )
            pProject->ModifyState(true);
      }
   }

   return UpdateVRuler | RefreshAll;
}

UIHandle::Result WaveTrackVZoomHandle::Cancel(AudacityProject*)
{
   // Cancel is implemented!  And there is no initial state to restore,
   // so just return a code.
   return RefreshCode::RefreshAll;
}

void WaveTrackVZoomHandle::Draw(
   TrackPanelDrawingContext &context,
   const wxRect &rect, unsigned iPass )
{
   if ( iPass == TrackArtist::PassZooming ) {
      if (!mpTrack.lock()) //? TrackList::Lock()
         return;
      
      if ( WaveTrackViewGroupData::IsDragZooming( mZoomStart, mZoomEnd ) )
         TrackVRulerControls::DrawZooming
            ( context, rect, mZoomStart, mZoomEnd );
   }
}

wxRect WaveTrackVZoomHandle::DrawingArea(
   const wxRect &rect, const wxRect &panelRect, unsigned iPass )
{
   if ( iPass == TrackArtist::PassZooming )
      return TrackVRulerControls::ZoomingArea( rect, panelRect );
   else
      return rect;
}
