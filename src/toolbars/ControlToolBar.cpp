/**********************************************************************

  Audacity: A Digital Audio Editor

  ControlToolBar.cpp

  Dominic Mazzoni
  Shane T. Mueller
  James Crook
  Leland Lucius

*******************************************************************//**

\class ControlToolBar
\brief A ToolBar that has the main Transport buttons.

  In the GUI, this is referred to as "Transport Toolbar", as
  it corresponds to commands in the Transport menu.
  "Control Toolbar" is historic.
  This class, which is a child of Toolbar, creates the
  window containing the Transport (rewind/play/stop/record/ff)
  buttons. The window can be embedded within a
  normal project window, or within a ToolBarFrame.

  All of the controls in this window were custom-written for
  Audacity - they are not native controls on any platform -
  however, it is intended that the images could be easily
  replaced to allow "skinning" or just customization to
  match the look and feel of each platform.

*//*******************************************************************/

#include "../Audacity.h" // for USE_* macros
#include "ControlToolBar.h"

#include "../Experimental.h"

#include <algorithm>
#include <cfloat>

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#include <wx/setup.h> // for wxUSE_* macros

#ifndef WX_PRECOMP
#include <wx/app.h>
#include <wx/dc.h>
#include <wx/event.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/timer.h>
#endif
#include <wx/tooltip.h>
#include <wx/datetime.h>

#include "TranscriptionToolBar.h"

#include "../AColor.h"
#include "../AllThemeResources.h"
#include "../AudioIO.h"
#include "../ImageManipulation.h"
#include "../Prefs.h"
#include "../Project.h"
#include "../Theme.h"
#include "../ViewInfo.h"
#include "../WaveTrack.h"
#include "../widgets/AButton.h"
#include "../widgets/LinkingHtmlWindow.h"
#include "../widgets/ErrorDialog.h"
#include "../FileNames.h"

#include "../tracks/ui/Scrubbing.h"
#include "../tracks/ui/TrackView.h"
#include "../TransportState.h"

IMPLEMENT_CLASS(ControlToolBar, ToolBar);

////////////////////////////////////////////////////////////
/// Methods for ControlToolBar
////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(ControlToolBar, ToolBar)
   EVT_CHAR(ControlToolBar::OnKeyEvent)
   EVT_BUTTON(ID_PLAY_BUTTON,   ControlToolBar::OnPlay)
   EVT_BUTTON(ID_STOP_BUTTON,   ControlToolBar::OnStop)
   EVT_BUTTON(ID_RECORD_BUTTON, ControlToolBar::OnRecord)
   EVT_BUTTON(ID_REW_BUTTON,    ControlToolBar::OnRewind)
   EVT_BUTTON(ID_FF_BUTTON,     ControlToolBar::OnFF)
   EVT_BUTTON(ID_PAUSE_BUTTON,  ControlToolBar::OnPause)
END_EVENT_TABLE()

//Standard constructor
// This was called "Control" toolbar in the GUI before - now it is "Transport".
// Note that we use the legacy "Control" string as the section because this
// gets written to prefs and cannot be changed in prefs to maintain backwards
// compatibility
ControlToolBar::ControlToolBar()
: ToolBar(TransportBarID, _("Transport"), wxT("Control"))
{
   mPaused = false;

   gPrefs->Read(wxT("/GUI/ErgonomicTransportButtons"), &mErgonomicTransportButtons, true);
   mStrLocale = gPrefs->Read(wxT("/Locale/Language"), wxT(""));

   mSizer = NULL;

   /* i18n-hint: These are strings for the status bar, and indicate whether Audacity
   is playing or recording or stopped, and whether it is paused. */
   mStatePlay = XO("Playing");
   mStateStop = XO("Stopped");
   mStateRecord = XO("Recording");
   mStatePause = XO("Paused");
}

ControlToolBar::~ControlToolBar()
{
}


void ControlToolBar::Create(wxWindow * parent)
{
   ToolBar::Create(parent);
   UpdatePrefs();
}

