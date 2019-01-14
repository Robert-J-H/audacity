/**********************************************************************

  Audacity: A Digital Audio Editor

  PlaybackPrefs.h

  Joshua Haberman
  James Crook

**********************************************************************/

#ifndef __AUDACITY_PLAYBACK_PREFS__
#define __AUDACITY_PLAYBACK_PREFS__

#include <wx/defs.h>

#include "PrefsPanel.h"

class ShuttleGui;

class PlaybackPrefs final : public PrefsPanel
{
 public:
   PlaybackPrefs(wxWindow * parent, wxWindowID winid);
   virtual ~PlaybackPrefs();
   bool Commit() override;
   wxString HelpPageName() override;
   void PopulateOrExchange(ShuttleGui & S) override;
   
   static bool GetUnpinnedScrubbingPreference();

 private:
   void Populate();
};


/// A PrefsPanel::Factory that creates one PlaybackPrefs panel.
extern PrefsPanel::Factory PlaybackPrefsFactory;

#endif
