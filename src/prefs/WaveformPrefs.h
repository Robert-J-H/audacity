/**********************************************************************

Audacity: A Digital Audio Editor

WaveformPrefs.h

Paul Licameli

**********************************************************************/


#ifndef __AUDACITY_WAVEFORM_PREFS__
#define __AUDACITY_WAVEFORM_PREFS__

#include "PrefsPanel.h"
#include "WaveformSettings.h"

class ShuttleGui;
class WaveTrack;
class wxCheckBox;
class wxChoice;

class wxArrayStringEx;

class WaveformPrefs final : public PrefsPanel
{
public:
   WaveformPrefs(wxWindow * parent, wxWindowID winid, WaveTrack *wt);
   virtual ~WaveformPrefs();
   bool Commit() override;
   bool ShowsPreviewButton() override;
   bool Validate() override;
   void PopulateOrExchange(ShuttleGui & S) override;

private:
   void Populate();

   void OnControl(wxCommandEvent&);
   void OnScale(wxCommandEvent&);
   void OnDefaults(wxCommandEvent&);
   DECLARE_EVENT_TABLE()

   void EnableDisableRange();

   WaveTrack *const mWt;
   bool mDefaulted;

   wxCheckBox *mDefaultsCheckbox;
   wxChoice *mScaleChoice;
   wxChoice *mRangeChoice;

   wxArrayStringEx mRangeCodes;
   wxArrayStringEx mRangeChoices;

   WaveformSettings mTempSettings;

   bool mPopulating;
};

/// A PrefsPanel::Factory that creates one WaveformPrefs panel.
/// This factory can be parametrized by a single track, to change settings
/// non-globally
extern PrefsPanel::Factory WaveformPrefsFactory(WaveTrack *wt);
#endif
