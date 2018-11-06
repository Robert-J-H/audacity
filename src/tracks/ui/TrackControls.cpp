/**********************************************************************

Audacity: A Digital Audio Editor

TrackControls.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../Audacity.h"
#include "TrackControls.h"

#include "TrackView.h"
#include "TrackButtonHandles.h"
#include "TrackSelectHandle.h"
#include "TrackViewGroupData.h"
#include "../../AColor.h"
#include "../../AllThemeResources.h"
#include "../../HitTestResult.h"
#include "../../RefreshCode.h"
#include "../../Menus.h"
#include "../../Project.h"
#include "../../TrackArtist.h"
#include "../../TrackPanel.h" // for TrackInfo
#include "../../TrackPanelDrawingContext.h"
#include "../../TrackPanelMouseEvent.h"
#include "../../Track.h"
#include <wx/textdlg.h>
#include "../../commands/CommandType.h"
#include "../../commands/Command.h"
#include "../../commands/CommandManager.h"
#include "../../ShuttleGui.h"


TrackControls::TrackControls( std::shared_ptr<Track> pTrack )
   : mwTrack{ pTrack }
{
}

TrackControls::~TrackControls()
{
}

TrackControls &TrackControls::Get( Track &track )
{
   return *track.GetTrackControls();
}

const TrackControls &TrackControls::Get( const Track &track )
{
   return *track.GetTrackControls();
}

std::shared_ptr<Track> TrackControls::DoFindTrack()
{
   return mwTrack.lock();
}

void TrackControls::Reparent( Track &parent )
{
   mwTrack = parent.shared_from_this();
}

std::vector<UIHandlePtr> TrackControls::HitTest
(const TrackPanelMouseState &st,
 const AudacityProject *WXUNUSED(project))
{
   // Hits are mutually exclusive, results single

   const wxMouseState &state = st.state;
   const wxRect &rect = st.rect;
   UIHandlePtr result;
   std::vector<UIHandlePtr> results;

   auto sThis = shared_from_this();

   if (NULL != (result = CloseButtonHandle::HitTest(
      mCloseHandle, state, rect, this)))
      results.push_back(result);

   if (NULL != (result = MenuButtonHandle::HitTest(
      mMenuHandle, state, rect, sThis)))
      results.push_back(result);

   if (NULL != (result = MinimizeButtonHandle::HitTest(
      mMinimizeHandle, state, rect, this)))
      results.push_back(result);

   if (NULL != (result = SelectButtonHandle::HitTest(
      mSelectButtonHandle, state, rect, this)))
      results.push_back(result);

   if (results.empty()) {
      if (NULL != (result = TrackSelectHandle::HitAnywhere(
         mSelectHandle, FindTrack())))
         results.push_back(result);
   }

   return results;
}

enum
{
   OnSetNameID = 2000,
   OnMoveUpID,
   OnMoveDownID,
   OnMoveTopID,
   OnMoveBottomID,
};

class TrackMenuTable : public PopupMenuTable
{
   TrackMenuTable() : mpData(NULL) {}
   DECLARE_POPUP_MENU(TrackMenuTable);

public:
   static TrackMenuTable &Instance();

private:
   void OnSetName(wxCommandEvent &);
   void OnMoveTrack(wxCommandEvent &event);

   void InitMenu(Menu *pMenu, void *pUserData) override;

   void DestroyMenu() override
   {
      mpData = nullptr;
   }

   TrackControls::InitMenuData *mpData;
};

TrackMenuTable &TrackMenuTable::Instance()
{
   static TrackMenuTable instance;
   return instance;
}

void TrackMenuTable::InitMenu(Menu *pMenu, void *pUserData)
{
   mpData = static_cast<TrackControls::InitMenuData*>(pUserData);
   Track *const pTrack = mpData->pTrack;

   const auto &tracks = TrackList::Get( *GetActiveProject() );

   pMenu->Enable(OnMoveUpID, tracks.CanMoveUp(pTrack));
   pMenu->Enable(OnMoveDownID, tracks.CanMoveDown(pTrack));
   pMenu->Enable(OnMoveTopID, tracks.CanMoveUp(pTrack));
   pMenu->Enable(OnMoveBottomID, tracks.CanMoveDown(pTrack));
}

BEGIN_POPUP_MENU(TrackMenuTable)
   POPUP_MENU_ITEM(OnSetNameID, _("&Name..."), OnSetName)
   POPUP_MENU_SEPARATOR()
   POPUP_MENU_ITEM(
      // It is not correct to use NormalizedKeyString::Display here --
      // wxWidgets will apply its equivalent to the key names passed to menu
      // functions.
      OnMoveUpID,
      _("Move Track &Up") + wxT("\t") +
         (CommandManager::Get( *GetActiveProject() ).
          GetKeyFromName(wxT("TrackMoveUp")).Raw()),
      OnMoveTrack)
   POPUP_MENU_ITEM(
      OnMoveDownID,
      _("Move Track &Down") + wxT("\t") +
         (CommandManager::Get( *GetActiveProject() ).
          GetKeyFromName(wxT("TrackMoveDown")).Raw()),
      OnMoveTrack)
   POPUP_MENU_ITEM(
      OnMoveTopID,
      _("Move Track to &Top") + wxT("\t") +
         (CommandManager::Get( *GetActiveProject() ).
         GetKeyFromName(wxT("TrackMoveTop")).Raw()),
      OnMoveTrack)
   POPUP_MENU_ITEM(
      OnMoveBottomID,
      _("Move Track to &Bottom") + wxT("\t") +
         (CommandManager::Get( *GetActiveProject() ).
          GetKeyFromName(wxT("TrackMoveBottom")).Raw()),
      OnMoveTrack)
END_POPUP_MENU()




// An example of using an AudacityCommand simply to create a dialog.
// We can add additional functions later, if we want to make it
// available to scripting.
// However there is no reason to, as SetTrackStatus is already provided.
class SetTrackNameCommand : public AudacityCommand
{
public:
   static const ComponentInterfaceSymbol Symbol;

   // ComponentInterface overrides
   ComponentInterfaceSymbol GetSymbol() override
   {return Symbol;};
   //wxString GetDescription() override {return _("Sets the track name.");};
   //bool DefineParams( ShuttleParams & S ) override;
   void PopulateOrExchange(ShuttleGui & S) override;
   //bool Apply(const CommandContext & context) override;

   // Provide an override, if we want the help button.
   // wxString ManualPage() override {return wxT("");};
public:
   wxString mName;
};

const ComponentInterfaceSymbol SetTrackNameCommand::Symbol
{ XO("Set Track Name") };

void SetTrackNameCommand::PopulateOrExchange(ShuttleGui & S)
{
   S.AddSpace(0, 5);

   S.StartMultiColumn(2, wxALIGN_CENTER);
   {
      S.TieTextBox(_("Name:"),mName);
   }
   S.EndMultiColumn();
}

void TrackMenuTable::OnSetName(wxCommandEvent &)
{
   Track *const pTrack = mpData->pTrack;
   if (pTrack)
   {
      AudacityProject *const proj = ::GetActiveProject();
      auto &data = pTrack->GetGroupData();
      const wxString oldName = data.GetName();

      SetTrackNameCommand Command;
      Command.mName = oldName;
      // Bug 1837 : We need an OK/Cancel result if we are to enter a blank string.
      bool bResult = Command.PromptUser( &ProjectWindow::Get( *proj ) );
      if (bResult) 
      {
         wxString newName = Command.mName;
         data.SetName(newName);

         proj->PushState(wxString::Format(_("Renamed '%s' to '%s'"),
            oldName,
            newName),
            _("Name Change"));

         mpData->result = RefreshCode::RefreshAll;
      }
   }
}

void TrackMenuTable::OnMoveTrack(wxCommandEvent &event)
{
   AudacityProject *const project = GetActiveProject();
   TrackActions::MoveChoice choice;
   switch (event.GetId()) {
   default:
      wxASSERT(false);
   case OnMoveUpID:
      choice = TrackActions::OnMoveUpID; break;
   case OnMoveDownID:
      choice = TrackActions::OnMoveDownID; break;
   case OnMoveTopID:
      choice = TrackActions::OnMoveTopID; break;
   case OnMoveBottomID:
      choice = TrackActions::OnMoveBottomID; break;
   }

   TrackActions::DoMoveTrack(*project, mpData->pTrack, choice);

   // MoveTrack already refreshed TrackPanel, which means repaint will happen.
   // This is a harmless redundancy:
   mpData->result = RefreshCode::RefreshAll;
}

unsigned TrackControls::DoContextMenu
   (const wxRect &rect, wxWindow *pParent, wxPoint *)
{
   wxRect buttonRect;
   TrackInfo::GetTitleBarRect(rect, buttonRect);

   auto track = FindTrack();
   if (!track)
      return RefreshCode::RefreshNone;

   InitMenuData data{ track.get(), pParent, RefreshCode::RefreshNone };

   const auto pTable = &TrackMenuTable::Instance();
   auto pMenu = PopupMenuTable::BuildMenu(pParent, pTable, &data);

   PopupMenuTable *const pExtension = GetMenuExtension(track.get());
   if (pExtension)
      pMenu->Extend(pExtension);

   pParent->PopupMenu
      (pMenu.get(), buttonRect.x + 1, buttonRect.y + buttonRect.height + 1);

   return data.result;
}

// Some old cut-and-paste legacy from TrackPanel.cpp here:
#if 0
void TrackInfo::DrawBordersWithin
   ( wxDC* dc, const wxRect & rect, const Track &track ) const
{
   AColor::Dark(dc, false); // same color as border of toolbars (ToolBar::OnPaint())

   // below close box and title bar
   wxRect buttonRect;
   GetTitleBarRect( rect, buttonRect );
   AColor::Line
      (*dc, rect.x,              buttonRect.y + buttonRect.height,
            rect.width - 1,      buttonRect.y + buttonRect.height);

   // between close box and title bar
   AColor::Line
      (*dc, buttonRect.x, buttonRect.y,
            buttonRect.x, buttonRect.y + buttonRect.height - 1);

   GetMuteSoloRect( rect, buttonRect, false, true, &track );

   bool bHasMuteSolo = dynamic_cast<const PlayableTrack*>( &track ) != NULL;
   if( bHasMuteSolo && !TrackInfo::HideTopItem( rect, buttonRect ) )
   {
      // above mute/solo
      AColor::Line
         (*dc, rect.x,          buttonRect.y,
               rect.width - 1,  buttonRect.y);

      // between mute/solo
      // Draw this little line; if there is no solo, wide mute button will
      // overpaint it later:
      AColor::Line
         (*dc, buttonRect.x + buttonRect.width, buttonRect.y,
               buttonRect.x + buttonRect.width, buttonRect.y + buttonRect.height - 1);

      // below mute/solo
      AColor::Line
         (*dc, rect.x,          buttonRect.y + buttonRect.height,
               rect.width - 1,  buttonRect.y + buttonRect.height);
   }

   // left of and above minimize button
   wxRect minimizeRect;
   this->GetMinimizeRect(rect, minimizeRect);
   AColor::Line
      (*dc, minimizeRect.x - 1, minimizeRect.y,
            minimizeRect.x - 1, minimizeRect.y + minimizeRect.height - 1);
   AColor::Line
      (*dc, minimizeRect.x,                          minimizeRect.y - 1,
            minimizeRect.x + minimizeRect.width - 1, minimizeRect.y - 1);
}
#endif

#define RANGE(array) (array), (array) + (WXSIZEOF(array))

using TCPLine = TrackControls::TCPLine;
using TCPLines = TrackControls::TCPLines;

// return y value and height
std::pair< int, int > CalcItemY( const TCPLines &lines, unsigned iItem )
{
   int y = 0;
   auto pLines = lines.begin();
   while ( pLines != lines.end() &&
           0 == (pLines->items & iItem) ) {
      y += pLines->height + pLines->extraSpace;
      ++pLines;
   }
   int height = 0;
   if ( pLines != lines.end() )
      height = pLines->height;
   return { y, height };
}

namespace {


#ifdef EXPERIMENTAL_DA

   #define TITLE_ITEMS \
      { TCPLine::kItemBarButtons, kTrackInfoBtnSize, 4, \
        &TrackInfo::CloseTitleDrawFunction },
   // DA: Has Mute and Solo on separate lines.
   #define MUTE_SOLO_ITEMS(extra) \
      { TCPLine::kItemMute, kTrackInfoBtnSize + 1, 1, \
        &TrackInfo::WideMuteDrawFunction }, \
      { TCPLine::kItemSolo, kTrackInfoBtnSize + 1, extra, \
        &TrackInfo::WideSoloDrawFunction },
   // DA: Does not have status information for a track.
   #define STATUS_ITEMS

#else

   #define TITLE_ITEMS \
      { TCPLine::kItemBarButtons, kTrackInfoBtnSize, 0, \
        &TrackInfo::CloseTitleDrawFunction },
   #define MUTE_SOLO_ITEMS(extra) \
      { TCPLine::kItemMute | TCPLine::kItemSolo, kTrackInfoBtnSize + 1, extra, \
        &TrackInfo::MuteAndSoloDrawFunction },
   #define STATUS_ITEMS \
      { TCPLine::kItemStatusInfo1, 12, 0, \
        &TrackInfo::Status1DrawFunction }, \
      { TCPLine::kItemStatusInfo2, 12, 0, \
        &TrackInfo::Status2DrawFunction },

#endif

#define COMMON_ITEMS \
   TITLE_ITEMS

const TCPLine defaultCommonTrackTCPLines[] = {
   COMMON_ITEMS
};
TCPLines commonTrackTCPLines{ RANGE(defaultCommonTrackTCPLines) };

const TCPLine defaultWaveTrackTCPLines[] = {
   COMMON_ITEMS
   MUTE_SOLO_ITEMS(2)
   { TCPLine::kItemGain, kTrackInfoSliderHeight, kTrackInfoSliderExtra,
     &TrackInfo::GainSliderDrawFunction },
   { TCPLine::kItemPan, kTrackInfoSliderHeight, kTrackInfoSliderExtra,
     &TrackInfo::PanSliderDrawFunction },
   STATUS_ITEMS
};

int totalTCPLines( const TCPLines &lines, bool omitLastExtra )
{
   int total = 0;
   int lastExtra = 0;
   for ( const auto line : lines ) {
      lastExtra = line.extraSpace;
      total += line.height + lastExtra;
   }
   if (omitLastExtra)
      total -= lastExtra;
   return total;
}

// Items for the bottom of the panel, listed bottom-upwards
// As also with the top items, the extra space is below the item
const TCPLine defaultCommonTrackTCPBottomLines[] = {
   // The '0' avoids impinging on bottom line of TCP
   // Use -1 if you do want to do so.
   { TCPLine::kItemSyncLock | TCPLine::kItemMinimize, kTrackInfoBtnSize, 0,
     &TrackInfo::MinimizeSyncLockDrawFunction },
};
TCPLines commonTrackTCPBottomLines{ RANGE(defaultCommonTrackTCPBottomLines) };

// return y value and height
std::pair< int, int > CalcBottomItemY
   ( const TCPLines &lines, unsigned iItem, int height )
{
   int y = height;
   auto pLines = lines.begin();
   while ( pLines != lines.end() &&
           0 == (pLines->items & iItem) ) {
      y -= pLines->height + pLines->extraSpace;
      ++pLines;
   }
   if (pLines != lines.end())
      y -= (pLines->height + pLines->extraSpace );
   return { y, pLines->height };
}

void DrawItems_
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack,
  const TCPLines &topLines, const TCPLines &bottomLines )
{
   auto dc = &context.dc;
   TrackInfo::SetTrackInfoFont(dc);
   dc->SetTextForeground(theTheme.Colour(clrTrackPanelText));

   {
      int yy = 0;
      for ( const auto &line : topLines ) {
         wxRect itemRect{
            rect.x, rect.y + yy,
            rect.width, line.height
         };
         if ( !TrackInfo::HideTopItem( rect, itemRect ) &&
              line.drawFunction )
            line.drawFunction( context, itemRect, pTrack );
         yy += line.height + line.extraSpace;
      }
   }
   {
      int yy = rect.height;
      for ( const auto &line : bottomLines ) {
         yy -= line.height + line.extraSpace;
         if ( line.drawFunction ) {
            wxRect itemRect{
               rect.x, rect.y + yy,
               rect.width, line.height
            };
            line.drawFunction( context, itemRect, pTrack );
         }
      }
   }
}

void DrawItems
( TrackPanelDrawingContext &context,
  const wxRect &rect, const TCPLines &topLines, const Track &track  )
{
   const auto bottomLines = commonTrackTCPBottomLines;
   DrawItems_( context, rect, &track, topLines, bottomLines );
}

}

const TCPLines &TrackControls::GetControlLines() const
{
   return commonTrackTCPLines;
}

void TrackInfo::GetCloseBoxHorizontalBounds( const wxRect & rect, wxRect &dest )
{
   dest.x = rect.x;
   dest.width = kTrackInfoBtnSize;
}

void TrackInfo::GetCloseBoxRect(const wxRect & rect, wxRect & dest)
{
   GetCloseBoxHorizontalBounds( rect, dest );
   auto results = CalcItemY( commonTrackTCPLines, TCPLine::kItemBarButtons );
   dest.y = rect.y + results.first;
   dest.height = results.second;
}

static const int TitleSoloBorderOverlap = 1;

void TrackInfo::GetTitleBarHorizontalBounds( const wxRect & rect, wxRect &dest )
{
   // to right of CloseBoxRect, plus a little more
   wxRect closeRect;
   GetCloseBoxHorizontalBounds( rect, closeRect );
   dest.x = rect.x + closeRect.width + 1;
   dest.width = rect.x + rect.width - dest.x + TitleSoloBorderOverlap;
}

void TrackInfo::GetTitleBarRect(const wxRect & rect, wxRect & dest)
{
   GetTitleBarHorizontalBounds( rect, dest );
   auto results = CalcItemY( commonTrackTCPLines, TCPLine::kItemBarButtons );
   dest.y = rect.y + results.first;
   dest.height = results.second;
}

void TrackInfo::GetNarrowMuteHorizontalBounds( const wxRect & rect, wxRect &dest )
{
   dest.x = rect.x;
   dest.width = rect.width / 2 + 1;
}

void TrackInfo::GetNarrowSoloHorizontalBounds( const wxRect & rect, wxRect &dest )
{
   wxRect muteRect;
   GetNarrowMuteHorizontalBounds( rect, muteRect );
   dest.x = rect.x + muteRect.width;
   dest.width = rect.width - muteRect.width + TitleSoloBorderOverlap;
}

void TrackInfo::GetWideMuteSoloHorizontalBounds( const wxRect & rect, wxRect &dest )
{
   // Larger button, symmetrically placed intended.
   // On windows this gives 15 pixels each side.
   dest.width = rect.width - 2 * kTrackInfoBtnSize + 6;
   dest.x = rect.x + kTrackInfoBtnSize -3;
}

void TrackInfo::GetMuteSoloRect
(const wxRect & rect, wxRect & dest, bool solo, bool bHasSoloButton,
 const Track *pTrack)
{

   const auto &lines = TrackControls::Get( *pTrack ).GetControlLines();
   auto resultsM = CalcItemY( lines, TCPLine::kItemMute );
   auto resultsS = CalcItemY( lines, TCPLine::kItemSolo );
   dest.height = resultsS.second;

   int yMute = resultsM.first;
   int ySolo = resultsS.first;

   bool bSameRow = ( yMute == ySolo );
   bool bNarrow = bSameRow && bHasSoloButton;

   if( bNarrow )
   {
      if( solo )
         GetNarrowSoloHorizontalBounds( rect, dest );
      else
         GetNarrowMuteHorizontalBounds( rect, dest );
   }
   else
      GetWideMuteSoloHorizontalBounds( rect, dest );

   if( bSameRow || !solo )
      dest.y = rect.y + yMute;
   else
      dest.y = rect.y + ySolo;

}

void TrackInfo::GetSliderHorizontalBounds( const wxPoint &topleft, wxRect &dest )
{
   dest.x = topleft.x + 6;
   dest.width = kTrackInfoSliderWidth;
}

void TrackInfo::GetGainRect(const wxPoint &topleft, wxRect & dest)
{
   GetSliderHorizontalBounds( topleft, dest );
   auto results = CalcItemY( waveTrackTCPLines, TCPLine::kItemGain );
   dest.y = topleft.y + results.first;
   dest.height = results.second;
}

void TrackInfo::GetPanRect(const wxPoint &topleft, wxRect & dest)
{
   GetGainRect( topleft, dest );
   auto results = CalcItemY( waveTrackTCPLines, TCPLine::kItemPan );
   dest.y = topleft.y + results.first;
}

void TrackInfo::GetMinimizeHorizontalBounds( const wxRect &rect, wxRect &dest )
{
   const int space = 0;// was 3.
   dest.x = rect.x + space;

   wxRect syncLockRect;
   GetSyncLockHorizontalBounds( rect, syncLockRect );

   // Width is rect.width less space on left for track select
   // and on right for sync-lock icon.
   dest.width = kTrackInfoBtnSize;
   // rect.width - (space + syncLockRect.width);
}

void TrackInfo::GetMinimizeRect(const wxRect & rect, wxRect &dest)
{
   GetMinimizeHorizontalBounds( rect, dest );
   auto results = CalcBottomItemY
      ( commonTrackTCPBottomLines, TCPLine::kItemMinimize, rect.height);
   dest.y = rect.y + results.first;
   dest.height = results.second;
}

void TrackInfo::GetSelectButtonHorizontalBounds( const wxRect &rect, wxRect &dest )
{
   const int space = 0;// was 3.
   dest.x = rect.x + space;

   wxRect syncLockRect;
   GetSyncLockHorizontalBounds( rect, syncLockRect );
   wxRect minimizeRect;
   GetMinimizeHorizontalBounds( rect, minimizeRect );

   dest.x = dest.x + space + minimizeRect.width;
   // Width is rect.width less space on left for track select
   // and on right for sync-lock icon.
   dest.width = rect.width - (space + syncLockRect.width) - (space + minimizeRect.width);
}

void TrackInfo::GetSelectButtonRect(const wxRect & rect, wxRect &dest)
{
   GetSelectButtonHorizontalBounds( rect, dest );
   auto results = CalcBottomItemY
      ( commonTrackTCPBottomLines, TCPLine::kItemMinimize, rect.height);
   dest.y = rect.y + results.first;
   dest.height = results.second;
}

void TrackInfo::GetSyncLockHorizontalBounds( const wxRect &rect, wxRect &dest )
{
   dest.width = kTrackInfoBtnSize;
   dest.x = rect.x + rect.width - dest.width;
}

void TrackInfo::GetSyncLockIconRect(const wxRect & rect, wxRect &dest)
{
   GetSyncLockHorizontalBounds( rect, dest );
   auto results = CalcBottomItemY
      ( commonTrackTCPBottomLines, TCPLine::kItemSyncLock, rect.height);
   dest.y = rect.y + results.first;
   dest.height = results.second;
}

#include "../../tracks/ui/TrackButtonHandles.h"
void TrackInfo::CloseTitleDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto dc = &context.dc;
   bool selected = pTrack ? pTrack->GetSelected() : true;
   {
      wxRect bev = rect;
      GetCloseBoxHorizontalBounds( rect, bev );
      auto target = dynamic_cast<CloseButtonHandle*>( context.target.get() );
      bool hit = target && target->GetTrack().get() == pTrack;
      bool captured = hit && target->IsClicked();
      bool down = captured && bev.Contains( context.lastState.GetPosition());
      AColor::Bevel2(*dc, !down, bev, selected, hit );

#ifdef EXPERIMENTAL_THEMING
      wxPen pen( theTheme.Colour( clrTrackPanelText ));
      dc->SetPen( pen );
#else
      dc->SetPen(*wxBLACK_PEN);
#endif
      bev.Inflate( -1, -1 );
      // Draw the "X"
      const int s = 6;

      int ls = bev.x + ((bev.width - s) / 2);
      int ts = bev.y + ((bev.height - s) / 2);
      int rs = ls + s;
      int bs = ts + s;

      AColor::Line(*dc, ls,     ts, rs,     bs);
      AColor::Line(*dc, ls + 1, ts, rs + 1, bs);
      AColor::Line(*dc, rs,     ts, ls,     bs);
      AColor::Line(*dc, rs + 1, ts, ls + 1, bs);

      //   bev.Inflate(-1, -1);
   }

   {
      wxRect bev = rect;
      GetTitleBarHorizontalBounds( rect, bev );
      auto target = dynamic_cast<MenuButtonHandle*>( context.target.get() );
      bool hit = target && target->GetTrack().get() == pTrack;
      bool captured = hit && target->IsClicked();
      bool down = captured && bev.Contains( context.lastState.GetPosition());
      const auto *pGroupData = pTrack ? &pTrack->GetGroupData() : nullptr;
      wxString titleStr =
         pGroupData ? pGroupData->GetName() : _("Name");

      //bev.Inflate(-1, -1);
      AColor::Bevel2(*dc, !down, bev, selected, hit);

      // Draw title text
      SetTrackInfoFont(dc);

      // Bug 1660 The 'k' of 'Audio Track' was being truncated.
      // Constant of 32 found by counting pixels on a windows machine.
      // I believe it's the size of the X close button + the size of the 
      // drop down arrow.
      int allowableWidth = rect.width - 32;

      wxCoord textWidth, textHeight;
      dc->GetTextExtent(titleStr, &textWidth, &textHeight);
      while (textWidth > allowableWidth) {
         titleStr = titleStr.Left(titleStr.length() - 1);
         dc->GetTextExtent(titleStr, &textWidth, &textHeight);
      }

      // Pop-up triangle
   #ifdef EXPERIMENTAL_THEMING
      wxColour c = theTheme.Colour( clrTrackPanelText );
   #else
      wxColour c = *wxBLACK;
   #endif

      // wxGTK leaves little scraps (antialiasing?) of the
      // characters if they are repeatedly drawn.  This
      // happens when holding down mouse button and moving
      // in and out of the title bar.  So clear it first.
   //   AColor::MediumTrackInfo(dc, t->GetSelected());
   //   dc->DrawRectangle(bev);

      dc->SetTextForeground( c );
      dc->SetTextBackground( wxTRANSPARENT );
      dc->DrawText(titleStr, bev.x + 2, bev.y + (bev.height - textHeight) / 2);



      dc->SetPen(c);
      dc->SetBrush(c);

      int s = 10; // Width of dropdown arrow...height is half of width
      AColor::Arrow(*dc,
                    bev.GetRight() - s - 3, // 3 to offset from right border
                    bev.y + ((bev.height - (s / 2)) / 2),
                    s);

   }
}

void TrackInfo::MinimizeSyncLockDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto dc = &context.dc;
   bool selected = pTrack ? pTrack->GetSelected() : true;
   bool syncLockSelected = pTrack ? pTrack->IsSyncLockSelected() : true;
   bool minimized =
      pTrack ? TrackViewGroupData::Get( *pTrack ).GetMinimized() : false;
   {
      wxRect bev = rect;
      GetMinimizeHorizontalBounds(rect, bev);
      auto target = dynamic_cast<MinimizeButtonHandle*>( context.target.get() );
      bool hit = target && target->GetTrack().get() == pTrack;
      bool captured = hit && target->IsClicked();
      bool down = captured && bev.Contains( context.lastState.GetPosition());

      // Clear background to get rid of previous arrow
      //AColor::MediumTrackInfo(dc, t->GetSelected());
      //dc->DrawRectangle(bev);

      AColor::Bevel2(*dc, !down, bev, selected, hit);

#ifdef EXPERIMENTAL_THEMING
      wxColour c = theTheme.Colour(clrTrackPanelText);
      dc->SetBrush(c);
      dc->SetPen(c);
#else
      AColor::Dark(dc, selected);
#endif

      AColor::Arrow(*dc,
                    bev.x - 5 + bev.width / 2,
                    bev.y - 2 + bev.height / 2,
                    10,
                    minimized);
   }

   {
      wxRect bev = rect;
      GetSelectButtonHorizontalBounds(rect, bev);
      auto target = dynamic_cast<SelectButtonHandle*>( context.target.get() );
      bool hit = target && target->GetTrack().get() == pTrack;
      bool captured = hit && target->IsClicked();
      bool down = captured && bev.Contains( context.lastState.GetPosition());

      AColor::Bevel2(*dc, !down, bev, selected, hit);

#ifdef EXPERIMENTAL_THEMING
      wxColour c = theTheme.Colour(clrTrackPanelText);
      dc->SetBrush(c);
      dc->SetPen(c);
#else
      AColor::Dark(dc, selected);
#endif

      wxString str = _("Select");
      wxCoord textWidth;
      wxCoord textHeight;
      SetTrackInfoFont(dc);
      dc->GetTextExtent(str, &textWidth, &textHeight);

      dc->SetTextForeground( c );
      dc->SetTextBackground( wxTRANSPARENT );
      dc->DrawText(str, bev.x + 2 + (bev.width-textWidth)/2, bev.y + (bev.height - textHeight) / 2);
   }

   // Draw the sync-lock indicator if this track is in a sync-lock selected group.
   if (syncLockSelected)
   {
      wxRect syncLockIconRect = rect;
	
      GetSyncLockHorizontalBounds( rect, syncLockIconRect );
      wxBitmap syncLockBitmap(theTheme.Image(bmpSyncLockIcon));
      // Icon is 12x12 and syncLockIconRect is 16x16.
      dc->DrawBitmap(syncLockBitmap,
                     syncLockIconRect.x + 3,
                     syncLockIconRect.y + 2,
                     true);
   }
}

#include "../../tracks/playabletrack/notetrack/ui/NoteTrackButtonHandle.h"
#include "../../NoteTrack.h"
void TrackInfo::MidiControlsDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
#ifdef EXPERIMENTAL_MIDI_OUT
   auto target = dynamic_cast<NoteTrackButtonHandle*>( context.target.get() );
   bool hit = target && target->GetTrack().get() == pTrack;
   auto channel = hit ? target->GetChannel() : -1;
   auto &dc = context.dc;
   wxRect midiRect = rect;
   GetMidiControlsHorizontalBounds(rect, midiRect);
   NoteTrack::DrawLabelControls
      ( static_cast<const NoteTrack *>(pTrack), dc, midiRect, channel );
#endif // EXPERIMENTAL_MIDI_OUT
}

#include "../../widgets/ASlider.h"
template<typename TrackClass>
void TrackInfo::SliderDrawFunction
( LWSlider *(*Selector)
    (const wxRect &sliderRect, const TrackClass *t, bool captured, wxWindow*),
  wxDC *dc, const wxRect &rect, const Track *pTrack,
  bool captured, bool highlight )
{
   wxRect sliderRect = rect;
   TrackInfo::GetSliderHorizontalBounds( rect.GetTopLeft(), sliderRect );
   auto wt = static_cast<const TrackClass*>( pTrack );
   Selector( sliderRect, wt, captured, nullptr )->OnPaint(*dc, highlight);
}

#include "../../tracks/playabletrack/wavetrack/ui/WaveTrackSliderHandles.h"
void TrackInfo::PanSliderDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto target = dynamic_cast<PanSliderHandle*>( context.target.get() );
   auto dc = &context.dc;
   bool hit = target && target->GetTrack().get() == pTrack;
   bool captured = hit && target->IsClicked();
   SliderDrawFunction<WaveTrack>
      ( &TrackInfo::PanSlider, dc, rect, pTrack, captured, hit);
}

void TrackInfo::GainSliderDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto target = dynamic_cast<GainSliderHandle*>( context.target.get() );
   auto dc = &context.dc;
   bool hit = target && target->GetTrack().get() == pTrack;
   if( hit )
      hit=hit;
   bool captured = hit && target->IsClicked();
   SliderDrawFunction<WaveTrack>
      ( &TrackInfo::GainSlider, dc, rect, pTrack, captured, hit);
}

#ifdef EXPERIMENTAL_MIDI_OUT
#include "../../tracks/playabletrack/notetrack/ui/NoteTrackControls.h"
#include "../../tracks/playabletrack/notetrack/ui/NoteTrackSliderHandles.h"
void TrackInfo::VelocitySliderDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto dc = &context.dc;
   auto target = dynamic_cast<VelocitySliderHandle*>( context.target.get() );
   bool hit = target && target->GetTrack().get() == pTrack;
   bool captured = hit && target->IsClicked();
   SliderDrawFunction<NoteTrack>
      ( &NoteTrackControls::VelocitySlider, dc, rect, pTrack, captured, hit);
}
#endif

void TrackInfo::MuteOrSoloDrawFunction
( wxDC *dc, const wxRect &bev, const Track *pTrack, bool down, 
  bool WXUNUSED(captured),
  bool solo, bool hit )
{
   //bev.Inflate(-1, -1);
   bool selected = pTrack ? pTrack->GetSelected() : true;
   auto pt = dynamic_cast<const PlayableTrack *>(pTrack);
   bool value = pt ? (solo ? pt->GetSolo() : pt->GetMute()) : false;

#if 0
   AColor::MediumTrackInfo( dc, t->GetSelected());
   if( solo )
   {
      if( pt && pt->GetSolo() )
      {
         AColor::Solo(dc, pt->GetSolo(), t->GetSelected());
      }
   }
   else
   {
      if( pt && pt->GetMute() )
      {
         AColor::Mute(dc, pt->GetMute(), t->GetSelected(), pt->GetSolo());
      }
   }
   //(solo) ? AColor::Solo(dc, t->GetSolo(), t->GetSelected()) :
   //    AColor::Mute(dc, t->GetMute(), t->GetSelected(), t->GetSolo());
   dc->SetPen( *wxTRANSPARENT_PEN );//No border!
   dc->DrawRectangle(bev);
#endif

   wxCoord textWidth, textHeight;
   wxString str = (solo) ?
      /* i18n-hint: This is on a button that will silence all the other tracks.*/
      _("Solo") :
      /* i18n-hint: This is on a button that will silence this track.*/
      _("Mute");

   AColor::Bevel2(
      *dc,
      value == down,
      bev,
      selected, hit
   );

   SetTrackInfoFont(dc);
   dc->GetTextExtent(str, &textWidth, &textHeight);
   dc->DrawText(str, bev.x + (bev.width - textWidth) / 2, bev.y + (bev.height - textHeight) / 2);
}

