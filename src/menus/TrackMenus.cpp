#include "../Audacity.h"
#include "../Experimental.h"

#include "../AudacityApp.h"
#include "../LabelTrack.h"
#include "../Menus.h"
#include "../Mix.h"

#include "../Prefs.h"
#include "../Project.h"
#include "../PluginManager.h"
#include "../ShuttleGui.h"
#include "../TimeTrack.h"
#include "../TrackPanel.h"
#include "../UndoManager.h"
#include "../WaveClip.h"
#include "../ViewInfo.h"
#include "../WaveTrack.h"
#include "../commands/CommandContext.h"
#include "../commands/CommandManager.h"
#include "../effects/EffectManager.h"
#include "../widgets/ASlider.h"

#include <wx/combobox.h>

#ifdef EXPERIMENTAL_SCOREALIGN
#include "../effects/ScoreAlignDialog.h"
#include "audioreader.h"
#include "scorealign.h"
#include "scorealign-glue.h"
#endif /* EXPERIMENTAL_SCOREALIGN */

// private helper classes and functions
namespace {

void DoMixAndRender
(AudacityProject &project, bool toNewTrack)
{
   auto &tracks = TrackList::Get( project );
   auto &trackFactory = TrackFactory::Get( project );
   auto rate = project.GetRate();
   auto defaultFormat = project.GetDefaultFormat();
   auto &trackPanel = TrackPanel::Get( project );
   auto &window = ProjectWindow::Get( project );

   wxGetApp().SetMissingAliasedFileWarningShouldShow(true);

   WaveTrack::Holder uNewLeft, uNewRight;
   ::MixAndRender(
      &tracks, &trackFactory, rate, defaultFormat, 0.0, 0.0, uNewLeft, uNewRight);

   if (uNewLeft) {
      // Remove originals, get stats on what tracks were mixed

      auto trackRange = tracks.Selected< WaveTrack >();
      auto selectedCount = (trackRange + &Track::IsLeader).size();
      wxString firstName;
      if (selectedCount > 0)
         firstName = (*trackRange.begin())->GetGroupData().GetName();
      if (!toNewTrack)  {
         // Beware iterator invalidation!
         for (auto &it = trackRange.first, &end = trackRange.second; it != end;)
            tracks.Remove( *it++ );
      }

      // Add NEW tracks

      auto pNewLeft = tracks.Add( uNewLeft, true );
      decltype(pNewLeft) pNewRight{};
      if (uNewRight)
         pNewRight = tracks.Add( uNewRight, false );

      // If we're just rendering (not mixing), keep the track name the same
      if (selectedCount==1)
         pNewLeft->GetGroupData().SetName(firstName);

      // Smart history/undo message
      if (selectedCount==1) {
         wxString msg;
         msg.Printf(_("Rendered all audio in track '%s'"), firstName);
         /* i18n-hint: Convert the audio into a more usable form, so apply
          * panning and amplification and write to some external file.*/
         project.PushState(msg, _("Render"));
      }
      else {
         wxString msg;
         if (pNewRight)
            msg.Printf(
               _("Mixed and rendered %d tracks into one new stereo track"),
               (int)selectedCount);
         else
            msg.Printf(
               _("Mixed and rendered %d tracks into one new mono track"),
               (int)selectedCount);
         project.PushState(msg, _("Mix and Render"));
      }

      trackPanel.SetFocus();
      trackPanel.SetFocusedTrack(pNewLeft);
      trackPanel.EnsureVisible(pNewLeft);
      window.RedrawProject();
   }
}

void DoPanTracks(AudacityProject &project, float PanValue)
{
   auto &tracks = TrackList::Get( project );
   auto &window = ProjectWindow::Get( project );

   // count selected wave tracks
   const auto range = tracks.Any< WaveTrack >();
   const auto selectedRange = range + &Track::IsSelected;
   auto count = selectedRange.size();

   // iter through them, all if none selected.
   for (auto group : (count == 0 ? range : selectedRange).ByGroups() )
      group.data->SetPan( PanValue );

   window.RedrawProject();

   auto flags = UndoPush::AUTOSAVE;
   /*i18n-hint: One or more audio tracks have been panned*/
   project.PushState(_("Panned audio track(s)"), _("Pan Track"), flags);
         flags = flags | UndoPush::CONSOLIDATE;
}

enum {
   kAlignStartZero = 0,
   kAlignStartSelStart,
   kAlignStartSelEnd,
   kAlignEndSelStart,
   kAlignEndSelEnd,
   // The next two are only in one subMenu, so more easily handled at the end.
   kAlignEndToEnd,
   kAlignTogether
};

static const std::initializer_list< ComponentInterfaceSymbol > alignLabels{
   { wxT("StartToZero"),     XO("Start to &Zero") },
   { wxT("StartToSelStart"), XO("Start to &Cursor/Selection Start") },
   { wxT("StartToSelEnd"),   XO("Start to Selection &End") },
   { wxT("EndToSelStart"),   XO("End to Cu&rsor/Selection Start") },
   { wxT("EndToSelEnd"),     XO("End to Selection En&d") },
};

const size_t kAlignLabelsCount = alignLabels.end() - alignLabels.begin();

void DoAlign
(AudacityProject &project, int index, bool moveSel)
{
   auto &tracks = TrackList::Get( project );
   auto &selectedRegion = ViewInfo::Get( project ).selectedRegion;
   auto &window = ProjectWindow::Get( project );

   wxString action;
   wxString shortAction;
   double delta = 0.0;
   double newPos = -1.0;

   auto channelRange = tracks.Selected< AudioTrack >();
   auto trackRange = tracks.SelectedLeaders< AudioTrack >();

   auto FindOffset = []( const Track *pTrack ) {
      return TrackList::Channels(pTrack).min( &Track::GetOffset ); };

   auto firstTrackOffset = [&]{ return FindOffset( *trackRange.begin() ); };
   auto minOffset = [&]{ return trackRange.min( FindOffset ); };
   auto avgOffset = [&]{
      return trackRange.sum( FindOffset ) /
                             std::max( size_t(1), trackRange.size() ); };

   auto maxEndOffset = [&]{
      return std::max(0.0, channelRange.max( &Track::GetEndTime ) ); };

   switch(index) {
   case kAlignStartZero:
      delta = -minOffset();
      action = moveSel
         /* i18n-hint: In this and similar messages describing editing actions,
            the starting or ending points of tracks are re-"aligned" to other
            times, and the time selection may be "moved" too.  The first
            noun -- "start" in this example -- is the object of a verb (not of
            an implied preposition "from"). */
         ? _("Aligned/Moved start to zero")
         : _("Aligned start to zero");
         /* i18n-hint: This and similar messages give shorter descriptions of
            the aligning and moving editing actions */
      shortAction = moveSel
         ? _("Align/Move Start")
         : _("Align Start");
      break;
   case kAlignStartSelStart:
      delta = selectedRegion.t0() - minOffset();
      action = moveSel
         ? _("Aligned/Moved start to cursor/selection start")
         : _("Aligned start to cursor/selection start");
      shortAction = moveSel
         ? _("Align/Move Start")
         : _("Align Start");
      break;
   case kAlignStartSelEnd:
      delta = selectedRegion.t1() - minOffset();
      action = moveSel
         ? _("Aligned/Moved start to selection end")
         : _("Aligned start to selection end");
      shortAction = moveSel
         ? _("Align/Move Start")
         : _("Align Start");
      break;
   case kAlignEndSelStart:
      delta = selectedRegion.t0() - maxEndOffset();
      action = moveSel
         ? _("Aligned/Moved end to cursor/selection start")
         : _("Aligned end to cursor/selection start");
      shortAction =
         moveSel
         ? _("Align/Move End")
         : _("Align End");
      break;
   case kAlignEndSelEnd:
      delta = selectedRegion.t1() - maxEndOffset();
      action = moveSel
         ? _("Aligned/Moved end to selection end")
         : _("Aligned end to selection end");
      shortAction =
         moveSel
         ? _("Align/Move End")
         : _("Align End");
      break;
   // index set in alignLabelsNoSync
   case kAlignEndToEnd:
      newPos = firstTrackOffset();
      action = moveSel
         ? _("Aligned/Moved end to end")
         : _("Aligned end to end");
      shortAction =
         moveSel
         ? _("Align/Move End to End")
         : _("Align End to End");
      break;
   case kAlignTogether:
      newPos = avgOffset();
      action = moveSel
         ? _("Aligned/Moved together")
         : _("Aligned together");
      shortAction =
         moveSel
         ? _("Align/Move Together")
         : _("Align Together");
   }

   if ((unsigned)index >= kAlignLabelsCount) {
      // This is an alignLabelsNoSync command.
      for (auto t : tracks.SelectedLeaders< AudioTrack >()) {
         // This shifts different tracks in different ways, so no sync-lock
         // move.
         // Only align Wave and Note tracks end to end.
         auto channels = TrackList::Channels(t);

         auto trackStart = channels.min( &Track::GetStartTime );
         auto trackEnd = channels.max( &Track::GetEndTime );

         for (auto channel : channels)
            // Move the track
            channel->SetOffset(newPos + channel->GetStartTime() - trackStart);

         if (index == kAlignEndToEnd)
            newPos += (trackEnd - trackStart);
      }
      if (index == kAlignEndToEnd) {
         ViewActions::DoZoomFit(project);
      }
   }

   if (delta != 0.0) {
      // For a fixed-distance shift move sync-lock selected tracks also.
      for (auto t : tracks.Any() + &Track::IsSelectedOrSyncLockSelected )
         t->SetOffset(t->GetOffset() + delta);
   }

   if (moveSel)
      selectedRegion.move(delta);

   project.PushState(action, shortAction);

   window.RedrawProject();
}

#ifdef EXPERIMENTAL_SCOREALIGN

// rough relative amount of time to compute one
//    frame of audio or midi, or one cell of matrix, or one iteration
//    of smoothing, measured on a 1.9GHz Core 2 Duo in 32-bit mode
//    (see COLLECT_TIMING_DATA below)
#define AUDIO_WORK_UNIT 0.004F
#define MIDI_WORK_UNIT 0.0001F
#define MATRIX_WORK_UNIT 0.000002F
#define SMOOTHING_WORK_UNIT 0.000001F

// Write timing data to a file; useful for calibrating AUDIO_WORK_UNIT,
// MIDI_WORK_UNIT, MATRIX_WORK_UNIT, and SMOOTHING_WORK_UNIT coefficients
// Data is written to timing-data.txt; look in
//     audacity-src/win/Release/modules/
#define COLLECT_TIMING_DATA

// Audacity Score Align Progress class -- progress reports come here
class ASAProgress final : public SAProgress {
 private:
   float mTotalWork;
   float mFrames[2];
   long mTotalCells; // how many matrix cells?
   long mCellCount; // how many cells so far?
   long mPrevCellCount; // cell_count last reported with Update()
   Maybe<ProgressDialog> mProgress;
   #ifdef COLLECT_TIMING_DATA
      FILE *mTimeFile;
      wxDateTime mStartTime;
      long iterations;
   #endif

