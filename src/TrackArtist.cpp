/**********************************************************************

  Audacity: A Digital Audio Editor

  TrackArtist.cpp

  Dominic Mazzoni


*******************************************************************//*!

\class TrackArtist
\brief   This class handles the actual rendering of WaveTracks (both
  waveforms and spectra), NoteTracks, LabelTracks and TimeTracks.

  It's actually a little harder than it looks, because for
  waveforms at least it needs to cache the samples that are
  currently on-screen.

<b>How Audacity Redisplay Works \n
 Roger Dannenberg</b> \n
Oct 2010 \n

In my opinion, the bitmap should contain only the waveform, note, and
label images along with gray selection highlights. The track info
(sliders, buttons, title, etc.), track selection highlight, cursor, and
indicator should be drawn in the normal way, and clipping regions should
be used to avoid excessive copying of bitmaps (say, when sliders move),
or excessive redrawing of track info widgets (say, when scrolling occurs).
This is a fairly tricky code change since it requires careful specification
of what and where redraw should take place when any state changes. One
surprising finding is that NoteTrack display is slow compared to WaveTrack
display. Each note takes some time to gather attributes and select colors,
and while audio draws two amplitudes per horizontal pixels, large MIDI
scores can have more notes than horizontal pixels. This can make slider
changes very sluggish, but this can also be a problem with many
audio tracks.

*//*******************************************************************/

#include "Audacity.h" // for USE_* macros and HAVE_ALLOCA_H
#include "TrackArtist.h"

#include "Experimental.h"

#include "AColor.h"
#include "AllThemeResources.h"
#include "NoteTrack.h"
#include "NumberScale.h"
#include "Prefs.h"
#include "TimeTrack.h"
#include "TrackPanelDrawingContext.h"
#include "SelectedRegion.h"
#include "ViewInfo.h"
#include "WaveTrack.h"

#include "prefs/GUISettings.h"
#include "prefs/TracksPrefs.h"
#include "prefs/SpectrogramSettings.h"
#include "prefs/WaveformSettings.h"
#include "tracks/labeltrack/ui/LabelTrackView.h"
#include "tracks/playabletrack/notetrack/ui/NoteTrackView.h"
#include "tracks/playabletrack/wavetrack/ui/WaveTrackViewGroupData.h"

#include "widgets/Ruler.h"

#ifdef USE_MIDI
/*
const int octaveHeight = 62;
const int blackPos[5] = { 6, 16, 32, 42, 52 };
const int whitePos[7] = { 0, 9, 17, 26, 35, 44, 53 };
const int notePos[12] = { 1, 6, 11, 16, 21, 27,
                        32, 37, 42, 47, 52, 57 };

// map pitch number to window coordinate of the *top* of the note
// Note the "free" variable bottom, which is assumed to be a local
// variable set to the offset of pitch 0 relative to the window
#define IPITCH_TO_Y(t, p) (bottom - ((p) / 12) * octaveHeight - \
                          notePos[(p) % 12] - (t)->GetPitchHeight())

// GetBottom is called from a couple of places to compute the hypothetical
// coordinate of the bottom of pitch 0 in window coordinates. See
// IPITCH_TO_Y above, which computes coordinates relative to GetBottom()
// Note the -NOTE_MARGIN, which leaves a little margin to draw notes that
// are out of bounds. I'm not sure why the -2 is necessary.
int TrackArt::GetBottom(NoteTrack *t, const wxRect &rect)
{
   int bottomNote = t->GetBottomNote();
   int bottom = rect.y + rect.height - 2 - t->GetNoteMargin() +
          ((bottomNote / 12) * octaveHeight + notePos[bottomNote % 12]);
   return bottom;

}
*/
#endif // USE_MIDI

TrackArtist::TrackArtist( TrackPanel *parent_ )
   : parent( parent_ )
{
   mdBrange = ENV_DB_RANGE;
   mShowClipping = false;
   mSampleDisplay = 1;// Stem plots by default.
   UpdatePrefs();

   SetColours(0);
   vruler = std::make_unique<Ruler>();

   UpdatePrefs();
}