// This is a convenience function that allows for button creation in
// MakeButtons() with fewer arguments
AButton *ControlToolBar::MakeButton(ControlToolBar *pBar,
                                    teBmps eEnabledUp, teBmps eEnabledDown, teBmps eDisabled,
                                    int id,
                                    bool processdownevents,
                                    const wxChar *label)
{
   AButton *r = ToolBar::MakeButton(pBar,
      bmpRecoloredUpLarge, bmpRecoloredDownLarge, bmpRecoloredUpHiliteLarge, bmpRecoloredHiliteLarge,
      eEnabledUp, eEnabledDown, eDisabled,
      wxWindowID(id),
      wxDefaultPosition, processdownevents,
      theTheme.ImageSize( bmpRecoloredUpLarge ));
   r->SetLabel(label);
   enum { deflation =
#ifdef __WXMAC__
      6
#else
      12
#endif
   };
   r->SetFocusRect( r->GetClientRect().Deflate( deflation, deflation ) );

   return r;
}

// static
void ControlToolBar::MakeAlternateImages(AButton &button, int idx,
                                         teBmps eEnabledUp,
                                         teBmps eEnabledDown,
                                         teBmps eDisabled)
{
   ToolBar::MakeAlternateImages(button, idx,
      bmpRecoloredUpLarge, bmpRecoloredDownLarge, bmpRecoloredUpHiliteLarge, bmpRecoloredHiliteLarge,
      eEnabledUp, eEnabledDown, eDisabled,
      theTheme.ImageSize( bmpRecoloredUpLarge ));
}

void ControlToolBar::Populate()
{
   SetBackgroundColour( theTheme.Colour( clrMedium  ) );
   MakeButtonBackgroundsLarge();

   mPause = MakeButton(this, bmpPause, bmpPause, bmpPauseDisabled,
      ID_PAUSE_BUTTON,  true,  _("Pause"));

   mPlay = MakeButton(this, bmpPlay, bmpPlay, bmpPlayDisabled,
      ID_PLAY_BUTTON, true, _("Play"));
   MakeAlternateImages(*mPlay, 1, bmpLoop, bmpLoop, bmpLoopDisabled);
   MakeAlternateImages(*mPlay, 2,
      bmpCutPreview, bmpCutPreview, bmpCutPreviewDisabled);
   MakeAlternateImages(*mPlay, 3,
                       bmpScrub, bmpScrub, bmpScrubDisabled);
   MakeAlternateImages(*mPlay, 4,
                       bmpSeek, bmpSeek, bmpSeekDisabled);
   mPlay->FollowModifierKeys();

   mStop = MakeButton(this, bmpStop, bmpStop, bmpStopDisabled ,
      ID_STOP_BUTTON, false, _("Stop"));

   mRewind = MakeButton(this, bmpRewind, bmpRewind, bmpRewindDisabled,
      ID_REW_BUTTON, false, _("Skip to Start"));

   mFF = MakeButton(this, bmpFFwd, bmpFFwd, bmpFFwdDisabled,
      ID_FF_BUTTON, false, _("Skip to End"));

   mRecord = MakeButton(this, bmpRecord, bmpRecord, bmpRecordDisabled,
      ID_RECORD_BUTTON, false, _("Record"));

   bool bPreferNewTrack;
   gPrefs->Read("/GUI/PreferNewTrackRecord",&bPreferNewTrack, false);
   if( !bPreferNewTrack )
      MakeAlternateImages(*mRecord, 1, bmpRecordBelow, bmpRecordBelow,
         bmpRecordBelowDisabled);
   else
      MakeAlternateImages(*mRecord, 1, bmpRecordBeside, bmpRecordBeside,
         bmpRecordBesideDisabled);

   mRecord->FollowModifierKeys();

#if wxUSE_TOOLTIPS
   RegenerateTooltips();
   wxToolTip::Enable(true);
   wxToolTip::SetDelay(1000);
#endif

   // Set default order and mode
   ArrangeButtons();
}