 public:
   ASAProgress() {
      smoothing = false;
      #ifdef COLLECT_TIMING_DATA
         mTimeFile = fopen("timing-data.txt", "w");
      #endif
   }
   ~ASAProgress() {
      #ifdef COLLECT_TIMING_DATA
         fclose(mTimeFile);
      #endif
   }
   void set_phase(int i) override {
      float work[2]; // chromagram computation work estimates
      float work2, work3 = 0; // matrix and smoothing work estimates
      SAProgress::set_phase(i);
      #ifdef COLLECT_TIMING_DATA
         long ms = 0;
         wxDateTime now = wxDateTime::UNow();
         wxFprintf(mTimeFile, "Phase %d begins at %s\n",
                 i, now.FormatTime());
         if (i != 0)
            ms = now.Subtract(mStartTime).GetMilliseconds().ToLong();
         mStartTime = now;
      #endif
      if (i == 0) {
         mCellCount = 0;
         for (int j = 0; j < 2; j++) {
            mFrames[j] = durations[j] / frame_period;
         }
         mTotalWork = 0;
         for (int j = 0; j < 2; j++) {
             work[j] =
                (is_audio[j] ? AUDIO_WORK_UNIT : MIDI_WORK_UNIT) * mFrames[j];
             mTotalWork += work[j];
         }
         mTotalCells = mFrames[0] * mFrames[1];
         work2 = mTotalCells * MATRIX_WORK_UNIT;
         mTotalWork += work2;
         // arbitarily assume 60 iterations to fit smooth segments and
         // per frame per iteration is SMOOTHING_WORK_UNIT
         if (smoothing) {
            work3 =
               wxMax(mFrames[0], mFrames[1]) * SMOOTHING_WORK_UNIT * 40;
            mTotalWork += work3;
         }
         #ifdef COLLECT_TIMING_DATA
            wxFprintf(mTimeFile,
               " mTotalWork (an estimate) = %g\n", mTotalWork);
            wxFprintf(mTimeFile, " work0 = %g, frames %g, is_audio %d\n",
                    work[0], mFrames[0], is_audio[0]);
            wxFprintf(mTimeFile, " work1 = %g, frames %g, is_audio %d\n",
                    work[1], mFrames[1], is_audio[1]);
            wxFprintf(mTimeFile, "work2 = %g, work3 = %g\n", work2, work3);
         #endif
         mProgress.create(_("Synchronize MIDI with Audio"),
                               _("Synchronizing MIDI and Audio Tracks"));
      } else if (i < 3) {
         wxFprintf(mTimeFile,
                "Phase %d took %d ms for %g frames, coefficient = %g s/frame\n",
               i - 1, ms, mFrames[i - 1], (ms * 0.001) / mFrames[i - 1]);
      } else if (i == 3) {
        wxFprintf(mTimeFile,
                "Phase 2 took %d ms for %d cells, coefficient = %g s/cell\n",
                ms, mCellCount, (ms * 0.001) / mCellCount);
      } else if (i == 4) {
        wxFprintf(mTimeFile,
                "Phase 3 took %d ms for %d iterations on %g frames, "
                "coefficient = %g s per frame per iteration\n",
                ms, iterations, wxMax(mFrames[0], mFrames[1]),
                (ms * 0.001) / (wxMax(mFrames[0], mFrames[1]) * iterations));
      }
   }
   bool set_feature_progress(float s) override {
      float work;
      if (phase == 0) {
         float f = s / frame_period;
         work = (is_audio[0] ? AUDIO_WORK_UNIT : MIDI_WORK_UNIT) * f;
      } else if (phase == 1) {
         float f = s / frame_period;
         work = (is_audio[0] ? AUDIO_WORK_UNIT : MIDI_WORK_UNIT) * mFrames[0] +
                (is_audio[1] ? AUDIO_WORK_UNIT : MIDI_WORK_UNIT) * f;
      }
      auto updateResult = mProgress->Update((int)(work), (int)(mTotalWork));
      return (updateResult == ProgressResult::Success);
   }
   bool set_matrix_progress(int cells) override {
      mCellCount += cells;
      float work =
             (is_audio[0] ? AUDIO_WORK_UNIT : MIDI_WORK_UNIT) * mFrames[0] +
             (is_audio[1] ? AUDIO_WORK_UNIT : MIDI_WORK_UNIT) * mFrames[1];
      work += mCellCount * MATRIX_WORK_UNIT;
      auto updateResult = mProgress->Update((int)(work), (int)(mTotalWork));
      return (updateResult == ProgressResult::Success);
   }
   bool set_smoothing_progress(int i) override {
      iterations = i;
      float work =
             (is_audio[0] ? AUDIO_WORK_UNIT : MIDI_WORK_UNIT) * mFrames[0] +
             (is_audio[1] ? AUDIO_WORK_UNIT : MIDI_WORK_UNIT) * mFrames[1] +
             MATRIX_WORK_UNIT * mFrames[0] * mFrames[1];
      work += i * wxMax(mFrames[0], mFrames[1]) * SMOOTHING_WORK_UNIT;
      auto updateResult = mProgress->Update((int)(work), (int)(mTotalWork));
      return (updateResult == ProgressResult::Success);
   }
};


long mixer_process(void *mixer, float **buffer, long n)
{
   Mixer *mix = (Mixer *) mixer;
   long frame_count = mix->Process(std::max(0L, n));
   *buffer = (float *) mix->GetBuffer();
   return frame_count;
}

#endif // EXPERIMENTAL_SCOREALIGN

//sort based on flags.  see Project.h for sort flags
void DoSortTracks( AudacityProject &project, int flags )
{
   auto GetTime = [](const Track *t) {
      return t->TypeSwitch< double >(
         [&](const WaveTrack* w) {
            auto stime = w->GetEndTime();

            int ndx;
            for (ndx = 0; ndx < w->GetNumClips(); ndx++) {
               const auto c = w->GetClipByIndex(ndx);
               if (c->GetNumSamples() == 0)
                  continue;
               stime = std::min(stime, c->GetStartTime());
            }
            return stime;
         },
         [&](const LabelTrack* l) {
            return l->GetStartTime();
         }
      );
   };

   size_t ndx = 0;
   // This one place outside of TrackList where we must use undisguised
   // std::list iterators!  Avoid this elsewhere!
   std::vector<TrackNodePointer> arr;
   auto &tracks = TrackList::Get( project );
   arr.reserve(tracks.size());

   // First find the permutation.
   // This routine, very unusually, deals with the underlying stl list
   // iterators, not with TrackIter!  Dangerous!
   for (auto iter = tracks.ListOfTracks::begin(),
        end = tracks.ListOfTracks::end(); iter != end; ++iter) {
      const auto &track = *iter;
      if ( !track->IsLeader() )
         // keep channels contiguous
         ndx++;
      else {
         auto size = arr.size();
         for (ndx = 0; ndx < size;) {
            Track &arrTrack = **arr[ndx].first;
            auto channels = TrackList::Channels(&arrTrack);
            if(flags & kAudacitySortByName) {
               //do case insensitive sort - cmpNoCase returns less than zero if
               // the string is 'less than' its argument
               //also if we have case insensitive equality, then we need to sort
               // by case as well
               //We sort 'b' before 'B' accordingly.  We uncharacteristically
               // use greater than for the case sensitive
               //compare because 'b' is greater than 'B' in ascii.
               const auto &trackName = track->GetGroupData().GetName();
               const auto &arrName = arrTrack.GetGroupData().GetName();
               auto cmpValue = trackName.CmpNoCase(arrName);
               if ( cmpValue < 0 ||
                     ( 0 == cmpValue &&
                        trackName.CompareTo(arrName) > 0 ) )
                  break;
            }
            //sort by time otherwise
            else if(flags & kAudacitySortByTime) {
               auto time1 = TrackList::Channels(track.get()).min( GetTime );

               //get candidate's (from sorted array) time
               auto time2 = channels.min( GetTime );

               if (time1 < time2)
                  break;
            }
            ndx += channels.size();
         }
      }
      arr.insert(arr.begin() + ndx, TrackNodePointer{iter, &tracks});
   }

   // Now apply the permutation
   tracks.Permute(arr);
}

void SetTrackGain(AudacityProject &project, WaveTrack * wt, LWSlider * slider)
{
   wxASSERT(wt);
   float newValue = slider->Get();

   wt->GetGroupData().SetGain(newValue);

   project.PushState(_("Adjusted gain"), _("Gain"), UndoPush::CONSOLIDATE);

   TrackPanel::Get( project ).RefreshTrack(wt);
}

void SetTrackPan(AudacityProject &project, WaveTrack * wt, LWSlider * slider)
{
   wxASSERT(wt);
   float newValue = slider->Get();

   wt->GetGroupData().SetPan(newValue);

   project.PushState(_("Adjusted Pan"), _("Pan"), UndoPush::CONSOLIDATE);

   TrackPanel::Get( project ).RefreshTrack(wt);
}

}