#include "../../tracks/playabletrack/ui/PlayableTrackButtonHandles.h"
void TrackInfo::WideMuteDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto dc = &context.dc;
   wxRect bev = rect;
   GetWideMuteSoloHorizontalBounds( rect, bev );
   auto target = dynamic_cast<MuteButtonHandle*>( context.target.get() );
   bool hit = target && target->GetTrack().get() == pTrack;
   bool captured = hit && target->IsClicked();
   bool down = captured && bev.Contains( context.lastState.GetPosition());
   MuteOrSoloDrawFunction( dc, bev, pTrack, down, captured, false, hit );
}

void TrackInfo::WideSoloDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto dc = &context.dc;
   wxRect bev = rect;
   GetWideMuteSoloHorizontalBounds( rect, bev );
   auto target = dynamic_cast<SoloButtonHandle*>( context.target.get() );
   bool hit = target && target->GetTrack().get() == pTrack;
   bool captured = hit && target->IsClicked();
   bool down = captured && bev.Contains( context.lastState.GetPosition());
   MuteOrSoloDrawFunction( dc, bev, pTrack, down, captured, true, hit );
}

void TrackInfo::MuteAndSoloDrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto dc = &context.dc;
   bool bHasSoloButton = TrackPanel::HasSoloButton();

   wxRect bev = rect;
   if ( bHasSoloButton )
      GetNarrowMuteHorizontalBounds( rect, bev );
   else
      GetWideMuteSoloHorizontalBounds( rect, bev );
   {
      auto target = dynamic_cast<MuteButtonHandle*>( context.target.get() );
      bool hit = target && target->GetTrack().get() == pTrack;
      bool captured = hit && target->IsClicked();
      bool down = captured && bev.Contains( context.lastState.GetPosition());
      MuteOrSoloDrawFunction( dc, bev, pTrack, down, captured, false, hit );
   }

   if( !bHasSoloButton )
      return;

   GetNarrowSoloHorizontalBounds( rect, bev );
   {
      auto target = dynamic_cast<SoloButtonHandle*>( context.target.get() );
      bool hit = target && target->GetTrack().get() == pTrack;
      bool captured = hit && target->IsClicked();
      bool down = captured && bev.Contains( context.lastState.GetPosition());
      MuteOrSoloDrawFunction( dc, bev, pTrack, down, captured, true, hit );
   }
}