void ControlToolBar::RegenerateTooltips()
{
#if wxUSE_TOOLTIPS
   for (long iWinID = ID_PLAY_BUTTON; iWinID < BUTTON_COUNT; iWinID++)
   {
      auto pCtrl = static_cast<AButton*>(this->FindWindow(iWinID));
      CommandID name;
      switch (iWinID)
      {
         case ID_PLAY_BUTTON:
            // Without shift
            name = wxT("PlayStop");
            break;
         case ID_RECORD_BUTTON:
            // Without shift
            //name = wxT("Record");
            name = wxT("Record1stChoice");
            break;
         case ID_PAUSE_BUTTON:
            name = wxT("Pause");
            break;
         case ID_STOP_BUTTON:
            name = wxT("Stop");
            break;
         case ID_FF_BUTTON:
            name = wxT("CursProjectEnd");
            break;
         case ID_REW_BUTTON:
            name = wxT("CursProjectStart");
            break;
      }
      std::vector<TranslatedInternalString> commands(
         1u, { name, pCtrl->GetLabel() } );

      // Some have a second
      switch (iWinID)
      {
         case ID_PLAY_BUTTON:
            // With shift
            commands.push_back( { wxT("PlayLooped"), _("Loop Play") } );
            break;
         case ID_RECORD_BUTTON:
            // With shift
            {  bool bPreferNewTrack;
               gPrefs->Read("/GUI/PreferNewTrackRecord",&bPreferNewTrack, false);
               // For the shortcut tooltip.
               commands.push_back( {
                  wxT("Record2ndChoice"),
                  !bPreferNewTrack
                     ? _("Record New Track")
                     : _("Append Record")
               } );
            }
            break;
         case ID_PAUSE_BUTTON:
            break;
         case ID_STOP_BUTTON:
            break;
         case ID_FF_BUTTON:
            // With shift
            commands.push_back( {
               wxT("SelEnd"), _("Select to End") } );
            break;
         case ID_REW_BUTTON:
            // With shift
            commands.push_back( {
               wxT("SelStart"), _("Select to Start") } );
            break;
      }
      ToolBar::SetButtonToolTip(*pCtrl, commands.data(), commands.size());
   }
#endif
}

void ControlToolBar::UpdatePrefs()
{
   bool updated = false;
   bool active;

   gPrefs->Read( wxT("/GUI/ErgonomicTransportButtons"), &active, true );
   if( mErgonomicTransportButtons != active )
   {
      mErgonomicTransportButtons = active;
      updated = true;
   }
   wxString strLocale = gPrefs->Read(wxT("/Locale/Language"), wxT(""));
   if (mStrLocale != strLocale)
   {
      mStrLocale = strLocale;
      updated = true;
   }

   if( updated )
   {
      ReCreateButtons(); // side effect: calls RegenerateTooltips()
      Updated();
   }
   else
      // The other reason to regenerate tooltips is if keyboard shortcuts for
      // transport buttons changed, but that's too much work to check for, so just
      // always do it. (Much cheaper than calling ReCreateButtons() in all cases.
      RegenerateTooltips();


   // Set label to pull in language change
   SetLabel(_("Transport"));

   // Give base class a chance
   ToolBar::UpdatePrefs();
}

void ControlToolBar::ArrangeButtons()
{
   int flags = wxALIGN_CENTER | wxRIGHT;

   // (Re)allocate the button sizer
   if( mSizer )
   {
      Detach( mSizer );
      std::unique_ptr < wxSizer > {mSizer}; // DELETE it
   }

   Add((mSizer = safenew wxBoxSizer(wxHORIZONTAL)), 1, wxEXPAND);

   // Start with a little extra space
   mSizer->Add( 5, 55 );

   // Add the buttons in order based on ergonomic setting
   if( mErgonomicTransportButtons )
   {
      mPause->MoveBeforeInTabOrder( mRecord );
      mPlay->MoveBeforeInTabOrder( mRecord );
      mStop->MoveBeforeInTabOrder( mRecord );
      mRewind->MoveBeforeInTabOrder( mRecord );
      mFF->MoveBeforeInTabOrder( mRecord );

      mSizer->Add( mPause,  0, flags, 2 );
      mSizer->Add( mPlay,   0, flags, 2 );
      mSizer->Add( mStop,   0, flags, 2 );
      mSizer->Add( mRewind, 0, flags, 2 );
      mSizer->Add( mFF,     0, flags, 10 );
      mSizer->Add( mRecord, 0, flags, 5 );
   }
   else
   {
      mRewind->MoveBeforeInTabOrder( mFF );
      mPlay->MoveBeforeInTabOrder( mFF );
      mRecord->MoveBeforeInTabOrder( mFF );
      mPause->MoveBeforeInTabOrder( mFF );
      mStop->MoveBeforeInTabOrder( mFF );

      mSizer->Add( mRewind, 0, flags, 2 );
      mSizer->Add( mPlay,   0, flags, 2 );
      mSizer->Add( mRecord, 0, flags, 2 );
      mSizer->Add( mPause,  0, flags, 2 );
      mSizer->Add( mStop,   0, flags, 2 );
      mSizer->Add( mFF,     0, flags, 5 );
   }

   // Layout the sizer
   mSizer->Layout();

   // Layout the toolbar
   Layout();

   // (Re)Establish the minimum size
   SetMinSize( GetSizer()->GetMinSize() );
}