namespace TrackActions {

// exported helper functions

void DoRemoveTracks( AudacityProject &project )
{
   auto &tracks = TrackList::Get( project );
   auto &trackPanel = TrackPanel::Get( project );

   std::vector<Track*> toRemove;
   for (auto track : tracks.Selected())
      toRemove.push_back(track);

   // Capture the track preceding the first removed track
   Track *f{};
   if (!toRemove.empty()) {
      auto found = tracks.Find(toRemove[0]);
      f = *--found;
   }

   for (auto track : toRemove)
      tracks.Remove(track);

   if (!f)
      // try to use the last track
      f = *tracks.Any().rbegin();
   if (f) {
      // Try to use the first track after the removal
      auto found = tracks.FindLeader(f);
      auto t = *++found;
      if (t)
         f = t;
   }

   // If we actually have something left, then make sure it's seen
   if (f)
      trackPanel.EnsureVisible(f);

   project.PushState(_("Removed audio track(s)"), _("Remove Track"));

   trackPanel.UpdateViewIfNoTracks();
   trackPanel.Refresh(false);
}

void DoTrackMute(AudacityProject &project, Track *t, bool exclusive)
{
   auto &tracks = TrackList::Get( project );
   auto &trackPanel = TrackPanel::Get( project );

   // Whatever t is, replace with lead channel
   t = *tracks.FindLeader(t);

   // "exclusive" mute means mute the chosen track and unmute all others.
   if (exclusive) {
      for (auto group : tracks.Any<PlayableTrack>().ByGroups()) {
         bool chosen = (t == group.Leader());
         group.data->SetMute( chosen ),
         group.data->SetSolo( false );
      }
   }
   else {
      // Normal click toggles this track.
      auto pt = dynamic_cast<PlayableTrack *>( t );
      if (!pt)
         return;

      auto &groupData = pt->GetGroupData();
      bool wasMute = groupData.GetMute();
      groupData.SetMute( !wasMute );

      if (project.IsSoloSimple() || project.IsSoloNone())
      {
         // We also set a solo indicator if we have just one track / stereo pair playing.
         // in a group of more than one playable tracks.
         // otherwise clear solo on everything.

         auto range = tracks.Leaders<PlayableTrack>();
         auto nPlayableTracks = range.size();
         auto nPlaying = (range - &PlayableTrack::GetMute).size();

         for (auto group : tracks.Any<PlayableTrack>().ByGroups())
            group.data->SetSolo(
               (nPlaying==1) && (nPlayableTracks > 1 ) &&
               !group.data->GetMute()
            );
      }
   }
   project.ModifyState(true);

   trackPanel.UpdateAccessibility();
   trackPanel.Refresh(false);
}

void DoTrackSolo(AudacityProject &project, Track *t, bool exclusive)
{
   auto &tracks = TrackList::Get( project );
   auto &trackPanel = TrackPanel::Get( project );
   
   // Whatever t is, replace with lead channel
   t = *tracks.FindLeader(t);

   const auto pt = dynamic_cast<PlayableTrack *>( t );
   if (!pt)
      return;
   bool bWasSolo = pt->GetSolo();

   bool bSoloMultiple = !project.IsSoloSimple() ^ exclusive;

   // Standard and Simple solo have opposite defaults:
   //   Standard - Behaves as individual buttons, shift=radio buttons
   //   Simple   - Behaves as radio buttons, shift=individual
   // In addition, Simple solo will mute/unmute tracks
   // when in standard radio button mode.
   if ( bSoloMultiple )
      pt->GetGroupData().SetSolo( !bWasSolo );
   else
   {
      // Normal click solo this track only, mute everything else.
      // OR unmute and unsolo everything.
      for (auto group : tracks.Any<PlayableTrack>().ByGroups()) {
         bool chosen = (t == group.Leader());
         auto data = group.data;
         if (chosen) {
            data->SetSolo( !bWasSolo );
            if( project.IsSoloSimple() )
               data->SetMute( false );
         }
         else {
            data->SetSolo( false );
            if( project.IsSoloSimple() )
               data->SetMute( !bWasSolo );
         }
      }
   }
   project.ModifyState(true);

   trackPanel.UpdateAccessibility();
   trackPanel.Refresh(false);
}

void DoRemoveTrack(AudacityProject &project, Track * toRemove)
{
   auto &tracks = TrackList::Get( project );
   auto &trackPanel = TrackPanel::Get( project );
   auto &window = ProjectWindow::Get( project );

   // If it was focused, then NEW focus is the next or, if
   // unavailable, the previous track. (The NEW focus is set
   // after the track has been removed.)
   bool toRemoveWasFocused = trackPanel.GetFocusedTrack() == toRemove;
   Track* newFocus{};
   if (toRemoveWasFocused) {
      auto iterNext = tracks.FindLeader(toRemove), iterPrev = iterNext;
      newFocus = *++iterNext;
      if (!newFocus) {
         newFocus = *--iterPrev;
      }
   }

   wxString name = toRemove->GetGroupData().GetName();

   auto channels = TrackList::Channels(toRemove);
   // Be careful to post-increment over positions that get erased!
   auto &iter = channels.first;
   while (iter != channels.end())
      tracks.Remove( * iter++ );

   if (toRemoveWasFocused)
      trackPanel.SetFocusedTrack(newFocus);

   project.PushState(
      wxString::Format(_("Removed track '%s.'"),
      name),
      _("Track Remove"));

   window.FixScrollbars();
   window.HandleResize();
   trackPanel.Refresh(false);
}

void DoMoveTrack
(AudacityProject &project, Track* target, MoveChoice choice)
{
   auto &trackPanel = TrackPanel::Get( project );
   auto &tracks = TrackList::Get( project );

   wxString longDesc, shortDesc;

   switch (choice)
   {
   case OnMoveTopID:
      /* i18n-hint: Past tense of 'to move', as in 'moved audio track up'.*/
      longDesc = _("Moved '%s' to Top");
      shortDesc = _("Move Track to Top");

      // TODO: write TrackList::Rotate to do this in one step and avoid emitting
      // an event for each swap
      while (tracks.CanMoveUp(target))
         tracks.Move(target, true);

      break;
   case OnMoveBottomID:
      /* i18n-hint: Past tense of 'to move', as in 'moved audio track up'.*/
      longDesc = _("Moved '%s' to Bottom");
      shortDesc = _("Move Track to Bottom");

      // TODO: write TrackList::Rotate to do this in one step and avoid emitting
      // an event for each swap
      while (tracks.CanMoveDown(target))
         tracks.Move(target, false);

      break;
   default:
      bool bUp = (OnMoveUpID == choice);

      tracks.Move(target, bUp);
      longDesc =
         /* i18n-hint: Past tense of 'to move', as in 'moved audio track up'.*/
         bUp? _("Moved '%s' Up")
         : _("Moved '%s' Down");
      shortDesc =
         /* i18n-hint: Past tense of 'to move', as in 'moved audio track up'.*/
         bUp? _("Move Track Up")
         : _("Move Track Down");

   }

   longDesc = longDesc.Format(target->GetGroupData().GetName());

   project.PushState(longDesc, shortDesc);
   trackPanel.Refresh(false);
}

// Menu handler functions

struct Handler : CommandHandlerObject {

void OnNewWaveTrack(const CommandContext &context)
{
   auto &project = context.project;
   auto &tracks = TrackList::Get( project );
   auto &trackFactory = TrackFactory::Get( project );
   auto &trackPanel = TrackPanel::Get( project );
   auto &window = ProjectWindow::Get( project );
   
   auto defaultFormat = project.GetDefaultFormat();
   auto rate = project.GetRate();

   auto t = tracks.Add( trackFactory.NewWaveTrack( defaultFormat, rate ), true );
   project.SelectNone();

   t->GetGroupData().SetSelected(true);

   project.PushState(_("Created new audio track"), _("New Track"));

   window.RedrawProject();
   trackPanel.EnsureVisible(t);
}

void OnNewStereoTrack(const CommandContext &context)
{
   auto &project = context.project;
   auto &tracks = TrackList::Get( project );
   auto &trackFactory = TrackFactory::Get( project );
   auto &trackPanel = TrackPanel::Get( project );
   auto &window = ProjectWindow::Get( project );

   auto defaultFormat = project.GetDefaultFormat();
   auto rate = project.GetRate();

   project.SelectNone();

   auto left =
      tracks.Add( trackFactory.NewWaveTrack( defaultFormat, rate ), true );

   auto right =
      tracks.Add( trackFactory.NewWaveTrack( defaultFormat, rate ), false );


   project.PushState(_("Created new stereo audio track"), _("New Track"));

   window.RedrawProject();
   trackPanel.EnsureVisible(left);
}

void OnNewLabelTrack(const CommandContext &context)
{
   auto &project = context.project;
   auto &tracks = TrackList::Get( project );
   auto &trackFactory = TrackFactory::Get( project );
   auto &trackPanel = TrackPanel::Get( project );
   auto &window = ProjectWindow::Get( project );

   auto t = tracks.Add( trackFactory.NewLabelTrack(), true );

   project.SelectNone();

   t->GetGroupData().SetSelected(true);

   project.PushState(_("Created new label track"), _("New Track"));

   window.RedrawProject();
   trackPanel.EnsureVisible(t);
}

void OnNewTimeTrack(const CommandContext &context)
{
   auto &project = context.project;
   auto &tracks = TrackList::Get( project );
   auto &trackFactory = TrackFactory::Get( project );
   auto &trackPanel = TrackPanel::Get( project );
   auto &window = ProjectWindow::Get( project );

   if (tracks.GetTimeTrack()) {
      AudacityMessageBox(_("This version of Audacity only allows one time track for each project window."));
      return;
   }

   auto t = tracks.AddToHead( trackFactory.NewTimeTrack() );

   project.SelectNone();

   t->GetGroupData().SetSelected(true);

   project.PushState(_("Created new time track"), _("New Track"));

   window.RedrawProject();
   trackPanel.EnsureVisible(t);
}

void OnStereoToMono(const CommandContext &context)
{
   PluginActions::DoEffect(
      EffectManager::Get().GetEffectByIdentifier(wxT("StereoToMono")),
      context,
      PluginActions::kConfigured);
}

void OnMixAndRender(const CommandContext &context)
{
   auto &project = context.project;
   DoMixAndRender(project, false);
}

void OnMixAndRenderToNewTrack(const CommandContext &context)
{
   auto &project = context.project;
   DoMixAndRender(project, true);
}

void OnResample(const CommandContext &context)
{
   auto &project = context.project;
   auto projectRate = project.GetRate();
   auto &tracks = TrackList::Get( project );
   auto &undoManager = UndoManager::Get( project );
   auto &window = ProjectWindow::Get( project );

   int newRate;

   while (true)
   {
      auto &window = ProjectWindow::Get( project );
      wxDialogWrapper dlg(&window, wxID_ANY, wxString(_("Resample")));
      dlg.SetName(dlg.GetTitle());
      ShuttleGui S(&dlg, eIsCreating);
      wxString rate;
      wxComboBox *cb;

      rate.Printf(wxT("%ld"), lrint(projectRate));

      wxArrayStringEx rates{
         wxT("8000") ,
         wxT("11025") ,
         wxT("16000") ,
         wxT("22050") ,
         wxT("32000") ,
         wxT("44100") ,
         wxT("48000") ,
         wxT("88200") ,
         wxT("96000") ,
         wxT("176400") ,
         wxT("192000") ,
         wxT("352800") ,
         wxT("384000") ,
      };

      S.StartVerticalLay(true);
      {
         S.AddSpace(-1, 15);

         S.StartHorizontalLay(wxCENTER, false);
         {
            cb = S.AddCombo(_("New sample rate (Hz):"),
                            rate,
                            rates);
         }
         S.EndHorizontalLay();

         S.AddSpace(-1, 15);

         S.AddStandardButtons();
      }
      S.EndVerticalLay();

      dlg.Layout();
      dlg.Fit();
      dlg.Center();

      if (dlg.ShowModal() != wxID_OK)
      {
         return;  // user cancelled dialog
      }

      long lrate;
      if (cb->GetValue().ToLong(&lrate) && lrate >= 1 && lrate <= 1000000)
      {
         newRate = (int)lrate;
         break;
      }

      AudacityMessageBox(_("The entered value is invalid"), _("Error"),
                   wxICON_ERROR, &window);
   }

   int ndx = 0;
   auto flags = UndoPush::AUTOSAVE;
   for (auto wt : tracks.Selected< WaveTrack >())
   {
      wxString msg;

      msg.Printf(_("Resampling track %d"), ++ndx);

      ProgressDialog progress(_("Resample"), msg);

      // The resampling of a track may be stopped by the user.  This might
      // leave a track with multiple clips in a partially resampled state.
      // But the thrown exception will cause rollback in the application
      // level handler.

       wt->Resample(newRate, &progress);

      // Each time a track is successfully, completely resampled,
      // commit that to the undo stack.  The second and later times,
      // consolidate.

      project.PushState(
         _("Resampled audio track(s)"), _("Resample Track"), flags);
      flags = flags | UndoPush::CONSOLIDATE;
   }

   undoManager.StopConsolidating();
   window.RedrawProject();

   // Need to reset
   window.FinishAutoScroll();
}

void OnRemoveTracks(const CommandContext &context)
{
   DoRemoveTracks( context.project );
}

void OnMuteAllTracks(const CommandContext &context)
{
   auto &project = context.project;
   auto &tracks = TrackList::Get( project );
   auto soloSimple = project.IsSoloSimple();
   auto soloNone = project.IsSoloNone();
   auto &window = ProjectWindow::Get( project );

   for (auto group : tracks.Any<PlayableTrack>().ByGroups())
   {
      group.data->SetMute(true);
      if (soloSimple || soloNone)
         group.data->SetSolo(false);
   }

   project.ModifyState(true);
   window.RedrawProject();
}

void OnUnmuteAllTracks(const CommandContext &context)
{
   auto &project = context.project;
   auto &tracks = TrackList::Get( project );
   auto &window = ProjectWindow::Get( project );

   auto soloSimple = project.IsSoloSimple();
   auto soloNone = project.IsSoloNone();

   for (auto group : tracks.Any<PlayableTrack>().ByGroups())
   {
      group.data->SetMute(false);
      if (soloSimple || soloNone)
         group.data->SetSolo(false);
   }

   project.ModifyState(true);
   window.RedrawProject();
}

void OnPanLeft(const CommandContext &context)
{
   auto &project = context.project;
   DoPanTracks( project, -1.0);
}

void OnPanRight(const CommandContext &context)
{
   auto &project = context.project;
   DoPanTracks( project, 1.0);
}

void OnPanCenter(const CommandContext &context)
{
   auto &project = context.project;
   DoPanTracks( project, 0.0);
}

void OnAlignNoSync(const CommandContext &context)
{
   auto &project = context.project;

   DoAlign(project,
      context.index + kAlignLabelsCount, false);
}

void OnAlign(const CommandContext &context)
{
   auto &project = context.project;

   bool bMoveWith;
   gPrefs->Read(wxT("/GUI/MoveSelectionWithTracks"), &bMoveWith, false);
   DoAlign(project, context.index, bMoveWith);
}

/*
// Now handled in OnAlign.
void OnAlignMoveSel(int index)
{
   DoAlign(index, true);
}
*/

void OnMoveSelectionWithTracks(const CommandContext &WXUNUSED(context) )
{
   bool bMoveWith;
   gPrefs->Read(wxT("/GUI/MoveSelectionWithTracks"), &bMoveWith, false);
   gPrefs->Write(wxT("/GUI/MoveSelectionWithTracks"), !bMoveWith);
   gPrefs->Flush();

}

#ifdef EXPERIMENTAL_SCOREALIGN
void OnScoreAlign(const CommandContext &context)
{
   auto &project = context.project;
   auto tracks = project.GetTracks();
   const auto rate = project.GetRate();

   int numWaveTracksSelected = 0;
   int numNoteTracksSelected = 0;
   int numOtherTracksSelected = 0;
   double endTime = 0.0;

   // Iterate through once to make sure that there is exactly
   // one WaveTrack and one NoteTrack selected.
   GetTracks()->Selected().Visit(
      [&](WaveTrack *wt) {
         numWaveTracksSelected++;
         endTime = endTime > wt->GetEndTime() ? endTime : wt->GetEndTime();
      },
      [&](NoteTrack *) {
         numNoteTracksSelected++;
      },
      [&](Track*) {
         numOtherTracksSelected++;
      }
   );

   if(numWaveTracksSelected == 0 ||
      numNoteTracksSelected != 1 ||
      numOtherTracksSelected != 0){
      AudacityMessageBox(
         _("Please select at least one audio track and one MIDI track."));
      return;
   }

   // Creating the dialog also stores dialog into gScoreAlignDialog so
   // that it can be delted by CloseScoreAlignDialog() either here or
   // if the program is quit by the user while the dialog is up.
   ScoreAlignParams params;

   // safe because the class maintains a global resource pointer
   safenew ScoreAlignDialog(params);

   CloseScoreAlignDialog();

   if (params.mStatus != wxID_OK) return;

   // We're going to do it.
   //pushing the state before the change is wrong (I think)
   //PushState(_("Sync MIDI with Audio"), _("Sync MIDI with Audio"));
   // Make a copy of the note track in case alignment is canceled or fails
   auto holder = nt->Duplicate();
   auto alignedNoteTrack = static_cast<NoteTrack*>(holder.get());
   // Remove offset from NoteTrack because audio is
   // mixed starting at zero and incorporating clip offsets.
   if (alignedNoteTrack->GetOffset() < 0) {
      // remove the negative offset data before alignment
      nt->Clear(alignedNoteTrack->GetOffset(), 0);
   } else if (alignedNoteTrack->GetOffset() > 0) {
      alignedNoteTrack->Shift(alignedNoteTrack->GetOffset());
   }
   alignedNoteTrack->SetOffset(0);

   WaveTrackConstArray waveTracks =
      tracks->GetWaveTrackConstArray(true /* selectionOnly */);

   int result;
   {
      Mixer mix(
         waveTracks,              // const WaveTrackConstArray &inputTracks
         false, // mayThrow -- is this right?
         Mixer::WarpOptions{ tracks->GetTimeTrack() }, // const WarpOptions &warpOptions
         0.0,                     // double startTime
         endTime,                 // double stopTime
         2,                       // int numOutChannels
         44100u,                   // size_t outBufferSize
         true,                    // bool outInterleaved
         rate,                   // double outRate
         floatSample,             // sampleFormat outFormat
         true,                    // bool highQuality = true
         NULL);                   // MixerSpec *mixerSpec = NULL

      ASAProgress progress;

      // There's a lot of adjusting made to incorporate the note track offset into
      // the note track while preserving the position of notes within beats and
      // measures. For debugging, you can see just the pre-scorealign note track
      // manipulation by setting SKIP_ACTUAL_SCORE_ALIGNMENT. You could then, for
      // example, save the modified note track in ".gro" form to read the details.
      //#define SKIP_ACTUAL_SCORE_ALIGNMENT 1
#ifndef SKIP_ACTUAL_SCORE_ALIGNMENT
      result = scorealign((void *) &mix, &mixer_process,
         2 /* channels */, 44100.0 /* srate */, endTime,
         &alignedNoteTrack->GetSeq(), &progress, params);
#else
      result = SA_SUCCESS;
#endif
   }

   if (result == SA_SUCCESS) {
      tracks->Replace(nt, holder);
      project.RedrawProject();
      AudacityMessageBox(wxString::Format(
         _("Alignment completed: MIDI from %.2f to %.2f secs, Audio from %.2f to %.2f secs."),
         params.mMidiStart, params.mMidiEnd,
         params.mAudioStart, params.mAudioEnd));
      project.PushState(_("Sync MIDI with Audio"), _("Sync MIDI with Audio"));
   } else if (result == SA_TOOSHORT) {
      AudacityMessageBox(wxString::Format(
         _("Alignment error: input too short: MIDI from %.2f to %.2f secs, Audio from %.2f to %.2f secs."),
         params.mMidiStart, params.mMidiEnd,
         params.mAudioStart, params.mAudioEnd));
   } else if (result == SA_CANCEL) {
      // wrong way to recover...
      //GetActiveProject()->OnUndo(); // recover any changes to note track
      return; // no message when user cancels alignment
   } else {
      //GetActiveProject()->OnUndo(); // recover any changes to note track
      AudacityMessageBox(_("Internal error reported by alignment process."));
   }
}
#endif /* EXPERIMENTAL_SCOREALIGN */

void OnSortTime(const CommandContext &context)
{
   auto &project = context.project;
   DoSortTracks(project, kAudacitySortByTime);

   project.PushState(_("Tracks sorted by time"), _("Sort by Time"));

   auto &trackPanel = TrackPanel::Get( project );
   trackPanel.Refresh(false);
}

void OnSortName(const CommandContext &context)
{
   auto &project = context.project;
   DoSortTracks(project, kAudacitySortByName);

   project.PushState(_("Tracks sorted by name"), _("Sort by Name"));

   auto &trackPanel = TrackPanel::Get( project );
   trackPanel.Refresh(false);
}

void OnSyncLock(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   bool bSyncLockTracks;
   gPrefs->Read(wxT("/GUI/SyncLockTracks"), &bSyncLockTracks, false);
   gPrefs->Write(wxT("/GUI/SyncLockTracks"), !bSyncLockTracks);
   gPrefs->Flush();

   // Toolbar, project sync-lock handled within
   MenuManager::ModifyAllProjectToolbarMenus();

   trackPanel.Refresh(false);
}

///The following methods operate controls on specified tracks,
///This will pop up the track panning dialog for specified track
void OnTrackPan(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   Track *const track = trackPanel.GetFocusedTrack();
   if (track) track->TypeSwitch( [&](WaveTrack *wt) {
      LWSlider *slider = trackPanel.PanSlider(wt);
      if (slider->ShowDialog())
         SetTrackPan(project, wt, slider);
   });
}

void OnTrackPanLeft(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   Track *const track = trackPanel.GetFocusedTrack();
   if (track) track->TypeSwitch( [&](WaveTrack *wt) {
      LWSlider *slider = trackPanel.PanSlider(wt);
      slider->Decrease(1);
      SetTrackPan(project, wt, slider);
   });
}

void OnTrackPanRight(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   Track *const track = trackPanel.GetFocusedTrack();
   if (track) track->TypeSwitch( [&](WaveTrack *wt) {
      LWSlider *slider = trackPanel.PanSlider(wt);
      slider->Increase(1);
      SetTrackPan(project, wt, slider);
   });
}

void OnTrackGain(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   /// This will pop up the track gain dialog for specified track
   Track *const track = trackPanel.GetFocusedTrack();
   if (track) track->TypeSwitch( [&](WaveTrack *wt) {
      LWSlider *slider = trackPanel.GainSlider(wt);
      if (slider->ShowDialog())
         SetTrackGain(project, wt, slider);
   });
}

void OnTrackGainInc(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   Track *const track = trackPanel.GetFocusedTrack();
   if (track) track->TypeSwitch( [&](WaveTrack *wt) {
      LWSlider *slider = trackPanel.GainSlider(wt);
      slider->Increase(1);
      SetTrackGain(project, wt, slider);
   });
}

void OnTrackGainDec(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   Track *const track = trackPanel.GetFocusedTrack();
   if (track) track->TypeSwitch( [&](WaveTrack *wt) {
      LWSlider *slider = trackPanel.GainSlider(wt);
      slider->Decrease(1);
      SetTrackGain(project, wt, slider);
   });
}

void OnTrackMenu(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   trackPanel.OnTrackMenu();
}

void OnTrackMute(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   const auto track = trackPanel.GetFocusedTrack();
   if (track) track->TypeSwitch( [&](PlayableTrack *t) {
      DoTrackMute(project, t, false);
   });
}

void OnTrackSolo(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );

