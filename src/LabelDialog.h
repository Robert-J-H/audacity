/**********************************************************************

  Audacity: A Digital Audio Editor

  LabelDialog.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_LABELDIALOG__
#define __AUDACITY_LABELDIALOG__

#include <vector>
#include <wx/defs.h>

#include "Internat.h"
#include "widgets/wxPanelWrapper.h" // to inherit
#include "audacity/ComponentInterface.h" // member variable

class wxArrayString;
class wxGridEvent;
class ChoiceEditor;
class Grid;
class NumericEditor;
class TrackFactory;
class TrackList;
class RowData;
class EmptyLabelRenderer;
class LabelTrack;
class ViewInfo;
class ShuttleGui;

typedef std::vector<RowData> RowDataArray;

class LabelDialog final : public wxDialogWrapper
{
 public:

   LabelDialog(wxWindow *parent,
               TrackFactory &factory,
               TrackList *tracks,

               // if NULL edit all tracks, else this one only:
               LabelTrack *selectedTrack,

               // This is nonnegative only if selectedTrack is not NULL
               // and is the unique label to edit
               int index,

               ViewInfo &viewinfo,
               double rate,
               const NumericFormatSymbol & format,
               const NumericFormatSymbol &freqFormat);
   ~LabelDialog();

    bool Show(bool show = true) override;

 private:

   void Populate();
   void PopulateOrExchange( ShuttleGui & S );
   void PopulateLabels();
   virtual void OnHelp(wxCommandEvent & event);
   virtual wxString GetHelpPageName() {return "Labels_Editor";};

   bool TransferDataToWindow() override;
   bool TransferDataFromWindow() override;
   bool Validate() override;
   void FindAllLabels();
   void AddLabels(const LabelTrack *t);
   void FindInitialRow();
   wxString TrackName(int & index, const wxString &dflt = _("Label Track"));

   void OnUpdate(wxCommandEvent &event);
   void OnFreqUpdate(wxCommandEvent &event);
   void OnInsert(wxCommandEvent &event);
   void OnRemove(wxCommandEvent &event);
   void OnImport(wxCommandEvent &event);
   void OnExport(wxCommandEvent &event);
   void OnSelectCell(wxGridEvent &event);
   void OnCellChange(wxGridEvent &event);
   void OnChangeTrack(wxGridEvent &event, int row, RowData *rd);
   void OnChangeLabel(wxGridEvent &event, int row, RowData *rd);
   void OnChangeStime(wxGridEvent &event, int row, RowData *rd);
   void OnChangeEtime(wxGridEvent &event, int row, RowData *rd);
   void OnChangeLfreq(wxGridEvent &event, int row, RowData *rd);
   void OnChangeHfreq(wxGridEvent &event, int row, RowData *rd);
   void OnOK(wxCommandEvent &event);
   void OnCancel(wxCommandEvent &event);

   void ReadSize();
   void WriteSize();

 private:

   Grid *mGrid;
   ChoiceEditor *mChoiceEditor;
   NumericEditor *mTimeEditor;
   NumericEditor *mFrequencyEditor;

   RowDataArray mData;

   TrackFactory &mFactory;
   TrackList *mTracks;
   LabelTrack *mSelectedTrack {};
   int mIndex { -1 };
   ViewInfo *mViewInfo;
   wxArrayString mTrackNames;
   double mRate;
   NumericFormatSymbol mFormat, mFreqFormat;

   int mInitialRow;

   DECLARE_EVENT_TABLE()
};

#endif
