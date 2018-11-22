/**********************************************************************

Audacity: A Digital Audio Editor

NoteTrackView.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../../../Audacity.h" // for USE_* macros
#include "NoteTrackView.h"
#include "NoteTrackVRulerControls.h"

#ifdef USE_MIDI
#include "../../../../NoteTrack.h"

#include "../../../../Experimental.h"

#include "../../../../HitTestResult.h"
#include "../../../../Project.h"
#include "../../../../TrackPanel.h" // for TrackInfo
#include "../../../../TrackPanelMouseEvent.h"
#include "../../../ui/SelectHandle.h"
#include "StretchHandle.h"

NoteTrackView::NoteTrackView( const std::shared_ptr<Track> &pTrack )
   : TrackView{ pTrack }
{
   DoSetHeight( TrackInfo::DefaultNoteTrackHeight() );
}

NoteTrackView::~NoteTrackView()
{
}

NoteTrackView &NoteTrackView::Get( NoteTrack &track )
{
   return *static_cast< NoteTrackView* >( &TrackView::Get( track ) );
}

const NoteTrackView &NoteTrackView::Get( const NoteTrack &track )
{
   return *static_cast< const NoteTrackView* >( &TrackView::Get( track ) );
}

void NoteTrackView::Copy( const TrackView &other )
{
   TrackView::Copy( other );
   
   if ( const auto pOther = dynamic_cast< const NoteTrackView* >( &other ) ) {
      mPitchHeight = pOther->mPitchHeight;
      mBottomNote  = pOther->mBottomNote;
   }
}

int NoteTrackView::YToIPitch(int y)
{
   y = mBottom - y; // pixels above pitch 0
   int octave = (y / GetOctaveHeight());
   y -= octave * GetOctaveHeight();
   // result is approximate because C and G are one pixel taller than
   // mPitchHeight.
   return (y / GetPitchHeight(1)) + octave * 12;
}

void NoteTrackView::Zoom(const wxRect &rect, int y, float multiplier, bool center)
{
   // Construct track rectangle to map pitch to screen coordinates
   // Only y and height are needed:
   wxRect trackRect(0, rect.GetY(), 1, rect.GetHeight());
   PrepareIPitchToY(trackRect);
   int clickedPitch = YToIPitch(y);
   // zoom by changing the pitch height
   SetPitchHeight(rect.height, mPitchHeight * multiplier);
   PrepareIPitchToY(trackRect); // update because mPitchHeight changed
   if (center) {
      int newCenterPitch = YToIPitch(rect.GetY() + rect.GetHeight() / 2);
      // center the pitch that the user clicked on
      SetBottomNote(mBottomNote + (clickedPitch - newCenterPitch));
   } else {
      int newClickedPitch = YToIPitch(y);
      // align to keep the pitch that the user clicked on in the same place
      SetBottomNote(mBottomNote + (clickedPitch - newClickedPitch));
   }
}

void NoteTrackView::ZoomTo(const wxRect &rect, int start, int end)
{
   wxRect trackRect(0, rect.GetY(), 1, rect.GetHeight());
   PrepareIPitchToY(trackRect);
   int topPitch = YToIPitch(start);
   int botPitch = YToIPitch(end);
   if (topPitch < botPitch) { // swap
      int temp = topPitch; topPitch = botPitch; botPitch = temp;
   }
   if (topPitch == botPitch) { // can't divide by zero, do something else
      Zoom(rect, start, 1, true);
      return;
   }
   auto trialPitchHeight = (float)trackRect.height / (topPitch - botPitch);
   Zoom(rect, (start + end) / 2, trialPitchHeight / mPitchHeight, true);
}

std::vector<UIHandlePtr> NoteTrackView::DetailedHitTest
(const TrackPanelMouseState &WXUNUSED(state),
 const AudacityProject *WXUNUSED(pProject), int, bool )
{
   // Eligible for stretch?
   UIHandlePtr result;
   std::vector<UIHandlePtr> results;
#ifdef USE_MIDI
#ifdef EXPERIMENTAL_MIDI_STRETCHING
   result = StretchHandle::HitTest(
      mStretchHandle, state, pProject, Pointer<NoteTrack>(this) );
   if (result)
      results.push_back(result);
#endif
#endif

   return results;
}

std::shared_ptr<TrackVRulerControls> NoteTrackView::DoGetVRulerControls()
{
   return
      std::make_shared<NoteTrackVRulerControls>( shared_from_this() );
}

void NoteTrackView::DoSetHeight(int h)
{
   auto oldHeight = GetHeight();
   auto oldMargin = GetNoteMargin(oldHeight);
   TrackView::DoSetHeight(h);
   auto margin = GetNoteMargin(h);
   Zoom(
      wxRect{ 0, 0, 1, h }, // only height matters
      h - margin - 1, // preserve bottom note
      (float)(h - 2 * margin) /
           std::max(1, oldHeight - 2 * oldMargin),
      false
   );
}

const float NoteTrackView::ZoomStep = powf( 2.0f, 0.25f );

#endif
