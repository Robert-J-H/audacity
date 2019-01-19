/**********************************************************************

  Audacity: A Digital Audio Editor

  TransportState.h

  Paul Licameli
  split from ControlToolBar.h

***********************************************************************/

#ifndef __AUDACITY_TRANSPORT_STATE__
#define __AUDACITY_TRANSPORT_STATE__

#include <memory>
#include <vector>

class AudacityProject;
struct AudioIOStartStreamOptions;
class SelectedRegion;
class TrackList;
struct TransportTracks;

class WaveTrack;
using WaveTrackArray = std::vector < std::shared_ptr < WaveTrack > >;

enum class PlayMode : int {
   normalPlay,
   oneSecondPlay, // Disables auto-scrolling
   loopedPlay, // Disables auto-scrolling
   cutPreviewPlay
};

class TransportState
{
public:
   static bool IsTransportingPinned();

   // Starting and stopping of scrolling display
   static void StartScrollingIfPreferred();
   static void StartScrolling();
   static void StopScrolling();

   // A project is only allowed to stop an audio stream that it owns.
   static bool CanStopAudioStream ();

   // Play currently selected region, or if nothing selected,
   // play from current cursor.
   static void PlayCurrentRegion(bool looped = false, bool cutpreview = false);
   // Play the region [t0,t1]
   // Return the Audio IO token or -1 for failure
   static int PlayPlayRegion(const SelectedRegion &selectedRegion,
                      const AudioIOStartStreamOptions &options,
                      PlayMode playMode,
                      bool backwards = false,
                      // Allow t0 and t1 to be beyond end of tracks
                      bool playWhiteSpace = false);

   // Stop playing
   static void StopPlaying(bool stopStream = true);

   // Pause - used by AudioIO to pause sound activate recording
   static void Pause();

   static bool DoRecord(AudacityProject &project,
      const TransportTracks &transportTracks, // If captureTracks is empty, then tracks are created
      double t0, double t1,
      bool altAppearance,
      const AudioIOStartStreamOptions &options);

   // Find suitable tracks to record into, or return an empty array.
   static WaveTrackArray ChooseExistingRecordingTracks(AudacityProject &proj, bool selectedOnly);

   // Commit the addition of temporary recording tracks into the project
   static void CommitRecording();

   // Cancel the addition of temporary recording tracks into the project
   static void CancelRecording();

   static void SetupCutPreviewTracks(double playStart, double cutStart,
                             double cutEnd, double playEnd);
   static void ClearCutPreviewTracks();

   static PlayMode sLastPlayMode;
private:
   static std::shared_ptr<TrackList> mCutPreviewTracks;
   static AudacityProject *mBusyProject;
};

#endif
