/**********************************************************************

Audacity: A Digital Audio Editor

LabelTrackView.h

Paul Licameli split from class LabelTrack

**********************************************************************/

#ifndef __AUDACITY_LABEL_TRACK_VIEW__
#define __AUDACITY_LABEL_TRACK_VIEW__

#include "../../ui/TrackView.h"

class LabelDefaultClickHandle;
class LabelGlyphHandle;
class LabelTextHandle;
class LabelStruct;
class LabelTrack;
struct LabelTrackEvent;
struct LabelTrackHit;
class SelectedRegion;
struct TrackListEvent;
struct TrackPanelDrawingContext;
class ZoomInfo;

class wxBitmap;
class wxCommandEvent;
class wxDC;
class wxMouseEvent;

constexpr int NUM_GLYPH_CONFIGS = 3;
constexpr int NUM_GLYPH_HIGHLIGHTS = 4;
constexpr int MAX_NUM_ROWS =80;

class LabelTrackView final : public TrackView
{
   LabelTrackView( const LabelTrackView& ) = delete;
   LabelTrackView &operator=( const LabelTrackView& ) = delete;

public:
   enum : int { DefaultFontSize = 12 };
   
   explicit
   LabelTrackView( const std::shared_ptr<Track> &pTrack );
   ~LabelTrackView() override;

   static LabelTrackView &Get( LabelTrack& );
   static const LabelTrackView &Get( const LabelTrack& );

   //This returns the index of the label we just added.
   int AddLabel(const SelectedRegion &region,
      const wxString &title = {},
      int restoreFocus = -1);

private:

   // Preserve some view state too for undo/redo purposes
   void Copy( const TrackView &other ) override;

public:
   static void DoEditLabels(
      AudacityProject &project, LabelTrack *lt = nullptr, int index = -1);

   static int DialogForLabelName(
      AudacityProject &project, const SelectedRegion& region,
      const wxString& initialValue, wxString& value);

   bool IsTextSelected();

private:
   void CreateCustomGlyphs();

public:
   std::vector<UIHandlePtr> DetailedHitTest
      (const TrackPanelMouseState &state,
       const AudacityProject *pProject, int currentTool, bool bMultiTool)
      ;

   bool DoCaptureKey(wxKeyEvent &event);

   static wxFont GetFont(const wxString &faceName, int size = DefaultFontSize);   
   static void ResetFont();

   void Draw( TrackPanelDrawingContext &context, const wxRect & r ) const;

   int GetSelectedIndex() const;
   void SetSelectedIndex( int index );

   bool CutSelectedText();
   bool CopySelectedText();
   bool PasteSelectedText(double sel0, double sel1);

private:
   void OverGlyph(LabelTrackHit &hit, int x, int y) const;

   static wxBitmap & GetGlyph( int i);

   struct Flags {
      int mInitialCursorPos, mCurrentCursorPos, mSelIndex;
      bool mRightDragging, mDrawCursor;
   };

   void ResetFlags();
   Flags SaveFlags() const
   {
      return {
         mInitialCursorPos, mCurrentCursorPos, mSelIndex,
         mRightDragging, mDrawCursor
      };
   }
   void RestoreFlags( const Flags& flags );

   int OverATextBox(int xx, int yy) const;
   bool OverTextBox(const LabelStruct *pLabel, int x, int y) const;

   static bool IsTextClipSupported();
   
   void HandleGlyphClick
      (LabelTrackHit &hit,
       const wxMouseEvent & evt, const wxRect & r, const ZoomInfo &zoomInfo,
       SelectedRegion *newSel);
   void HandleTextClick
      (const wxMouseEvent & evt, const wxRect & r, const ZoomInfo &zoomInfo,
       SelectedRegion *newSel);
   bool HandleGlyphDragRelease
      (LabelTrackHit &hit,
       const wxMouseEvent & evt, wxRect & r, const ZoomInfo &zoomInfo,
       SelectedRegion *newSel);
   void HandleTextDragRelease(const wxMouseEvent & evt);

public:
   bool DoKeyDown(SelectedRegion &sel, wxKeyEvent & event);
   bool DoChar(SelectedRegion &sel, wxKeyEvent & event);

   void AddedLabel( const wxString &title, int pos );
   void DeletedLabel( int index );

private:
   //And this tells us the index, if there is a label already there.
   int GetLabelIndex(double t, double t1);

public:
   //get current cursor position,
   // relative to the left edge of the track panel
   bool CalcCursorX(int * x) const;

private:
   void CalcHighlightXs(int *x1, int *x2) const;

   void MayAdjustLabel
      ( LabelTrackHit &hit,
        int iLabel, int iEdge, bool bAllowSwapping, double fNewTime);
   void MayMoveLabel( int iLabel, int iEdge, double fNewTime);

   void ShowContextMenu();
   void OnContextMenu(wxCommandEvent & evt);

   mutable int mSelIndex{-1};  /// Keeps track of the currently selected label
   int mxMouseDisplacement;    /// Displacement of mouse cursor from the centre being dragged.
   
   static int mIconHeight;
   static int mIconWidth;
   static int mTextHeight;

   static bool mbGlyphsReady;
   static wxBitmap mBoundaryGlyphs[NUM_GLYPH_CONFIGS * NUM_GLYPH_HIGHLIGHTS];

   static int mFontHeight;
   int mCurrentCursorPos;                      /// current cursor position
   int mInitialCursorPos;                      /// initial cursor position

   bool mRightDragging;                        /// flag to tell if it's a valid dragging
   bool mDrawCursor;                           /// flag to tell if drawing the
                                                  /// cursor or not
   int mRestoreFocus{-1};                          /// Restore focus to this track
                                                  /// when done editing

   void ComputeTextPosition(const wxRect & r, int index) const;
   void ComputeLayout(const wxRect & r, const ZoomInfo &zoomInfo) const;
   static void DrawLines( wxDC & dc, const LabelStruct &ls, const wxRect & r);
   static void DrawGlyphs( wxDC & dc, const LabelStruct &ls, const wxRect & r,
      int GlyphLeft, int GlyphRight);
   static void DrawText( wxDC & dc, const LabelStruct &ls, const wxRect & r);
   static void DrawTextBox( wxDC & dc, const LabelStruct &ls, const wxRect & r);
   static void DrawHighlight(
      wxDC & dc, const LabelStruct &ls, int xPos1, int xPos2, int charHeight);

   int FindCurrentCursorPosition(int xPos);
   void SetCurrentCursorPosition(int xPos);

   static void calculateFontHeight(wxDC & dc);
   bool HasSelection() const;
   void RemoveSelectedText();

   void OnLabelAdded( LabelTrackEvent& );
   void OnLabelDeleted( LabelTrackEvent& );
   void OnLabelPermuted( LabelTrackEvent& );

   std::shared_ptr<TrackVRulerControls> DoGetVRulerControls() override;

   std::shared_ptr<LabelTrack> FindLabelTrack();
   std::shared_ptr<const LabelTrack> FindLabelTrack() const;

   static wxFont msFont;

   std::weak_ptr<LabelGlyphHandle> mGlyphHandle;
   std::weak_ptr<LabelTextHandle> mTextHandle;

   friend LabelDefaultClickHandle;
   friend LabelGlyphHandle;
   friend LabelTextHandle;
};

#endif