void TrackInfo::StatusDrawFunction
   ( const wxString &string, wxDC *dc, const wxRect &rect )
{
   static const int offset = 3;
   dc->DrawText(string, rect.x + offset, rect.y);
}

#include "../../WaveTrack.h"

void TrackInfo::Status1DrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto dc = &context.dc;
   auto wt = static_cast<const WaveTrack*>(pTrack);

   /// Returns the string to be displayed in the track label
   /// indicating whether the track is mono, left, right, or
   /// stereo and what sample rate it's using.
   auto rate = wt ? wt->GetRate() : 44100.0;
   wxString s;
   if (!pTrack || TrackList::Channels(pTrack).size() > 1)
      // TODO: more-than-two-channels-message
      // more appropriate strings
      s = _("Stereo, %dHz");
   else {
      if (wt->GetChannel() == WaveTrack::MonoChannel)
         s = _("Mono, %dHz");
      else if (wt->GetChannel() == WaveTrack::LeftChannel)
         s = _("Left, %dHz");
      else if (wt->GetChannel() == WaveTrack::RightChannel)
         s = _("Right, %dHz");
   }
   s = wxString::Format( s, (int) (rate + 0.5) );

   StatusDrawFunction( s, dc, rect );
}

void TrackInfo::Status2DrawFunction
( TrackPanelDrawingContext &context,
  const wxRect &rect, const Track *pTrack )
{
   auto dc = &context.dc;
   auto wt = static_cast<const WaveTrack*>(pTrack);
   auto format = wt ? wt->GetSampleFormat() : floatSample;
   auto s = GetSampleFormatStr(format);
   StatusDrawFunction( s, dc, rect );
}