   const auto track = trackPanel.GetFocusedTrack();
   if (track) track->TypeSwitch( [&](PlayableTrack *t) {
      DoTrackSolo(project, t, false);
   });
}

void OnTrackClose(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );
   auto &window = ProjectWindow::Get( project );

   Track *t = trackPanel.GetFocusedTrack();
   if (!t)
      return;

   auto isAudioActive = project.IsAudioActive();

   if (isAudioActive)
   {
      window.TP_DisplayStatusMessage(
         _("Can't delete track with active audio"));
      wxBell();
      return;
   }

   DoRemoveTrack(project, t);

   trackPanel.UpdateViewIfNoTracks();
   trackPanel.Refresh(false);
}

void OnTrackMoveUp(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );
   auto &tracks = TrackList::Get( project );

   Track *const focusedTrack = trackPanel.GetFocusedTrack();
   if (tracks.CanMoveUp(focusedTrack)) {
      DoMoveTrack(project, focusedTrack, OnMoveUpID);
      trackPanel.Refresh(false);
   }
}

void OnTrackMoveDown(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );
   auto &tracks = TrackList::Get( project );

   Track *const focusedTrack = trackPanel.GetFocusedTrack();
   if (tracks.CanMoveDown(focusedTrack)) {
      DoMoveTrack(project, focusedTrack, OnMoveDownID);
      trackPanel.Refresh(false);
   }
}

