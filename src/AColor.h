/**********************************************************************

  Audacity: A Digital Audio Editor

  AColor.h

  Dominic Mazzoni

  Manages color brushes and pens and provides utility
  drawing functions

**********************************************************************/

#ifndef __AUDACITY_COLOR__
#define __AUDACITY_COLOR__

#include "MemoryX.h"
#include <wx/brush.h> // member variable
#include <wx/pen.h> // member variable

class wxDC;
class wxRect;

/// Used to restore pen, brush and logical-op in a DC back to what they were.
struct DCUnchanger {
public:
   DCUnchanger() {}

   DCUnchanger(const wxBrush &brush_, const wxPen &pen_, long logicalOperation_)
   : brush(brush_), pen(pen_), logicalOperation(logicalOperation_)
   {}

   void operator () (wxDC *pDC) const;

   wxBrush brush {};
   wxPen pen {};
   long logicalOperation {};
};

/// Makes temporary drawing context changes that you back out of, RAII style
//  It's like wxDCPenChanger, etc., but simple and general
class ADCChanger : public std::unique_ptr<wxDC, ::DCUnchanger>
{
   using Base = std::unique_ptr<wxDC, ::DCUnchanger>;
public:
   ADCChanger() : Base{} {}
   ADCChanger(wxDC *pDC);
};

class AColor {
 public:

    enum ColorGradientChoice {
      ColorGradientUnselected = 0,
      ColorGradientTimeSelected,
      ColorGradientTimeAndFrequencySelected,
      ColorGradientEdge,

      ColorGradientTotal // keep me last
   };

   static void Init();
   static void ReInit();

   static void Arrow(wxDC & dc, wxCoord x, wxCoord y, int width, bool down = true);

   // Draw a line, INCLUSIVE of both endpoints
   // (unlike what wxDC::DrawLine() documentation specifies)
   static void Line(wxDC & dc, wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);

   // Draw lines, INCLUSIVE of all endpoints
   static void Lines(wxDC &dc, size_t nPoints, const wxPoint points[]);

   static void DrawFocus(wxDC & dc, wxRect & r);
   static void Bevel(wxDC & dc, bool up, const wxRect & r);
   static void Bevel2
      (wxDC & dc, bool up, const wxRect & r, bool bSel=false, bool bHighlight = false);
   static void BevelTrackInfo(wxDC & dc, bool up, const wxRect & r, bool highlight = false);
   static wxColour Blend(const wxColour & c1, const wxColour & c2);

   static void UseThemeColour( wxDC * dc, int iBrush, int iPen=-1, int alpha = 255 );
   static void TrackPanelBackground(wxDC * dc, bool selected);

   static void Light(wxDC * dc, bool selected, bool highlight = false);
   static void Medium(wxDC * dc, bool selected);
   static void MediumTrackInfo(wxDC * dc, bool selected);
   static void Dark(wxDC * dc, bool selected, bool highlight = false);

   static void CursorColor(wxDC * dc);
   static void IndicatorColor(wxDC * dc, bool bIsNotRecording);
   static void PlayRegionColor(wxDC * dc, bool locked);

   static void Mute(wxDC * dc, bool on, bool selected, bool soloing);
   static void Solo(wxDC * dc, bool on, bool selected);

   // In all of these, channel is 1-indexed (1 through 16); if out of bounds
   // (either due to being explicitly set to 0 or due to an allegro file with
   // more than 16 channels) a gray color is returned.

   static void MIDIChannel(wxDC * dc, int channel /* 1 - 16 */ );
   static void LightMIDIChannel(wxDC * dc, int channel /* 1 - 16 */ );
   static void DarkMIDIChannel(wxDC * dc, int channel /* 1 - 16 */ );

   static void TrackFocusPen(wxDC * dc, int level /* 0 - 2 */);
   static void SnapGuidePen(wxDC * dc);

   static void PreComputeGradient();

   // Member variables

   static wxBrush lightBrush[2];
   static wxBrush mediumBrush[2];
   static wxBrush darkBrush[2];
   static wxPen lightPen[2];
   static wxPen mediumPen[2];
   static wxPen darkPen[2];

   static wxPen cursorPen;
   static wxPen indicatorPen[2];
   static wxBrush indicatorBrush[2];
   static wxPen playRegionPen[2];
   static wxBrush playRegionBrush[2];

   static wxBrush muteBrush[2];
   static wxBrush soloBrush;

   static wxPen clippingPen;

   static wxPen envelopePen;
   static wxPen WideEnvelopePen;
   static wxBrush envelopeBrush;

   static wxBrush labelTextNormalBrush;
   static wxBrush labelTextEditBrush;
   static wxBrush labelUnselectedBrush;
   static wxBrush labelSelectedBrush;
   static wxBrush labelSyncLockSelBrush;
   static wxPen labelUnselectedPen;
   static wxPen labelSelectedPen;
   static wxPen labelSyncLockSelPen;
   static wxPen labelSurroundPen;

   static wxPen trackFocusPens[3];
   static wxPen snapGuidePen;

   static wxPen tooltipPen;
   static wxBrush tooltipBrush;

   static bool gradient_inited;
   static const int gradientSteps = 512;
   static unsigned char gradient_pre[ColorGradientTotal][2][gradientSteps][3];

   // For experiments in mouse-over highlighting only
   static wxPen uglyPen;
   static wxBrush uglyBrush;

 private:
   static wxPen sparePen;
   static wxBrush spareBrush;
   static bool inited;

};

inline void GetColorGradient(float value,
                             AColor::ColorGradientChoice selected,
                             bool grayscale,
                             unsigned char * __restrict red,
                             unsigned char * __restrict green,
                             unsigned char * __restrict blue) {

   int idx = value * (AColor::gradientSteps - 1);

   *red = AColor::gradient_pre[selected][grayscale][idx][0];
   *green = AColor::gradient_pre[selected][grayscale][idx][1];
   *blue = AColor::gradient_pre[selected][grayscale][idx][2];
}

#endif