void ControlToolBar::ReCreateButtons()
{
   bool playDown = false;
   bool playShift = false;
   bool pauseDown = false;
   bool recordDown = false;
   bool recordShift = false;

   // ToolBar::ReCreateButtons() will get rid of its sizer and
   // since we've attached our sizer to it, ours will get deleted too
   // so clean ours up first.
   if( mSizer )
   {
      playDown = mPlay->IsDown();
      playShift = mPlay->WasShiftDown();
      pauseDown = mPause->IsDown();
      recordDown = mRecord->IsDown();
      recordShift = mRecord->WasShiftDown();
      Detach( mSizer );

      std::unique_ptr < wxSizer > {mSizer}; // DELETE it
      mSizer = NULL;
   }

   ToolBar::ReCreateButtons();

   if (playDown)
   {
      ControlToolBar::PlayAppearance appearance =
         playShift ? ControlToolBar::PlayAppearance::Looped
         : ControlToolBar::PlayAppearance::Straight;
      SetPlay(playDown, appearance);
   }

   if (pauseDown)
   {
      mPause->PushDown();
   }

   if (recordDown)
   {
      SetRecord(recordDown, recordShift);
   }

   EnableDisableButtons();

   RegenerateTooltips();
}

void ControlToolBar::Repaint( wxDC *dc )
{
#ifndef USE_AQUA_THEME
   wxSize s = mSizer->GetSize();
   wxPoint p = mSizer->GetPosition();

   wxRect bevelRect( p.x, p.y, s.GetWidth() - 1, s.GetHeight() - 1 );
   AColor::Bevel( *dc, true, bevelRect );
#endif
}

void ControlToolBar::EnableDisableButtons()
{
   AudacityProject *p = GetActiveProject();

   bool paused = mPause->IsDown();
   bool playing = mPlay->IsDown();
   bool recording = mRecord->IsDown();
   bool busy = gAudioIO->IsBusy();

   // Only interested in audio type tracks
   bool tracks = p && TrackList::Get( *p ).Any<AudioTrack>(); // PRL:  PlayableTrack ?

   if (p) {
      TranscriptionToolBar *const playAtSpeedTB = p->GetTranscriptionToolBar();
      if (playAtSpeedTB)
         playAtSpeedTB->SetEnabled(
            TransportState::CanStopAudioStream() && tracks && !recording);
   }

   mPlay->SetEnabled(
      TransportState::CanStopAudioStream() && tracks && !recording);
   mRecord->SetEnabled(
      TransportState::CanStopAudioStream() &&
      !(busy && !recording && !paused) &&
      !(playing && !paused)
   );
   mStop->SetEnabled(
      TransportState::CanStopAudioStream() && (playing || recording));
   mRewind->SetEnabled(IsPauseDown() || (!playing && !recording));
   mFF->SetEnabled(tracks && (IsPauseDown() || (!playing && !recording)));

   //auto pProject = GetActiveProject();
   mPause->SetEnabled(
      TransportState::CanStopAudioStream());
}

void ControlToolBar::SetPlay(bool down )
{
   SetPlay( down, PlayAppearance::Straight );
}

void ControlToolBar::SetPlay(bool down, PlayAppearance appearance)
{
   if (down) {
      mPlay->SetShift(appearance == PlayAppearance::Looped);
      mPlay->SetControl(appearance == PlayAppearance::CutPreview);
      mPlay->SetAlternateIdx(static_cast<int>(appearance));
      mPlay->PushDown();
   }
   else {
      mPlay->PopUp();
      mPlay->SetAlternateIdx(0);
   }
   EnableDisableButtons();
   UpdateStatusBar(GetActiveProject());
}

void ControlToolBar::SetStop(bool down)
{
   if (down)
      mStop->PushDown();
   else {
      if(FindFocus() == mStop)
         mPlay->SetFocus();
      mStop->PopUp();
   }
   EnableDisableButtons();
}

void ControlToolBar::SetRecord(bool down, bool altAppearance)
{
   if (down)
   {
      mRecord->SetAlternateIdx(altAppearance ? 1 : 0);
      mRecord->PushDown();
   }
   else
   {
      mRecord->SetAlternateIdx(0);
      mRecord->PopUp();
   }
   EnableDisableButtons();
}

bool ControlToolBar::IsPauseDown() const
{
   return mPause->IsDown();
}

bool ControlToolBar::IsRecordDown() const
{
   return mRecord->IsDown();
}

