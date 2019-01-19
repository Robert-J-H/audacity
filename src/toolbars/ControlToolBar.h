/**********************************************************************

  Audacity: A Digital Audio Editor


  ControlToolbar.h

  Dominic Mazzoni
  Shane T. Mueller
  Leland Lucius

**********************************************************************/

#ifndef __AUDACITY_CONTROL_TOOLBAR__
#define __AUDACITY_CONTROL_TOOLBAR__

#include "../MemoryX.h"
#include "ToolBar.h"
#include "../Theme.h"

class wxBoxSizer;
class wxCommandEvent;
class wxDC;
class wxKeyEvent;
class wxTimer;
class wxTimerEvent;
class wxWindow;
class wxStatusBar;

class AButton;
class AudacityProject;
class TimeTrack;

struct AudioIOStartStreamOptions;


// In the GUI, ControlToolBar appears as the "Transport Toolbar". "Control Toolbar" is historic.
class ControlToolBar final : public ToolBar {

 public:

   ControlToolBar();
   virtual ~ControlToolBar();

   void Create(wxWindow *parent) override;

   void UpdatePrefs() override;
   void OnKeyEvent(wxKeyEvent & event);

   static bool UseDuplex();

   // msmeyer: These are public, but it's far better to
   // call the "real" interface functions like PlayCurrentRegion() and
   // StopPlaying() which are defined below.
   void OnRewind(wxCommandEvent & evt);
   void OnPlay(wxCommandEvent & evt);
   void OnStop(wxCommandEvent & evt);
   void OnRecord(wxCommandEvent & evt);
   void OnFF(wxCommandEvent & evt);
   void OnPause(wxCommandEvent & evt);

   // Choice among the appearances of the play button:
   enum class PlayAppearance {
      Straight, Looped, CutPreview, Scrub, Seek
   };

   //These allow buttons to be controlled externally:
   void SetPlay(bool down); // default to PlayAppearance::Straight
   void SetPlay(bool down, PlayAppearance appearance);
   void SetStop(bool down);
   void SetRecord(bool down, bool altAppearance = false);

   bool IsPauseDown() const;
   bool IsRecordDown() const;

   void PlayDefault();

   void Populate() override;
   void Repaint(wxDC *dc) override;
   void EnableDisableButtons() override;

   void ReCreateButtons() override;
   void RegenerateTooltips() override;

   int WidthForStatusBar(wxStatusBar* const);
   void UpdateStatusBar(AudacityProject *pProject);

 private:

   static AButton *MakeButton(
      ControlToolBar *pBar,
      teBmps eEnabledUp, teBmps eEnabledDown, teBmps eDisabled,
      int id,
      bool processdownevents,
      const wxChar *label);

   static
   void MakeAlternateImages(AButton &button, int idx,
                            teBmps eEnabledUp,
                            teBmps eEnabledDown,
                            teBmps eDisabled);

   void ArrangeButtons();
   wxString StateForStatusBar();

   enum
   {
      ID_PAUSE_BUTTON = 11000,
      ID_PLAY_BUTTON,
      ID_STOP_BUTTON,
      ID_FF_BUTTON,
      ID_REW_BUTTON,
      ID_RECORD_BUTTON,
      BUTTON_COUNT,
   };

   AButton *mRewind;
   AButton *mPlay;
   AButton *mRecord;
   AButton *mPause;
   AButton *mStop;
   AButton *mFF;

   // Maybe button state values shouldn't be duplicated in this toolbar?
   bool mPaused;         //Play or record is paused or not paused?

   // Activate ergonomic order for transport buttons
   bool mErgonomicTransportButtons;

   wxString mStrLocale; // standard locale abbreviation

   wxBoxSizer *mSizer;

   // strings for status bar
   wxString mStatePlay;
   wxString mStateStop;
   wxString mStateRecord;
   wxString mStatePause;

 public:

   DECLARE_CLASS(ControlToolBar)
   DECLARE_EVENT_TABLE()
};

#endif