void TrackControls::Draw(
   TrackPanelDrawingContext &context,
   const wxRect &rect, unsigned iPass )
{
   if ( iPass == TrackArtist::PassMargins ) {
      // fill in label
      auto dc = &context.dc;
      const auto pTrack = FindTrack();
      AColor::MediumTrackInfo( dc, pTrack && pTrack->GetSelected() );
      dc->DrawRectangle( rect );
   }
   
   if ( iPass == TrackArtist::PassControls ) {
      // rectangle?
      auto pTrack = FindTrack();
      if (pTrack)
         DrawItems( context, rect, GetControlLines(), *pTrack );
   }

   // Some old cut-and-paste legacy from TrackPanel.cpp here:
#undef USE_BEVELS
#ifdef USE_BEVELS
   // This branch is not now used
   // PRL:  todo:  banish magic numbers.
   // PRL: vrul was the x coordinate of left edge of the vertical ruler.
   // PRL: bHasMuteSolo was true iff the track was WaveTrack.
   if( bHasMuteSolo )
   {
      int ylast = rect.height-20;
      int ybutton = wxMin(32,ylast-17);
      int ybuttonEnd = 67;

      fill=wxRect( rect.x+1, rect.y+17, vrul-6, ybutton);
      AColor::BevelTrackInfo( *dc, true, fill );
   
      if( ybuttonEnd < ylast ){
         fill=wxRect( rect.x+1, rect.y+ybuttonEnd, fill.width, ylast - ybuttonEnd);
         AColor::BevelTrackInfo( *dc, true, fill );
      }
   }
   else
   {
      fill=wxRect( rect.x+1, rect.y+17, vrul-6, rect.height-37);
      AColor::BevelTrackInfo( *dc, true, fill );
   }
#endif

}