void OnTrackMoveTop(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );
   auto &tracks = TrackList::Get( project );

   Track *const focusedTrack = trackPanel.GetFocusedTrack();
   if (tracks.CanMoveUp(focusedTrack)) {
      DoMoveTrack(project, focusedTrack, OnMoveTopID);
      trackPanel.Refresh(false);
   }
}

void OnTrackMoveBottom(const CommandContext &context)
{
   auto &project = context.project;
   auto &trackPanel = TrackPanel::Get( project );
   auto &tracks = TrackList::Get( project );

   Track *const focusedTrack = trackPanel.GetFocusedTrack();
   if (tracks.CanMoveDown(focusedTrack)) {
      DoMoveTrack(project, focusedTrack, OnMoveBottomID);
      trackPanel.Refresh(false);
   }
}

}; // struct Handler

} // namespace

static CommandHandlerObject &findCommandHandler(AudacityProject &) {
   // Handler is not stateful.  Doesn't need a factory registered with
   // AudacityProject.
   static TrackActions::Handler instance;
   return instance;
};

// Menu definitions

#define FN(X) (& TrackActions::Handler :: X)

// Under /MenuBar
MenuTable::BaseItemSharedPtr TracksMenu()
{
   // Tracks Menu (formerly Project Menu)
   using namespace MenuTable;
   using Options = CommandManager::Options;
   
   static BaseItemSharedPtr menu{
   FinderScope( findCommandHandler ).Eval(
   Menu( wxT("Tracks"), XO("&Tracks"),
      Menu( wxT("Add"), XO("Add &New"),
         Command( wxT("NewMonoTrack"), XXO("&Mono Track"), FN(OnNewWaveTrack),
            AudioIONotBusyFlag, wxT("Ctrl+Shift+N") ),
         Command( wxT("NewStereoTrack"), XXO("&Stereo Track"),
            FN(OnNewStereoTrack), AudioIONotBusyFlag ),
         Command( wxT("NewLabelTrack"), XXO("&Label Track"),
            FN(OnNewLabelTrack), AudioIONotBusyFlag ),
         Command( wxT("NewTimeTrack"), XXO("&Time Track"),
            FN(OnNewTimeTrack), AudioIONotBusyFlag )
      ),

      //////////////////////////////////////////////////////////////////////////

      Separator(),

      Menu( wxT("Mix"), XO("Mi&x"),
         // Delayed evaluation
         // Stereo to Mono is an oddball command that is also subject to control
         // by the plug-in manager, as if an effect.  Decide whether to show or
         // hide it.
         [](void*) -> BaseItemPtr {
            const PluginID ID =
               EffectManager::Get().GetEffectByIdentifier(wxT("StereoToMono"));
            const PluginDescriptor *plug = PluginManager::Get().GetPlugin(ID);
            if (plug && plug->IsEnabled())
               return Command( wxT("Stereo to Mono"),
                  XXO("Mix Stereo Down to &Mono"), FN(OnStereoToMono),
                  AudioIONotBusyFlag | StereoRequiredFlag |
                     WaveTracksSelectedFlag, Options{}, findCommandHandler );
            else
               return {};
         },
         Command( wxT("MixAndRender"), XXO("Mi&x and Render"),
            FN(OnMixAndRender),
            AudioIONotBusyFlag | WaveTracksSelectedFlag ),
         Command( wxT("MixAndRenderToNewTrack"),
            XXO("Mix and Render to Ne&w Track"),
            FN(OnMixAndRenderToNewTrack),
            AudioIONotBusyFlag | WaveTracksSelectedFlag, wxT("Ctrl+Shift+M") )
      ),

      Command( wxT("Resample"), XXO("&Resample..."), FN(OnResample),
         AudioIONotBusyFlag | WaveTracksSelectedFlag ),

      Separator(),

      Command( wxT("RemoveTracks"), XXO("Remo&ve Tracks"), FN(OnRemoveTracks),
         AudioIONotBusyFlag | TracksSelectedFlag ),

      Separator(),

      Menu( wxT("Mute"), XO("M&ute/Unmute"),
         Command( wxT("MuteAllTracks"), XXO("&Mute All Tracks"),
            FN(OnMuteAllTracks), AudioIONotBusyFlag, wxT("Ctrl+U") ),
         Command( wxT("UnmuteAllTracks"), XXO("&Unmute All Tracks"),
            FN(OnUnmuteAllTracks), AudioIONotBusyFlag, wxT("Ctrl+Shift+U") )
      ),

      Menu( wxT("Pan"), XO("&Pan"),
         // As Pan changes are not saved on Undo stack,
         // pan settings for all tracks
         // in the project could very easily be lost unless we
         // require the tracks to be selected.
         Command( wxT("PanLeft"), XXO("&Left"), FN(OnPanLeft),
            TracksSelectedFlag,
            Options{}.LongName( XO("Pan Left") ) ),
         Command( wxT("PanRight"), XXO("&Right"), FN(OnPanRight),
            TracksSelectedFlag,
            Options{}.LongName( XO("Pan Right") ) ),
         Command( wxT("PanCenter"), XXO("&Center"), FN(OnPanCenter),
            TracksSelectedFlag,
            Options{}.LongName( XO("Pan Center") ) )
      ),

      Separator(),

      //////////////////////////////////////////////////////////////////////////

      Menu( wxT("Align"), XO("&Align Tracks"), //_("Just Move Tracks"),
         // Mutual alignment of tracks independent of selection or zero
         CommandGroup(wxT("Align"),
            {
               { wxT("EndToEnd"),     XO("&Align End to End") },
               { wxT("Together"),     XO("Align &Together") },
            },
            FN(OnAlignNoSync), AudioIONotBusyFlag | TracksSelectedFlag),

         Separator(),

         // Alignment commands using selection or zero
         CommandGroup(wxT("Align"),
            alignLabels,
            FN(OnAlign), AudioIONotBusyFlag | TracksSelectedFlag),

         Separator(),

         Command( wxT("MoveSelectionWithTracks"),
            XXO("&Move Selection with Tracks (on/off)"),
            FN(OnMoveSelectionWithTracks),
            AlwaysEnabledFlag,
            Options{}.CheckTest( wxT("/GUI/MoveSelectionWithTracks"), false ) )
      ),

#if 0
      // TODO: Can these labels be made clearer?
      // Do we need this sub-menu at all?
      Menu( wxT("MoveSelectionAndTracks"), XO("Move Sele&ction and Tracks"), {
         CommandGroup(wxT("AlignMove"), alignLabels,
            FN(OnAlignMoveSel), AudioIONotBusyFlag | TracksSelectedFlag),
      } ),
#endif

      //////////////////////////////////////////////////////////////////////////

#ifdef EXPERIMENTAL_SCOREALIGN
      Command( wxT("ScoreAlign"), XXO("Synchronize MIDI with Audio"),
         FN(OnScoreAlign),
         AudioIONotBusyFlag | NoteTracksSelectedFlag | WaveTracksSelectedFlag ),
#endif // EXPERIMENTAL_SCOREALIGN

      //////////////////////////////////////////////////////////////////////////

      Menu( wxT("Sort"), XO("S&ort Tracks"),
         Command( wxT("SortByTime"), XXO("By &Start Time"), FN(OnSortTime),
            TracksExistFlag,
            Options{}.LongName( XO("Sort by Time") ) ),
         Command( wxT("SortByName"), XXO("By &Name"), FN(OnSortName),
            TracksExistFlag,
            Options{}.LongName( XO("Sort by Name") ) )
      )

      //////////////////////////////////////////////////////////////////////////

#ifdef EXPERIMENTAL_SYNC_LOCK

      ,
      Separator(),

      Command( wxT("SyncLock"), XXO("Sync-&Lock Tracks (on/off)"),
         FN(OnSyncLock), AlwaysEnabledFlag,
         Options{}.CheckTest( wxT("/GUI/SyncLockTracks"), false ) )

#endif
   ) ) };
   return menu;
}