void ControlToolBar::OnKeyEvent(wxKeyEvent & event)
{
   if (event.ControlDown() || event.AltDown()) {
      event.Skip();
      return;
   }

   // Does not appear to be needed on Linux. Perhaps on some other platform?
   // If so, "!CanStopAudioStream()" should probably apply.
   if (event.GetKeyCode() == WXK_SPACE) {
      if (gAudioIO->IsStreamActive(GetActiveProject()->GetAudioIOToken())) {
         SetPlay(false);
         SetStop(true);
         TransportState::StopPlaying();
      }
      else if (!gAudioIO->IsBusy()) {
         //SetPlay(true);// Not needed as done in PlayPlayRegion
         SetStop(false);
         TransportState::PlayCurrentRegion();
      }
      return;
   }
   event.Skip();
}

void ControlToolBar::OnPlay(wxCommandEvent & WXUNUSED(evt))
{
   auto p = GetActiveProject();

   if (!TransportState::CanStopAudioStream())
      return;

   TransportState::StopPlaying();

   if (p)
      ProjectWindow::Get( *p ).TP_DisplaySelection();

   auto cleanup = finally( [&]{ UpdateStatusBar(p); } );
   PlayDefault();
}

void ControlToolBar::OnStop(wxCommandEvent & WXUNUSED(evt))
{
   if (TransportState::CanStopAudioStream()) {
      TransportState::StopPlaying();
      UpdateStatusBar(GetActiveProject());
   }
}

void ControlToolBar::PlayDefault()
{
   // Let control have precedence over shift
   const bool cutPreview = mPlay->WasControlDown();
   const bool looped = !cutPreview &&
      mPlay->WasShiftDown();
   TransportState::PlayCurrentRegion(looped, cutPreview);
}

void ControlToolBar::OnRecord(wxCommandEvent &evt)
// STRONG-GUARANTEE (for state of current project's tracks)
{
   // TODO: It would be neater if Menu items and Toolbar buttons used the same code for
   // enabling/disabling, and all fell into the same action routines.
   // Here instead we reduplicate some logic (from CommandHandler) because it isn't
   // normally used for buttons.

   // Code from CommandHandler start...
   AudacityProject * p = GetActiveProject();
   wxASSERT(p);
   if (!p)
      return;

   bool altAppearance = mRecord->WasShiftDown();
   if (evt.GetInt() == 1) // used when called by keyboard shortcut. Default (0) ignored.
      altAppearance = true;
   if (evt.GetInt() == 2)
      altAppearance = false;

   bool bPreferNewTrack;
   gPrefs->Read("/GUI/PreferNewTrackRecord", &bPreferNewTrack, false);
   const bool appendRecord = (altAppearance == bPreferNewTrack);

   if (p) {
      const auto &selectedRegion = ViewInfo::Get( *p ).selectedRegion;
      double t0 = selectedRegion.t0();
      double t1 = selectedRegion.t1();
      // When no time selection, recording duration is 'unlimited'.
      if (t1 == t0)
         t1 = DBL_MAX;

      WaveTrackArray existingTracks;

      if (appendRecord) {
         const auto trackRange = TrackList::Get( *p ).Any< const WaveTrack >();

         // Try to find wave tracks to record into.  (If any are selected,
         // try to choose only from them; else if wave tracks exist, may record into any.)
         existingTracks =
            TransportState::ChooseExistingRecordingTracks(*p, true);
         if ( !existingTracks.empty() )
            t0 = std::max( t0,
               ( trackRange + &Track::IsSelected ).max( &Track::GetEndTime ) );
         else {
            existingTracks =
               TransportState::ChooseExistingRecordingTracks(*p, false);
            t0 = std::max( t0, trackRange.max( &Track::GetEndTime ) );
            // If suitable tracks still not found, will record into NEW ones,
            // but the choice of t0 does not depend on that.
         }

         // Whether we decided on NEW tracks or not:
         if (t1 <= selectedRegion.t0() && selectedRegion.t1() > selectedRegion.t0()) {
            t1 = selectedRegion.t1();   // record within the selection
         }
         else {
            t1 = DBL_MAX;        // record for a long, long time
         }
      }

      TransportTracks transportTracks;
      if (UseDuplex()) {
         // Remove recording tracks from the list of tracks for duplex ("overdub")
         // playback.
         /* TODO: set up stereo tracks if that is how the user has set up
          * their preferences, and choose sample format based on prefs */
         transportTracks = GetAllPlaybackTracks(TrackList::Get( *p ), false, true);
         for (const auto &wt : existingTracks) {
            auto end = transportTracks.playbackTracks.end();
            auto it = std::find(transportTracks.playbackTracks.begin(), end, wt);
            if (it != end)
               transportTracks.playbackTracks.erase(it);
         }
      }

      transportTracks.captureTracks = existingTracks;
      auto options = AudioIOStartStreamOptions::PlayDefaults( *p );
      TransportState::DoRecord(*p, transportTracks, t0, t1, altAppearance, options);
   }
}

