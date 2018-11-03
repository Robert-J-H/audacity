/**********************************************************************

Audacity: A Digital Audio Editor

NoteTrackView.h

Paul Licameli split from class NoteTrack

**********************************************************************/

#ifndef __AUDACITY_NOTE_TRACK_VIEW__
#define __AUDACITY_NOTE_TRACK_VIEW__

#include "../../../ui/TrackView.h"

class NoteTrack;

class NoteTrackView final : public TrackView
{
   NoteTrackView( const NoteTrackView& ) = delete;
   NoteTrackView &operator=( const NoteTrackView& ) = delete;

public:
   explicit
   NoteTrackView( const std::shared_ptr<Track> &pTrack );
   ~NoteTrackView() override;

   static NoteTrackView &Get( NoteTrack & );
   static const NoteTrackView &Get( const NoteTrack & );

   std::shared_ptr<TrackVRulerControls> DoGetVRulerControls() override;

   int GetPitchHeight(int factor) const
      { return std::max(1, (int)(factor * mPitchHeight)); }
   void SetPitchHeight(int rectHeight, float h)
   {
      // Impose certain zoom limits
      auto octavePadding = 2 * 10; // 10 octaves times 2 single-pixel seperations per pixel
      auto availableHeight = rectHeight - octavePadding;
      auto numNotes = 128.f;
      auto minSpacePerNote =
         std::max((float)MinPitchHeight, availableHeight / numNotes);
      mPitchHeight =
         std::max(minSpacePerNote,
                  std::min((float)MaxPitchHeight, h));
   }

   // call this once before a series of calls to IPitchToY(). It
   // sets mBottom to offset of octave 0 so that mBottomNote
   // is located at r.y + r.height - (GetNoteMargin() + 1 + GetPitchHeight())
   void PrepareIPitchToY(const wxRect &r) const {
       mBottom =
         r.y + r.height - GetNoteMargin(r.height) - 1 - GetPitchHeight(1) +
             (mBottomNote / 12) * GetOctaveHeight() +
                GetNotePos(mBottomNote % 12);
   }

   // IPitchToY returns Y coordinate of top of pitch p
   int IPitchToY(int p) const {
      return mBottom - (p / 12) * GetOctaveHeight() - GetNotePos(p % 12);
   }

   // compute the window coordinate of the bottom of an octave: This is
   // the bottom of the line separating B and C.
   int GetOctaveBottom(int oct) const {
      return IPitchToY(oct * 12) + GetPitchHeight(1) + 1;
   }

   // Y coordinate for given floating point pitch (rounded to int)
   int PitchToY(double p) const {
      return IPitchToY((int) (p + 0.5));
   }

   // Integer pitch corresponding to a Y coordinate
   int YToIPitch(int y);

   // map pitch class number (0-11) to pixel offset from bottom of octave
   // (the bottom of the black line between B and C) to the top of the
   // note. Note extra pixel separates B(11)/C(0) and E(4)/F(5).
   int GetNotePos(int p) const
      { return 1 + GetPitchHeight(p + 1) + (p > 4); }

   // get pixel offset to top of ith black key note
   int GetBlackPos(int i) const { return GetNotePos(i * 2 + 1 + (i > 1)); }

   // GetWhitePos tells where to draw lines between keys as an offset from
   // GetOctaveBottom. GetWhitePos(0) returns 1, which matches the location
   // of the line separating B and C
   int GetWhitePos(int i) const { return 1 + (i * GetOctaveHeight()) / 7; }

   void SetBottomNote(int note)
   {
      if (note < 0)
         note = 0;
      else if (note > 96)
         note = 96;

      mBottomNote = note;
   }

   int GetBottomNote() const { return mBottomNote; }

   /// Zooms out a constant factor (subject to zoom limits)
   void ZoomOut(const wxRect &rect, int y)
      { Zoom(rect, y, 1.0f / ZoomStep, true); }

   /// Zooms in a contant factor (subject to zoom limits)
   void ZoomIn(const wxRect &rect, int y) { Zoom(rect, y, ZoomStep, true); }

   /// Zoom the note track around y.
   /// If center is true, the result will be centered at y.
   void Zoom(const wxRect &rect, int y, float multiplier, bool center);

   void ZoomTo(const wxRect &rect, int start, int end);

   int GetNoteMargin(int height) const
      { return std::min(height / 4, (GetPitchHeight(1) + 1) / 2); }

   int GetOctaveHeight() const { return GetPitchHeight(12) + 2; }

private:
   // Preserve some view state too for undo/redo purposes
   void Copy( const TrackView &other ) override;

   std::vector<UIHandlePtr> DetailedHitTest
      (const TrackPanelMouseState &state,
       const AudacityProject *pProject, int currentTool, bool bMultiTool)
      override;

   void DoSetHeight(int h) override;

   // TrackPanelDrawable implementation
   void Draw(
      TrackPanelDrawingContext &context,
      const wxRect &rect, unsigned iPass ) override;

   // mBottom is the Y offset of pitch 0 (normally off screen)
   mutable int mBottom;
   int mBottomNote{ 24 };

   // Remember continuous variation for zooming,
   // but it is rounded off whenever drawing:
   float mPitchHeight{ 5.0f };

   enum { MinPitchHeight = 1, MaxPitchHeight = 25 };

   static const float ZoomStep;
};

#endif