TrackArtist::~TrackArtist()
{
}

TrackArtist * TrackArtist::Get( TrackPanelDrawingContext &context )
{
   return static_cast< TrackArtist* >( context.pUserData );
}

void TrackArtist::SetColours( int iColorIndex)
{
   theTheme.SetBrushColour( blankBrush,      clrBlank );
   theTheme.SetBrushColour( unselectedBrush, clrUnselected);
   theTheme.SetBrushColour( selectedBrush,   clrSelected);
   theTheme.SetBrushColour( sampleBrush,     clrSample);
   theTheme.SetBrushColour( selsampleBrush,  clrSelSample);
   theTheme.SetBrushColour( dragsampleBrush, clrDragSample);
   theTheme.SetBrushColour( blankSelectedBrush, clrBlankSelected);

   theTheme.SetPenColour(   blankPen,        clrBlank);
   theTheme.SetPenColour(   unselectedPen,   clrUnselected);
   theTheme.SetPenColour(   selectedPen,     clrSelected);
   theTheme.SetPenColour(   muteSamplePen,   clrMuteSample);
   theTheme.SetPenColour(   odProgressDonePen, clrProgressDone);
   theTheme.SetPenColour(   odProgressNotYetPen, clrProgressNotYet);
   theTheme.SetPenColour(   shadowPen,       clrShadow);
   theTheme.SetPenColour(   clippedPen,      clrClipped);
   theTheme.SetPenColour(   muteClippedPen,  clrMuteClipped);
   theTheme.SetPenColour(   blankSelectedPen,clrBlankSelected);

   theTheme.SetPenColour(   selsamplePen,    clrSelSample);
   theTheme.SetPenColour(   muteRmsPen,      clrMuteRms);

   switch( iColorIndex %4 )
   {
      default:
      case 0:
         theTheme.SetPenColour(   samplePen,       clrSample);
         theTheme.SetPenColour(   rmsPen,          clrRms);
         break;
      case 1: // RED
         samplePen.SetColour( wxColor( 160,10,10 ) );
         rmsPen.SetColour( wxColor( 230,80,80 ) );
         break;
      case 2: // GREEN
         samplePen.SetColour( wxColor( 35,110,35 ) );
         rmsPen.SetColour( wxColor( 75,200,75 ) );
         break;
      case 3: //BLACK
         samplePen.SetColour( wxColor( 0,0,0 ) );
         rmsPen.SetColour( wxColor( 100,100,100 ) );
         break;

   }
}