bool ControlToolBar::UseDuplex()
{
   bool duplex;
   gPrefs->Read(wxT("/AudioIO/Duplex"), &duplex,
#ifdef EXPERIMENTAL_DA
      false
#else
      true
#endif
      );
   return duplex;
}

void ControlToolBar::OnPause(wxCommandEvent & WXUNUSED(evt))
{
   if (!TransportState::CanStopAudioStream()) {
      return;
   }


   if(mPaused)
   {
      mPause->PopUp();
      mPaused=false;
   }
   else
   {
      mPause->PushDown();
      mPaused=true;
   }

#ifdef EXPERIMENTAL_SCRUBBING_SUPPORT

   auto project = GetActiveProject();
   auto &scrubber = Scrubber::Get( *project );

   // Bug 1494 - Pausing a seek or scrub should just STOP as
   // it is confusing to be in a paused scrub state.
   bool bStopInstead = mPaused && 
      gAudioIO->IsScrubbing() && 
      !scrubber.IsSpeedPlaying();

   if (bStopInstead) {
      wxCommandEvent dummy;
      OnStop(dummy);
      return;
   }
   
   if (gAudioIO->IsScrubbing())
      scrubber.Pause(mPaused);
   else
#endif
   {
      gAudioIO->SetPaused(mPaused);
   }

   UpdateStatusBar(GetActiveProject());
}

void ControlToolBar::OnRewind(wxCommandEvent & WXUNUSED(evt))
{
   mRewind->PushDown();
   mRewind->PopUp();

   AudacityProject *p = GetActiveProject();
   if (p) {
      p->StopIfPaused();
      ProjectWindow::Get( *p ).Rewind(mRewind->WasShiftDown());
   }
}

void ControlToolBar::OnFF(wxCommandEvent & WXUNUSED(evt))
{
   mFF->PushDown();
   mFF->PopUp();

   AudacityProject *p = GetActiveProject();

   if (p) {
      p->StopIfPaused();
      ProjectWindow::Get( *p ).SkipEnd(mFF->WasShiftDown());
   }
}

// works out the width of the field in the status bar needed for the state (eg play, record pause)
int ControlToolBar::WidthForStatusBar(wxStatusBar* const sb)
{
   int xMax = 0;
   const auto pauseString = wxT(" ") + wxGetTranslation(mStatePause);

   auto update = [&] (const wxString &state) {
      int x, y;
      sb->GetTextExtent(
         wxGetTranslation(state) + pauseString + wxT("."),
         &x, &y
      );
      xMax = std::max(x, xMax);
   };

   update(mStatePlay);
   update(mStateStop);
   update(mStateRecord);

   // Note that Scrubbing + Paused is not allowed.
   for(const auto &state : Scrubber::GetAllUntranslatedStatusStrings())
      update(state);

   return xMax + 30;    // added constant needed because xMax isn't large enough for some reason, plus some space.
}

wxString ControlToolBar::StateForStatusBar()
{
   wxString state;

   auto pProject = GetActiveProject();
   auto scrubState = pProject
      ? Scrubber::Get( *pProject ).GetUntranslatedStateString()
      : wxString();
   if (!scrubState.empty())
      state = wxGetTranslation(scrubState);
   else if (mPlay->IsDown())
      state = wxGetTranslation(mStatePlay);
   else if (mRecord->IsDown())
      state = wxGetTranslation(mStateRecord);
   else
      state = wxGetTranslation(mStateStop);

   if (mPause->IsDown())
   {
      state.Append(wxT(" "));
      state.Append(wxGetTranslation(mStatePause));
   }

   state.Append(wxT("."));

   return state;
}

void ControlToolBar::UpdateStatusBar(AudacityProject *pProject)
{
   ProjectWindow::Get( *pProject )
      .GetStatusBar()->SetStatusText(StateForStatusBar(), stateStatusBarField);
}