// Under /MenuBar/Optional/Extra
MenuTable::BaseItemSharedPtr ExtraTrackMenu()
{
   using namespace MenuTable;
   static BaseItemSharedPtr menu{
   FinderScope( findCommandHandler ).Eval(
   Menu( wxT("Track"), XO("&Track"),
      Command( wxT("TrackPan"), XXO("Change P&an on Focused Track..."),
         FN(OnTrackPan),
         TrackPanelHasFocus | TracksExistFlag, wxT("Shift+P") ),
      Command( wxT("TrackPanLeft"), XXO("Pan &Left on Focused Track"),
         FN(OnTrackPanLeft),
         TrackPanelHasFocus | TracksExistFlag, wxT("Alt+Shift+Left") ),
      Command( wxT("TrackPanRight"), XXO("Pan &Right on Focused Track"),
         FN(OnTrackPanRight),
         TrackPanelHasFocus | TracksExistFlag, wxT("Alt+Shift+Right") ),
      Command( wxT("TrackGain"), XXO("Change Gai&n on Focused Track..."),
         FN(OnTrackGain),
         TrackPanelHasFocus | TracksExistFlag, wxT("Shift+G") ),
      Command( wxT("TrackGainInc"), XXO("&Increase Gain on Focused Track"),
         FN(OnTrackGainInc),
         TrackPanelHasFocus | TracksExistFlag, wxT("Alt+Shift+Up") ),
      Command( wxT("TrackGainDec"), XXO("&Decrease Gain on Focused Track"),
         FN(OnTrackGainDec),
         TrackPanelHasFocus | TracksExistFlag, wxT("Alt+Shift+Down") ),
      Command( wxT("TrackMenu"), XXO("Op&en Menu on Focused Track..."),
         FN(OnTrackMenu),
         TracksExistFlag | TrackPanelHasFocus, wxT("Shift+M\tskipKeydown") ),
      Command( wxT("TrackMute"), XXO("M&ute/Unmute Focused Track"),
         FN(OnTrackMute),
         TracksExistFlag | TrackPanelHasFocus, wxT("Shift+U") ),
      Command( wxT("TrackSolo"), XXO("&Solo/Unsolo Focused Track"),
         FN(OnTrackSolo),
         TracksExistFlag | TrackPanelHasFocus, wxT("Shift+S") ),
      Command( wxT("TrackClose"), XXO("&Close Focused Track"),
         FN(OnTrackClose),
         AudioIONotBusyFlag | TrackPanelHasFocus | TracksExistFlag,
         wxT("Shift+C") ),
      Command( wxT("TrackMoveUp"), XXO("Move Focused Track U&p"),
         FN(OnTrackMoveUp),
         AudioIONotBusyFlag | TrackPanelHasFocus | TracksExistFlag ),
      Command( wxT("TrackMoveDown"), XXO("Move Focused Track Do&wn"),
         FN(OnTrackMoveDown),
         AudioIONotBusyFlag | TrackPanelHasFocus | TracksExistFlag ),
      Command( wxT("TrackMoveTop"), XXO("Move Focused Track to T&op"),
         FN(OnTrackMoveTop),
         AudioIONotBusyFlag | TrackPanelHasFocus | TracksExistFlag ),
      Command( wxT("TrackMoveBottom"), XXO("Move Focused Track to &Bottom"),
         FN(OnTrackMoveBottom),
         AudioIONotBusyFlag | TrackPanelHasFocus | TracksExistFlag )
   ) ) };
   return menu;
}

#undef FN
