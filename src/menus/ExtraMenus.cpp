#include "../Prefs.h"
#include "../Project.h"
#include "../commands/CommandContext.h"
#include "../commands/CommandManager.h"
#include "../toolbars/MixerToolBar.h"
#include "../toolbars/DeviceToolBar.h"

// helper functions and classes
namespace {
}

/// Namespace for helper functions for Extra menu
namespace ExtraActions {

// exported helper functions
// none

// Menu handler functions

struct Handler : CommandHandlerObject {

void OnOutputGain(const CommandContext &context)
{
   auto &project = context.project;
   auto tb = &MixerToolBar::Get( project );

   if (tb) {
      tb->ShowOutputGainDialog();
   }
}

void OnOutputGainInc(const CommandContext &context)
{
   auto &project = context.project;
   auto tb = &MixerToolBar::Get( project );

   if (tb) {
      tb->AdjustOutputGain(1);
   }
}

void OnOutputGainDec(const CommandContext &context)
{
   auto &project = context.project;
   auto tb = &MixerToolBar::Get( project );

   if (tb) {
      tb->AdjustOutputGain(-1);
   }
}

void OnInputGain(const CommandContext &context)
{
   auto &project = context.project;
   auto tb = &MixerToolBar::Get( project );

   if (tb) {
      tb->ShowInputGainDialog();
   }
}

void OnInputGainInc(const CommandContext &context)
{
   auto &project = context.project;
   auto tb = &MixerToolBar::Get( project );

   if (tb) {
      tb->AdjustInputGain(1);
   }
}

void OnInputGainDec(const CommandContext &context)
{
   auto &project = context.project;
   auto tb = &MixerToolBar::Get( project );

   if (tb) {
      tb->AdjustInputGain(-1);
   }
}

void OnInputDevice(const CommandContext &context)
{
   auto &project = context.project;
   auto &tb = DeviceToolBar::Get( project );
   tb.ShowInputDialog();
}

void OnOutputDevice(const CommandContext &context)
{
   auto &project = context.project;
   auto &tb = DeviceToolBar::Get( project );
   tb.ShowOutputDialog();
}

void OnInputChannels(const CommandContext &context)
{
   auto &project = context.project;
   auto &tb = DeviceToolBar::Get( project );
   tb.ShowChannelsDialog();
}

void OnAudioHost(const CommandContext &context)
{
   auto &project = context.project;
   auto &tb = DeviceToolBar::Get( project );
   tb.ShowHostDialog();
}

void OnFullScreen(const CommandContext &context)
{
   auto &project = context.project;
   auto &window = ProjectWindow::Get( project );
   auto &commandManager = CommandManager::Get( project );

   bool bChecked = !window.wxTopLevelWindow::IsFullScreen();
   window.wxTopLevelWindow::ShowFullScreen(bChecked);
   commandManager.Check(wxT("FullScreenOnOff"), bChecked);
}

}; // struct Handler

} // namespace

static CommandHandlerObject &findCommandHandler(AudacityProject &) {
   // Handler is not stateful.  Doesn't need a factory registered with
   // AudacityProject.
   static ExtraActions::Handler instance;
   return instance;
};

// Menu definitions

#define FN(X) (& ExtraActions::Handler :: X)

namespace {
using namespace MenuTable;

BaseItemSharedPtr ExtraMixerMenu();
BaseItemSharedPtr ExtraDeviceMenu();
BaseItemPtr ExtraMiscItems( void* );

BaseItemSharedPtr ExtraMenu()
{
   using namespace MenuTable;

   // Table of menu factories.
   // TODO:  devise a registration system instead.
   static BaseItemSharedPtr extraItems{ Items( wxEmptyString
      , Items( wxT("Part1")
         , ExtraMixerMenu()
         , ExtraDeviceMenu()
      )

      , MenuTable::Separator()

      , Items( wxT("Part2")
         // Delayed evaluation:
         , ExtraMiscItems
      )
   ) };

   static const auto pred =
      []{ return gPrefs->ReadBool(wxT("/GUI/ShowExtraMenus"), false); };
   static BaseItemSharedPtr menu{
      ConditionalItems( wxT("Optional"),
         pred, Menu( wxT("Extra"), XO("Ext&ra"), extraItems ) )
   };
   return menu;
}

AttachedItem sAttachment1{
   wxT(""),
   Shared( ExtraMenu() )
};

// Under /MenuBar/Optional/Extra/Part1
BaseItemSharedPtr ExtraMixerMenu()
{
   using namespace MenuTable;
   static BaseItemSharedPtr menu{
   FinderScope( findCommandHandler ).Eval(
   Menu( wxT("Mixer"), XO("Mi&xer"),
      Command( wxT("OutputGain"), XXO("Ad&just Playback Volume..."),
         FN(OnOutputGain), AlwaysEnabledFlag ),
      Command( wxT("OutputGainInc"), XXO("&Increase Playback Volume"),
         FN(OnOutputGainInc), AlwaysEnabledFlag ),
      Command( wxT("OutputGainDec"), XXO("&Decrease Playback Volume"),
         FN(OnOutputGainDec), AlwaysEnabledFlag ),
      Command( wxT("InputGain"), XXO("Adj&ust Recording Volume..."),
         FN(OnInputGain), AlwaysEnabledFlag ),
      Command( wxT("InputGainInc"), XXO("I&ncrease Recording Volume"),
         FN(OnInputGainInc), AlwaysEnabledFlag ),
      Command( wxT("InputGainDec"), XXO("D&ecrease Recording Volume"),
         FN(OnInputGainDec), AlwaysEnabledFlag )
   ) ) };
   return menu;
}

// Under /MenuBar/Optional/Extra/Part1
BaseItemSharedPtr ExtraDeviceMenu()
{
   using namespace MenuTable;
   static BaseItemSharedPtr menu{
   FinderScope( findCommandHandler ).Eval(
   Menu( wxT("Device"), XO("De&vice"),
      Command( wxT("InputDevice"), XXO("Change &Recording Device..."),
         FN(OnInputDevice),
         AudioIONotBusyFlag, wxT("Shift+I") ),
      Command( wxT("OutputDevice"), XXO("Change &Playback Device..."),
         FN(OnOutputDevice),
         AudioIONotBusyFlag, wxT("Shift+O") ),
      Command( wxT("AudioHost"), XXO("Change Audio &Host..."), FN(OnAudioHost),
         AudioIONotBusyFlag, wxT("Shift+H") ),
      Command( wxT("InputChannels"), XXO("Change Recording Cha&nnels..."),
         FN(OnInputChannels),
         AudioIONotBusyFlag, wxT("Shift+N") )
   ) ) };
   return menu;
}

// Under /MenuBar/Optional/Extra/Part2
BaseItemPtr ExtraMiscItems( void *pContext )
{
   using namespace MenuTable;
   using Options = CommandManager::Options;
   auto &project = *static_cast< AudacityProject * >( pContext );

   constexpr auto key =
#ifdef __WXMAC__
      wxT("Ctrl+/")
#else
      wxT("F11")
#endif
   ;

   // Not a menu.
   return FinderScope( findCommandHandler ).Eval(
   Items( wxT("Misc"),
      // Accel key is not bindable.
      Command( wxT("FullScreenOnOff"), XXO("&Full Screen (on/off)"),
         FN(OnFullScreen),
         AlwaysEnabledFlag,
         Options{ key }.CheckState(
            ProjectWindow::Get( project ).wxTopLevelWindow::IsFullScreen() ) )
   ) );
}

}

#undef FN