unsigned TrackInfo::MinimumTrackHeight()
{
   unsigned height = 0;
   if (!commonTrackTCPLines.empty())
      height += commonTrackTCPLines.front().height;
   if (!commonTrackTCPBottomLines.empty())
      height += commonTrackTCPBottomLines.front().height;
   // + 1 prevents the top item from disappearing for want of enough space,
   // according to the rules in HideTopItem.
   return height + kTopMargin + kBottomMargin + 1;
}

bool TrackInfo::HideTopItem( const wxRect &rect, const wxRect &subRect,
                 int allowance ) {
   auto limit = CalcBottomItemY
   ( commonTrackTCPBottomLines, TCPLine::kHighestBottomItem, rect.height).first;
   // Return true if the rectangle is even touching the limit
   // without an overlap.  That was the behavior as of 2.1.3.
   return subRect.y + subRect.height - allowance >= rect.y + limit;
}

unsigned TrackControls::FindDefaultTrackHeight( const TCPLines &topLines )
{
   int needed =
   kTopMargin + kBottomMargin +
   totalTCPLines( topLines, true ) +
   totalTCPLines( commonTrackTCPBottomLines, false ) + 1;
   return (unsigned) std::max( needed, (int) TrackView::DefaultHeight );
}

unsigned TrackControls::DefaultTrackHeight() const
{
   return FindDefaultTrackHeight( GetControlLines() );
}

unsigned TrackInfo::DefaultWaveTrackHeight()
{
   return TrackControls::FindDefaultTrackHeight( waveTrackTCPLines );
}

#include "../playabletrack/wavetrack/ui/WaveTrackControls.h"
const TCPLines &WaveTrackControls::GetControlLines() const
{
   return waveTrackTCPLines;
}

const TCPLines waveTrackTCPLines{ RANGE(defaultWaveTrackTCPLines) };
