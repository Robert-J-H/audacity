#include "../Audacity.h"
#include "../Experimental.h"

#include "../AboutDialog.h"
#include "../AudacityApp.h"
#include "../AudacityLogger.h"
#include "../AudioIO.h"
#include "../Dependencies.h"
#include "../FileNames.h"
#include "../Menus.h"
#include "../Project.h"
#include "../ShuttleGui.h"
#include "../SplashDialog.h"
#include "../commands/CommandContext.h"
#include "../commands/CommandManager.h"
#include "../widgets/HelpSystem.h"

#if defined(EXPERIMENTAL_CRASH_REPORT)
#include <wx/debugrpt.h>
#endif

// private helper classes and functions
namespace {

void ShowDiagnostics(
   AudacityProject &project, const wxString &info,
   const wxString &description, const wxString &defaultPath,
   bool fixedWidth = false)
{
   auto &window = ProjectWindow::Get( project );
   wxDialogWrapper dlg( &window, wxID_ANY, description);
   dlg.SetName(dlg.GetTitle());
   ShuttleGui S(&dlg, eIsCreating);

   wxTextCtrl *text;
   S.StartVerticalLay();
   {
      S.SetStyle(wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);
      text = S.Id(wxID_STATIC).AddTextWindow("");

      S.AddStandardButtons(eOkButton | eCancelButton);
   }
   S.EndVerticalLay();

   if (fixedWidth) {
      auto style = text->GetDefaultStyle();
      style.SetFontFamily( wxFONTFAMILY_TELETYPE );
      text->SetDefaultStyle(style);
   }

   *text << info;

   dlg.FindWindowById(wxID_OK)->SetLabel(_("&Save"));
   dlg.SetSize(350, 450);

   if (dlg.ShowModal() == wxID_OK)
   {
      const auto fileDialogTitle =
         wxString::Format( _("Save %s"), description );
      wxString fName = FileNames::SelectFile(FileNames::Operation::Export,
         fileDialogTitle,
         wxEmptyString,
         defaultPath,
         wxT("txt"),
         wxT("*.txt"),
         wxFD_SAVE | wxFD_OVERWRITE_PROMPT | wxRESIZE_BORDER,
         &window);
      if (!fName.empty())
      {
         if (!text->SaveFile(fName))
         {
            AudacityMessageBox(
               wxString::Format( _("Unable to save %s"), description ),
               fileDialogTitle);
         }
      }
   }
}

}

namespace HelpActions {

// exported helper functions

void DoShowLog( AudacityProject & )
{
   AudacityLogger *logger = wxGetApp().GetLogger();
   if (logger) {
      logger->Show();
   }
}

void DoHelpWelcome( AudacityProject &project )
{
   SplashDialog::Show2( &ProjectWindow::Get( project ) );
}

// Menu handler functions

struct Handler : CommandHandlerObject {

void OnQuickFix(const CommandContext &context)
{
   auto &project = context.project;
   QuickFixDialog dlg( &ProjectWindow::Get( project ) );
   dlg.ShowModal();
}

void OnQuickHelp(const CommandContext &context)
{
   auto &project = context.project;
   HelpSystem::ShowHelp(
      &ProjectWindow::Get( project ),
      wxT("Quick_Help"));
}

void OnManual(const CommandContext &context)
{
   auto &project = context.project;
   HelpSystem::ShowHelp(
      &ProjectWindow::Get( project ),
      wxT("Main_Page"));
}

void OnAudioDeviceInfo(const CommandContext &context)
{
   auto &project = context.project;
   wxString info = gAudioIO->GetDeviceInfo();
   ShowDiagnostics( project, info,
      _("Audio Device Info"), wxT("deviceinfo.txt") );
}

#ifdef EXPERIMENTAL_MIDI_OUT
void OnMidiDeviceInfo(const CommandContext &context)
{
   auto &project = context.project;
   wxString info = gAudioIO->GetMidiDeviceInfo();
   ShowDiagnostics( project, info,
      _("MIDI Device Info"), wxT("midideviceinfo.txt") );
}
#endif

void OnShowLog( const CommandContext &context )
{
   DoShowLog( context.project );
}

#if defined(EXPERIMENTAL_CRASH_REPORT)
void OnCrashReport(const CommandContext &WXUNUSED(context) )
{
// Change to "1" to test a real crash
#if 0
   char *p = 0;
   *p = 1234;
#endif
   wxGetApp().GenerateCrashReport(wxDebugReport::Context_Current);
}
#endif

void OnCheckDependencies(const CommandContext &context)
{
   auto &project = context.project;
   ::ShowDependencyDialogIfNeeded(&project, false);
}

void OnMenuTree(const CommandContext &context)
{
   auto &project = context.project;
   
   using namespace MenuTable;
   struct MyVisitor : Visitor
   {
      enum : unsigned { TAB = 3 };
      void BeginGroup( GroupItem &item, const wxArrayString& ) override
      {
         Indent();
         info += item.name;
         Return();
         indentation = wxString{ ' ', TAB * ++level };
      }

      void EndGroup( GroupItem &, const wxArrayString& ) override
      {
         indentation = wxString{ ' ', TAB * --level };
      }

      void Visit( SingleItem &item, const wxArrayString& ) override
      {
         static const wxString separatorName{ '=', 20 };

         Indent();
         info += dynamic_cast<SeparatorItem*>(&item)
            ? separatorName
            : item.name;
         Return();
      }

      void Indent() { info += indentation; }
      void Return() { info += '\n'; }

      unsigned level{};
      wxString indentation;
      wxString info;
   } visitor;

   MenuManager::Visit( visitor, project );

   ShowDiagnostics( project, visitor.info,
      _("Menu Tree"), wxT("menutree.txt"), true );
}

void OnCheckForUpdates(const CommandContext &WXUNUSED(context))
{
   ::OpenInDefaultBrowser( VerCheckUrl());
}

void OnAbout(const CommandContext &context)
{
#ifdef __WXMAC__
   // Modeless dialog, consistent with other Mac applications
   wxCommandEvent dummy;
   wxGetApp().OnMenuAbout(dummy);
#else
   auto &project = context.project;

   // Windows and Linux still modal.
   AboutDialog dlog(&project);
   dlog.ShowModal();
#endif
}

#if 0
// Legacy handlers, not used as of version 2.3.0

// Only does the update checks if it's an ALPHA build and not disabled by
// preferences.
void MayCheckForUpdates(AudacityProject &project)
{
#ifdef IS_ALPHA
   OnCheckForUpdates(project);
#endif
}

void OnHelpWelcome(const CommandContext &context)
{
   DoHelpWelcome( context.project );
}

#endif

}; // struct Handler

} // namespace