void TrackArt::DrawVRuler
( TrackPanelDrawingContext &context, const Track *t, const wxRect & rect_,
  bool bSelected )
{
   auto rect = rect_;
   --rect.width;

   auto dc = &context.dc;
   bool highlight = false;
#ifdef EXPERIMENTAL_TRACK_PANEL_HIGHLIGHTING
   highlight = rect.Contains(context.lastState.GetPosition());
#endif


   // Label and Time tracks do not have a vruler
   // But give it a beveled area
   t->TypeSwitch(
      [&](const LabelTrack *) {
         wxRect bev = rect;
         bev.Inflate(-1, 0);
         bev.width += 1;
         AColor::BevelTrackInfo(*dc, true, bev);
      },

      [&](const TimeTrack *) {
         wxRect bev = rect;
         bev.Inflate(-1, 0);
         bev.width += 1;
         AColor::BevelTrackInfo(*dc, true, bev);

         // Right align the ruler
         wxRect rr = rect;
         rr.width--;
         if (t->vrulerSize.GetWidth() < rect.GetWidth()) {
            int adj = rr.GetWidth() - t->vrulerSize.GetWidth();
            rr.x += adj;
            rr.width -= adj;
         }

         const auto artist = TrackArtist::Get( context );
         artist->UpdateVRuler(t, rr);

         const auto &vruler = artist->vruler;
         vruler->SetTickColour( theTheme.Colour( clrTrackPanelText ));
         vruler->Draw(*dc);
      },

      [&](const WaveTrack *) {
         // All waves have a ruler in the info panel
         // The ruler needs a bevelled surround.
         wxRect bev = rect;
         bev.Inflate(-1, 0);
         bev.width += 1;
         AColor::BevelTrackInfo(*dc, true, bev, highlight);

         // Right align the ruler
         wxRect rr = rect;
         rr.width--;
         if (t->vrulerSize.GetWidth() < rect.GetWidth()) {
            int adj = rr.GetWidth() - t->vrulerSize.GetWidth();
            rr.x += adj;
            rr.width -= adj;
         }

         const auto artist = TrackArtist::Get( context );
         artist->UpdateVRuler(t, rr);

         const auto &vruler = artist->vruler;
         vruler->SetTickColour( theTheme.Colour( clrTrackPanelText ));
         vruler->Draw(*dc);
      }

#ifdef USE_MIDI
      ,
      [&](const NoteTrack *track) {
      // The note track draws a vertical keyboard to label pitches
         const auto &view = NoteTrackView::Get( *track );
         const auto artist = TrackArtist::Get( context );
         artist->UpdateVRuler(t, rect);

         dc->SetPen(highlight ? AColor::uglyPen : *wxTRANSPARENT_PEN);
         dc->SetBrush(*wxWHITE_BRUSH);
         wxRect bev = rect;
         bev.x++;
         bev.width--;
         dc->DrawRectangle(bev);

         rect.y += 1;
         rect.height -= 1;

         //int bottom = GetBottom(track, rect);
         view.PrepareIPitchToY(rect);

         wxPen hilitePen;
         hilitePen.SetColour(120, 120, 120);
         wxBrush blackKeyBrush;
         blackKeyBrush.SetColour(70, 70, 70);

         dc->SetBrush(blackKeyBrush);

         int fontSize = 10;
   #ifdef __WXMSW__
         fontSize = 8;
   #endif

         wxFont labelFont(fontSize, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
         dc->SetFont(labelFont);

         int octave = 0;
         int obottom = view.GetOctaveBottom(octave);
         int marg = view.GetNoteMargin(rect.height);
         //IPITCH_TO_Y(octave * 12) + PITCH_HEIGHT + 1;
         while (obottom >= rect.y) {
            dc->SetPen(*wxBLACK_PEN);
            for (int white = 0; white < 7; white++) {
               int pos = view.GetWhitePos(white);
               if (obottom - pos > rect.y + marg + 1 &&
                   // don't draw too close to margin line -- it's annoying
                   obottom - pos < rect.y + rect.height - marg - 3)
                  AColor::Line(*dc, rect.x, obottom - pos,
                               rect.x + rect.width, obottom - pos);
            }
            wxRect br = rect;
            br.height = view.GetPitchHeight(1);
            br.x++;
            br.width = 17;
            for (int black = 0; black < 5; black++) {
               br.y = obottom - view.GetBlackPos(black);
               if (br.y > rect.y + marg - 2 && br.y + br.height < rect.y + rect.height - marg) {
                  dc->SetPen(hilitePen);
                  dc->DrawRectangle(br);
                  dc->SetPen(*wxBLACK_PEN);
                  AColor::Line(*dc,
                               br.x + 1, br.y + br.height - 1,
                               br.x + br.width - 1, br.y + br.height - 1);
                  AColor::Line(*dc,
                               br.x + br.width - 1, br.y + 1,
                               br.x + br.width - 1, br.y + br.height - 1);
               }
            }

            if (octave >= 1 && octave <= 10) {
               wxString s;
               // ISO standard: A440 is in the 4th octave, denoted
               // A4 <- the "4" should be a subscript.
               s.Printf(wxT("C%d"), octave - 1);
               wxCoord width, height;
               dc->GetTextExtent(s, &width, &height);
               if (obottom - height + 4 > rect.y &&
                   obottom + 4 < rect.y + rect.height) {
                  dc->SetTextForeground(wxColour(60, 60, 255));
                  dc->DrawText(s, rect.x + rect.width - width,
                               obottom - height + 2);
               }
            }
            obottom = view.GetOctaveBottom(++octave);
         }
         // draw lines delineating the out-of-bounds margins
         dc->SetPen(*wxBLACK_PEN);
         // you would think the -1 offset here should be -2 to match the
         // adjustment to rect.y (see above), but -1 produces correct output
         AColor::Line(*dc, rect.x, rect.y + marg - 1, rect.x + rect.width, rect.y + marg - 1);
         // since the margin gives us the bottom of the line,
         // the extra -1 gets us to the top
         AColor::Line(*dc, rect.x, rect.y + rect.height - marg - 1,
                           rect.x + rect.width, rect.y + rect.height - marg - 1);

      }
#endif // USE_MIDI
   );
}

void TrackArtist::UpdateVRuler(const Track *t, const wxRect & rect)
{
   auto update = t->TypeSwitch<bool>(
      [] (const LabelTrack *) {
      // Label tracks do not have a vruler
         return false;
      },

      [&](const TimeTrack *tt) {
         float min, max;
         min = tt->GetRangeLower() * 100.0;
         max = tt->GetRangeUpper() * 100.0;

         vruler->SetDbMirrorValue( 0.0 );
         vruler->SetBounds(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height-1);
         vruler->SetOrientation(wxVERTICAL);
         vruler->SetRange(max, min);
         vruler->SetFormat((tt->GetDisplayLog()) ? Ruler::RealLogFormat : Ruler::RealFormat);
         vruler->SetUnits(wxT(""));
         vruler->SetLabelEdges(false);
         vruler->SetLog(tt->GetDisplayLog());
         return true;
      },

      [&](const WaveTrack *wt) {
         auto &data = WaveTrackViewGroupData::Get( *wt );
         // All waves have a ruler in the info panel
         // The ruler needs a bevelled surround.
         const float dBRange =
            data.GetWaveformSettings().dBRange;

         const int display = data.GetDisplay();

         if (display == WaveTrackViewConstants::Waveform) {
            WaveformSettings::ScaleType scaleType =
               data.GetWaveformSettings().scaleType;

            if (scaleType == WaveformSettings::stLinear) {
               // Waveform

               float min, max;
               data.GetDisplayBounds(&min, &max);
               if (data.GetLastScaleType() != scaleType &&
                   data.GetLastScaleType() != -1)
               {
                  // do a translation into the linear space
                  data.SetLastScaleType();
                  data.SetLastdBRange();
                  float sign = (min >= 0 ? 1 : -1);
                  if (min != 0.) {
                     min = DB_TO_LINEAR(fabs(min) * dBRange - dBRange);
                     if (min < 0.0)
                        min = 0.0;
                     min *= sign;
                  }
                  sign = (max >= 0 ? 1 : -1);

                  if (max != 0.) {
                     max = DB_TO_LINEAR(fabs(max) * dBRange - dBRange);
                     if (max < 0.0)
                        max = 0.0;
                     max *= sign;
                  }
                  data.SetDisplayBounds(min, max);
               }

               vruler->SetDbMirrorValue( 0.0 );
               vruler->SetBounds(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height - 1);
               vruler->SetOrientation(wxVERTICAL);
               vruler->SetRange(max, min);
               vruler->SetFormat(Ruler::RealFormat);
               vruler->SetUnits(wxT(""));
               vruler->SetLabelEdges(false);
               vruler->SetLog(false);
            }
            else {
               wxASSERT(scaleType == WaveformSettings::stLogarithmic);
               scaleType = WaveformSettings::stLogarithmic;

               vruler->SetUnits(wxT(""));

               float min, max;
               data.GetDisplayBounds(&min, &max);
               float lastdBRange;

               if (data.GetLastScaleType() != scaleType &&
                   data.GetLastScaleType() != -1)
               {
                  // do a translation into the dB space
                  data.SetLastScaleType();
                  data.SetLastdBRange();
                  float sign = (min >= 0 ? 1 : -1);
                  if (min != 0.) {
                     min = (LINEAR_TO_DB(fabs(min)) + dBRange) / dBRange;
                     if (min < 0.0)
                        min = 0.0;
                     min *= sign;
                  }
                  sign = (max >= 0 ? 1 : -1);

                  if (max != 0.) {
                     max = (LINEAR_TO_DB(fabs(max)) + dBRange) / dBRange;
                     if (max < 0.0)
                        max = 0.0;
                     max *= sign;
                  }
                  data.SetDisplayBounds(min, max);
               }
               else if (dBRange != (lastdBRange = data.GetLastdBRange())) {
                  data.SetLastdBRange();
                  // Remap the max of the scale
                  float newMax = max;

// This commented out code is problematic.
// min and max may be correct, and this code cause them to change.
#ifdef ONLY_LABEL_POSITIVE
                  const float sign = (max >= 0 ? 1 : -1);
                  if (max != 0.) {

   // Ugh, duplicating from TrackPanel.cpp
   #define ZOOMLIMIT 0.001f

                     const float extreme = LINEAR_TO_DB(2);
                     // recover dB value of max
                     const float dB = std::min(extreme, (float(fabs(max)) * lastdBRange - lastdBRange));
                     // find NEW scale position, but old max may get trimmed if the db limit rises
                     // Don't trim it to zero though, but leave max and limit distinct
                     newMax = sign * std::max(ZOOMLIMIT, (dBRange + dB) / dBRange);
                     // Adjust the min of the scale if we can,
                     // so the db Limit remains where it was on screen, but don't violate extremes
                     if (min != 0.)
                        min = std::max(-extreme, newMax * min / max);
                  }
#endif
                  data.SetDisplayBounds(min, newMax);
               }

// Old code was if ONLY_LABEL_POSITIVE were defined.  
// it uses the +1 to 0 range only.
// the enabled code uses +1 to -1, and relies on set ticks labelling knowing about
// the dB scale.
#ifdef ONLY_LABEL_POSITIVE
               if (max > 0) {
#endif
                  int top = 0;
                  float topval = 0;
                  int bot = rect.height;
                  float botval = -dBRange;

#ifdef ONLY_LABEL_POSITIVE
                  if (min < 0) {
                     bot = top + (int)((max / (max - min))*(bot - top));
                     min = 0;
                  }

                  if (max > 1) {
                     top += (int)((max - 1) / (max - min) * (bot - top));
                     max = 1;
                  }

                  if (max < 1 && max > 0)
                     topval = -((1 - max) * dBRange);

                  if (min > 0) {
                     botval = -((1 - min) * dBRange);
                  }
#else
                  topval = -((1 - max) * dBRange);
                  botval = -((1 - min) * dBRange);
                  vruler->SetDbMirrorValue( dBRange );
#endif
                  vruler->SetBounds(rect.x, rect.y + top, rect.x + rect.width, rect.y + bot - 1);
                  vruler->SetOrientation(wxVERTICAL);
                  vruler->SetRange(topval, botval);
#ifdef ONLY_LABEL_POSITIVE
               }
               else
                  vruler->SetBounds(0.0, 0.0, 0.0, 0.0); // A.C.H I couldn't find a way to just disable it?
#endif
               vruler->SetFormat(Ruler::RealLogFormat);
               vruler->SetLabelEdges(true);
               vruler->SetLog(false);
            }
         }
         else {
            wxASSERT(display == WaveTrackViewConstants::Spectrum);
            const SpectrogramSettings &settings = data.GetSpectrogramSettings();
            float minFreq, maxFreq;
            data.GetSpectrumBounds(wt->GetRate(), &minFreq, &maxFreq);
            vruler->SetDbMirrorValue( 0.0 );

            switch (settings.scaleType) {
            default:
               wxASSERT(false);
            case SpectrogramSettings::stLinear:
            {
               // Spectrum

               if (rect.height < 60)
                  return false;

               /*
               draw the ruler
               we will use Hz if maxFreq is < 2000, otherwise we represent kHz,
               and append to the numbers a "k"
               */
               vruler->SetBounds(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height - 1);
               vruler->SetOrientation(wxVERTICAL);
               vruler->SetFormat(Ruler::RealFormat);
               vruler->SetLabelEdges(true);
               // use kHz in scale, if appropriate
               if (maxFreq >= 2000) {
                  vruler->SetRange((maxFreq / 1000.), (minFreq / 1000.));
                  vruler->SetUnits(wxT("k"));
               }
               else {
                  // use Hz
                  vruler->SetRange((int)(maxFreq), (int)(minFreq));
                  vruler->SetUnits(wxT(""));
               }
               vruler->SetLog(false);
            }
            break;
            case SpectrogramSettings::stLogarithmic:
            case SpectrogramSettings::stMel:
            case SpectrogramSettings::stBark:
            case SpectrogramSettings::stErb:
            case SpectrogramSettings::stPeriod:
            {
               // SpectrumLog

               if (rect.height < 10)
                  return false;

               /*
               draw the ruler
               we will use Hz if maxFreq is < 2000, otherwise we represent kHz,
               and append to the numbers a "k"
               */
               vruler->SetBounds(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height - 1);
               vruler->SetOrientation(wxVERTICAL);
               vruler->SetFormat(Ruler::IntFormat);
               vruler->SetLabelEdges(true);
               vruler->SetRange(maxFreq, minFreq);
               vruler->SetUnits(wxT(""));
               vruler->SetLog(true);
               NumberScale scale(
                  data.GetSpectrogramSettings().GetScale( minFreq, maxFreq )
                     .Reversal() );
               vruler->SetNumberScale(&scale);
            }
            break;
            }
         }
         return true;
      }

#ifdef USE_MIDI
      ,
      [&](const NoteTrack *) {
         // The note track isn't drawing a ruler at all!
         // But it needs to!
         vruler->SetBounds(rect.x, rect.y, rect.x + 1, rect.y + rect.height-1);
         vruler->SetOrientation(wxVERTICAL);
         return true;
      }
#endif // USE_MIDI
   );

   if (update)
      vruler->GetMaxSize(&t->vrulerSize.x, &t->vrulerSize.y);
}

/// Takes a value between min and max and returns a value between
/// height and 0
/// \todo  Should this function move int GuiWaveTrack where it can
/// then use the zoomMin, zoomMax and height values without having
/// to have them passed in to it??
int GetWaveYPos(float value, float min, float max,
                int height, bool dB, bool outer,
                float dBr, bool clip)
{
   if (dB) {
      if (height == 0) {
         return 0;
      }

      float sign = (value >= 0 ? 1 : -1);

      if (value != 0.) {
         float db = LINEAR_TO_DB(fabs(value));
         value = (db + dBr) / dBr;
         if (!outer) {
            value -= 0.5;
         }
         if (value < 0.0) {
            value = 0.0;
         }
         value *= sign;
      }
   }
   else {
      if (!outer) {
         if (value >= 0.0) {
            value -= 0.5;
         }
         else {
            value += 0.5;
         }
      }
   }

   if (clip) {
      if (value < min) {
         value = min;
      }
      if (value > max) {
         value = max;
      }
   }

   value = (max - value) / (max - min);
   return (int) (value * (height - 1) + 0.5);
}

float FromDB(float value, double dBRange)
{
   if (value == 0)
      return 0;

   double sign = (value >= 0 ? 1 : -1);
   return DB_TO_LINEAR((fabs(value) * dBRange) - dBRange) * sign;
}

float ValueOfPixel(int yy, int height, bool offset,
   bool dB, double dBRange, float zoomMin, float zoomMax)
{
   wxASSERT(height > 0);
   // Map 0 to max and height - 1 (not height) to min
   float v =
      height == 1 ? (zoomMin + zoomMax) / 2 :
      zoomMax - (yy / (float)(height - 1)) * (zoomMax - zoomMin);
   if (offset) {
      if (v > 0.0)
         v += .5;
      else
         v -= .5;
   }

   if (dB)
      v = FromDB(v, dBRange);

   return v;
}

void TrackArt::DrawNegativeOffsetTrackArrows(
   TrackPanelDrawingContext &context, const wxRect &rect )
{
   auto &dc = context.dc;

   // Draws two black arrows on the left side of the track to
   // indicate the user that the track has been time-shifted
   // to the left beyond t=0.0.

   dc.SetPen(*wxBLACK_PEN);
   AColor::Line(dc,
                rect.x + 2, rect.y + 6,
                rect.x + 8, rect.y + 6);
   AColor::Line(dc,
                rect.x + 2, rect.y + 6,
                rect.x + 6, rect.y + 2);
   AColor::Line(dc,
                rect.x + 2, rect.y + 6,
                rect.x + 6, rect.y + 10);
   AColor::Line(dc,
                rect.x + 2, rect.y + rect.height - 8,
                rect.x + 8, rect.y + rect.height - 8);
   AColor::Line(dc,
                rect.x + 2, rect.y + rect.height - 8,
                rect.x + 6, rect.y + rect.height - 4);
   AColor::Line(dc,
                rect.x + 2, rect.y + rect.height - 8,
                rect.x + 6, rect.y + rect.height - 12);
}

void TrackArtist::UpdateSelectedPrefs( int id )
{
   if( id == ShowClippingPrefsId())
      mShowClipping = gPrefs->Read(wxT("/GUI/ShowClipping"), mShowClipping);
}

void TrackArtist::UpdatePrefs()
{
   mdBrange = gPrefs->Read(ENV_DB_KEY, mdBrange);
   mSampleDisplay = TracksPrefs::SampleViewChoice();

   mbShowTrackNameInWaveform =
      gPrefs->ReadBool(wxT("/GUI/ShowTrackNameInWaveform"), false);
   
   UpdateSelectedPrefs(ShowClippingPrefsId() );

   SetColours(0);
}

// Draws the sync-lock bitmap, tiled; always draws stationary relative to the DC
//
// AWD: now that the tiles don't link together, we're drawing a tilted grid, at
// two steps down for every one across. This creates a pattern that repeats in
// 5-step by 5-step boxes. Because we're only drawing in 5/25 possible positions
// we have a grid spacing somewhat smaller than the image dimensions. Thus we
// acheive lower density than with a square grid and eliminate edge cases where
// no tiles are displayed.
//
// The pattern draws in tiles at (0,0), (2,1), (4,2), (1,3), and (3,4) in each
// 5x5 box.
//
// There may be a better way to do this, or a more appealing pattern.
void TrackArt::DrawSyncLockTiles(
   TrackPanelDrawingContext &context, const wxRect &rect )
{
   const auto dc = &context.dc;

   wxBitmap syncLockBitmap(theTheme.Image(bmpSyncLockSelTile));

   // Grid spacing is a bit smaller than actual image size
   int gridW = syncLockBitmap.GetWidth() - 6;
   int gridH = syncLockBitmap.GetHeight() - 8;

   // Horizontal position within the grid, modulo its period
   int blockX = (rect.x / gridW) % 5;

   // Amount to offset drawing of first column
   int xOffset = rect.x % gridW;
   if (xOffset < 0) xOffset += gridW;

   // Check if we're missing an extra column to the left (this can happen
   // because the tiles are bigger than the grid spacing)
   bool extraCol = false;
   if (syncLockBitmap.GetWidth() - gridW > xOffset) {
      extraCol = true;
      xOffset += gridW;
      blockX = (blockX - 1) % 5;
   }
   // Make sure blockX is non-negative
   if (blockX < 0) blockX += 5;

   int xx = 0;
   while (xx < rect.width) {
      int width = syncLockBitmap.GetWidth() - xOffset;
      if (xx + width > rect.width)
         width = rect.width - xx;

      //
      // Draw each row in this column
      //

      // Vertical position in the grid, modulo its period
      int blockY = (rect.y / gridH) % 5;

      // Amount to offset drawing of first row
      int yOffset = rect.y % gridH;
      if (yOffset < 0) yOffset += gridH;

      // Check if we're missing an extra row on top (this can happen because
      // the tiles are bigger than the grid spacing)
      bool extraRow = false;
      if (syncLockBitmap.GetHeight() - gridH > yOffset) {
         extraRow = true;
         yOffset += gridH;
         blockY = (blockY - 1) % 5;
      }
      // Make sure blockY is non-negative
      if (blockY < 0) blockY += 5;

      int yy = 0;
      while (yy < rect.height)
      {
         int height = syncLockBitmap.GetHeight() - yOffset;
         if (yy + height > rect.height)
            height = rect.height - yy;

         // AWD: draw blocks according to our pattern
         if ((blockX == 0 && blockY == 0) || (blockX == 2 && blockY == 1) ||
             (blockX == 4 && blockY == 2) || (blockX == 1 && blockY == 3) ||
             (blockX == 3 && blockY == 4))
         {

            // Do we need to get a sub-bitmap?
            if (width != syncLockBitmap.GetWidth() || height != syncLockBitmap.GetHeight()) {
               wxBitmap subSyncLockBitmap =
                  syncLockBitmap.GetSubBitmap(wxRect(xOffset, yOffset, width, height));
               dc->DrawBitmap(subSyncLockBitmap, rect.x + xx, rect.y + yy, true);
            }
            else {
               dc->DrawBitmap(syncLockBitmap, rect.x + xx, rect.y + yy, true);
            }
         }

         // Updates for next row
         if (extraRow) {
            // Second offset row, still at y = 0; no more extra rows
            yOffset -= gridH;
            extraRow = false;
         }
         else {
            // Move on in y, no more offset rows
            yy += gridH - yOffset;
            yOffset = 0;
         }
         blockY = (blockY + 1) % 5;
      }

      // Updates for next column
      if (extraCol) {
         // Second offset column, still at x = 0; no more extra columns
         xOffset -= gridW;
         extraCol = false;
      }
      else {
         // Move on in x, no more offset rows
         xx += gridW - xOffset;
         xOffset = 0;
      }
      blockX = (blockX + 1) % 5;
   }
}

void TrackArt::DrawBackgroundWithSelection(
   TrackPanelDrawingContext &context, const wxRect &rect,
   const Track *track, const wxBrush &selBrush, const wxBrush &unselBrush,
   bool useSelection)
{
   const auto dc = &context.dc;
   const auto artist = TrackArtist::Get( context );
   const auto &selectedRegion = *artist->pSelectedRegion;
   const auto &zoomInfo = *artist->pZoomInfo;

   //MM: Draw background. We should optimize that a bit more.
   const double sel0 = useSelection ? selectedRegion.t0() : 0.0;
   const double sel1 = useSelection ? selectedRegion.t1() : 0.0;

   dc->SetPen(*wxTRANSPARENT_PEN);
   if (track->GetSelected() || track->IsSyncLockSelected())
   {
      // Rectangles before, within, after the selction
      wxRect before = rect;
      wxRect within = rect;
      wxRect after = rect;

      before.width = (int)(zoomInfo.TimeToPosition(sel0) );
      if (before.GetRight() > rect.GetRight()) {
         before.width = rect.width;
      }

      if (before.width > 0) {
         dc->SetBrush(unselBrush);
         dc->DrawRectangle(before);

         within.x = 1 + before.GetRight();
      }
      within.width = rect.x + (int)(zoomInfo.TimeToPosition(sel1) ) - within.x -1;

      if (within.GetRight() > rect.GetRight()) {
         within.width = 1 + rect.GetRight() - within.x;
      }

      if (within.width > 0) {
         if (track->GetSelected()) {
            dc->SetBrush(selBrush);
            dc->DrawRectangle(within);
         }
         else {
            // Per condition above, track must be sync-lock selected
            dc->SetBrush(unselBrush);
            dc->DrawRectangle(within);
            DrawSyncLockTiles( context, within );
         }

         after.x = 1 + within.GetRight();
      }
      else {
         // `within` not drawn; start where it would have gone
         after.x = within.x;
      }

      after.width = 1 + rect.GetRight() - after.x;
      if (after.width > 0) {
         dc->SetBrush(unselBrush);
         dc->DrawRectangle(after);
      }
   }
   else
   {
      // Track not selected; just draw background
      dc->SetBrush(unselBrush);
      dc->DrawRectangle(rect);
   }
}

int TrackArtist::ShowClippingPrefsId()
{
   static int value = wxNewId();
   return value;
}