static CommandHandlerObject &findCommandHandler(AudacityProject &) {
   // Handler is not stateful.  Doesn't need a factory registered with
   // AudacityProject.
   static HelpActions::Handler instance;
   return instance;
};

// Menu definitions

#define FN(X) (& HelpActions::Handler :: X)

// Under /MenuBar
MenuTable::BaseItemSharedPtr HelpMenu()
{
   using namespace MenuTable;
   static BaseItemSharedPtr menu{
   FinderScope( findCommandHandler ).Eval(
   Menu( wxT("Help"), XO("&Help"),
      // QuickFix menu item not in Audacity 2.3.1 whilst we discuss further.
#ifdef EXPERIMENTAL_DA
      // DA: Has QuickFix menu item.
      Command( wxT("QuickFix"), XXO("&Quick Fix..."), FN(OnQuickFix),
         AlwaysEnabledFlag ),
      // DA: 'Getting Started' rather than 'Quick Help'.
      Command( wxT("QuickHelp"), XXO("&Getting Started"), FN(OnQuickHelp) ),
      // DA: Emphasise it is the Audacity Manual (No separate DA manual).
      Command( wxT("Manual"), XXO("Audacity &Manual"), FN(OnManual) ),
#else
      Command( wxT("QuickHelp"), XXO("&Quick Help..."), FN(OnQuickHelp),
         AlwaysEnabledFlag ),
      Command( wxT("Manual"), XXO("&Manual..."), FN(OnManual),
         AlwaysEnabledFlag ),
#endif

      Separator(),

      Menu( wxT("Diagnostics"), XO("&Diagnostics"),
         Command( wxT("DeviceInfo"), XXO("Au&dio Device Info..."),
            FN(OnAudioDeviceInfo),
            AudioIONotBusyFlag ),
   #ifdef EXPERIMENTAL_MIDI_OUT
         Command( wxT("MidiDeviceInfo"), XXO("&MIDI Device Info..."),
            FN(OnMidiDeviceInfo),
            AudioIONotBusyFlag ),
   #endif
         Command( wxT("Log"), XXO("Show &Log..."), FN(OnShowLog),
            AlwaysEnabledFlag ),
   #if defined(EXPERIMENTAL_CRASH_REPORT)
         Command( wxT("CrashReport"), XXO("&Generate Support Data..."),
            FN(OnCrashReport), AlwaysEnabledFlag ),
   #endif
         Command( wxT("CheckDeps"), XXO("Chec&k Dependencies..."),
            FN(OnCheckDependencies),
            AudioIONotBusyFlag )

#ifdef IS_ALPHA
         ,
         // Menu explorer.  Perhaps this should become a macro command
         Command( wxT("MenuTree"), XXO("Menu Tree..."),
            FN(OnMenuTree),
            AlwaysEnabledFlag )
#endif
      ),

#ifndef __WXMAC__
      Separator(),
#endif

      // DA: Does not fully support update checking.
#ifndef EXPERIMENTAL_DA
      Command( wxT("Updates"), XXO("&Check for Updates..."),
         FN(OnCheckForUpdates),
         AlwaysEnabledFlag ),
#endif
      Command( wxT("About"), XXO("&About Audacity..."), FN(OnAbout),
         AlwaysEnabledFlag )
   ) ) };
   return menu;
}

#undef FN
