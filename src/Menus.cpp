/**********************************************************************

  Audacity: A Digital Audio Editor

  Menus.cpp

  Dominic Mazzoni
  Brian Gunlogson
  et al.

*******************************************************************//**

\file Menus.cpp
\brief Functions for building toobar menus and enabling and disabling items

*//****************************************************************//**

\class MenuCreator
\brief MenuCreator is responsible for creating the main menu bar.

*//****************************************************************//**

\class MenuManager
\brief MenuManager handles updates to menu state.

*//*******************************************************************/

#include "Audacity.h" // for USE_* macros
#include "Menus.h"

#include "Experimental.h"

#include "AdornedRulerPanel.h"
#include "AudacityApp.h"
#include "AudioIO.h"
#include "LabelTrack.h"
#include "ModuleManager.h"
#ifdef USE_MIDI
#include "NoteTrack.h"
#endif // USE_MIDI
#include "Prefs.h"
#include "Project.h"
#include "TrackPanel.h"
#include "UndoManager.h"
#include "ViewInfo.h"
#include "WaveTrack.h"
#include "commands/CommandManager.h"
#include "effects/EffectManager.h"
#include "prefs/TracksPrefs.h"
#include "toolbars/ControlToolBar.h"
#include "toolbars/ToolManager.h"

#include <wx/menu.h>

MenuCreator::MenuCreator()
{
}

MenuCreator::~MenuCreator()
{
}

static const AudacityProject::AttachedObjects::RegisteredFactory key{
  []( AudacityProject&){
     return std::make_shared< MenuManager >(); }
};

MenuManager &MenuManager::Get( AudacityProject &project )
{
   return project.AttachedObjects::Get< MenuManager >( key );
}

const MenuManager &MenuManager::Get( const AudacityProject &project )
{
   return Get( const_cast< AudacityProject & >( project ) );
}

MenuManager::MenuManager()
{
   UpdatePrefs();
}

void MenuManager::UpdatePrefs()
{
   bool bSelectAllIfNone;
   gPrefs->Read(wxT("/GUI/SelectAllOnNone"), &bSelectAllIfNone, false);
   // 0 is grey out, 1 is Autoselect, 2 is Give warnings.
#ifdef EXPERIMENTAL_DA
   // DA warns or greys out.
   mWhatIfNoSelection = bSelectAllIfNone ? 2 : 0;
#else
   // Audacity autoselects or warns.
   mWhatIfNoSelection = bSelectAllIfNone ? 1 : 2;
#endif
   mStopIfWasPaused = true;  // not configurable for now, but could be later.
}

/// Namespace for structures that go into building a menu
namespace Registry {

BaseItem::~BaseItem() {}

SharedItem::~SharedItem() {}

ComputedItem::~ComputedItem() {}

SingleItem::~SingleItem() {}

GroupItem::GroupItem( const wxString &internalName, BaseItemPtrs &&items_ )
: BaseItem{ internalName }, items{ std::move( items_ ) }
{
}
void GroupItem::AppendOne( BaseItemPtr&& ptr )
{
   items.push_back( std::move( ptr ) );
}
GroupItem::~GroupItem() {}

GroupingItem::~GroupingItem() {}

}

namespace MenuTable {

MenuItem::MenuItem( const wxString &internalName,
   const wxString &title_, BaseItemPtrs &&items_ )
: GroupItem{ internalName, std::move( items_ ) }, title{ title_ }
{
   wxASSERT( !title.empty() );
}
MenuItem::~MenuItem() {}

ConditionalGroupItem::ConditionalGroupItem(
   const wxString &internalName, Condition condition_, BaseItemPtrs &&items_ )
: GroupItem{ internalName, std::move( items_ ) }, condition{ condition_ }
{
}
ConditionalGroupItem::~ConditionalGroupItem() {}

SeparatorItem::~SeparatorItem() {}

CommandItem::CommandItem(const CommandID &name_,
         const wxString &label_in_,
         CommandHandlerFinder finder_,
         CommandFunctorPointer callback_,
         CommandFlag flags_,
         const CommandManager::Options &options_)
: SingleItem{ name_ }, label_in{ label_in_ }
, finder{ finder_ }, callback{ callback_ }
, flags{ flags_ }, options{ options_ }
{}
CommandItem::~CommandItem() {}

CommandGroupItem::CommandGroupItem(const wxString &name_,
         std::initializer_list< ComponentInterfaceSymbol > items_,
         CommandHandlerFinder finder_,
         CommandFunctorPointer callback_,
         CommandFlag flags_,
         bool isEffect_)
: SingleItem{ name_ }, items{ items_ }
, finder{ finder_ }, callback{ callback_ }
, flags{ flags_ }, isEffect{ isEffect_ }
{}
CommandGroupItem::~CommandGroupItem() {}

SpecialItem::~SpecialItem() {}

CommandHandlerFinder FinderScope::sFinder =
   [](AudacityProject &project) -> CommandHandlerObject & {
      // If this default finder function is reached, then FinderScope should
      // have been used somewhere, or an explicit CommandHandlerFinder passed
      // to menu item constructors
      wxASSERT( false );
      return project;
   };

}

namespace {

const auto MenuPathStart = wxT("MenuBar");

using namespace MenuTable;
struct CollectedItems
{
   std::vector< BaseItem * > items;
   std::vector< std::shared_ptr< BaseItem > > &computedItems;
};

// forward declaration for mutually recursive functions
void CollectItem( void *context,
   CollectedItems &collection, BaseItem *Item );
void CollectItems( void *context,
   CollectedItems &collection, const BaseItemPtrs &items )
{
   for ( auto &item : items )
      CollectItem( context, collection, item.get() );
}
void CollectItem( void *context,
   CollectedItems &collection, BaseItem *pItem )
{
   if (!pItem)
      return;

   using namespace MenuTable;
   if (const auto pShared =
       dynamic_cast<SharedItem*>( pItem )) {
      auto delegate = pShared->ptr;
      CollectItem( context, collection, delegate.get() );
   }
   else
   if (const auto pComputed =
       dynamic_cast<ComputedItem*>( pItem )) {
      auto result = pComputed->factory( context );
      if (result) {
         // Guarantee long enough lifetime of the result
         collection.computedItems.push_back( result );
         // recursion
         CollectItem( context, collection, result.get() );
      }
   }
   else
   if (auto pGroup = dynamic_cast<GroupItem*>(pItem)) {
      if (dynamic_cast<GroupingItem*>(pItem) && pItem->name.empty())
         // nameless grouping item is transparent to path calculations
         CollectItems( context, collection, pGroup->items );
      else
         // all other group items
         collection.items.push_back( pItem );
   }
   else {
      wxASSERT( dynamic_cast<SingleItem*>(pItem) );
      // common to all single items
      collection.items.push_back( pItem );
   }
}

using Path = wxArrayString;

// forward declaration for mutually recursive functions
void VisitItem(
   void *context, CollectedItems &collection,
   Path &path, BaseItem *pItem );
void VisitItems(
   void *context, CollectedItems &collection,
   Path &path, GroupItem *pGroup )
{
   // Make a new collection for this subtree, sharing the memo cache
   CollectedItems newCollection{ {}, collection.computedItems };

   // Gather items at this level
   CollectItems( context, newCollection, pGroup->items );

   // Now visit them
   path.push_back( pGroup->name );
   for ( auto &pSubItem : newCollection.items )
      VisitItem( context, collection, path, pSubItem );
   path.pop_back();
}
void VisitItem(
   void *context, CollectedItems &collection,
   Path &path, BaseItem *pItem )
{
   if (!pItem)
      return;

   auto &project = *static_cast< AudacityProject* >( context );
   auto &manager = CommandManager::Get( project );

   if (const auto pCommand =
       dynamic_cast<CommandItem*>( pItem )) {
      manager.AddItem(
         pCommand->name, pCommand->label_in,
         pCommand->finder, pCommand->callback,
         pCommand->flags, pCommand->options
      );
   }
   else
   if (const auto pCommandList =
      dynamic_cast<CommandGroupItem*>( pItem ) ) {
      manager.AddItemList(pCommandList->name,
         pCommandList->items.data(), pCommandList->items.size(),
         pCommandList->finder, pCommandList->callback,
         pCommandList->flags, pCommandList->isEffect);
   }
   else
   if (const auto pMenu =
       dynamic_cast<MenuItem*>( pItem )) {
      auto title = pMenu->translated
         ? pMenu->title
         : ::wxGetTranslation( pMenu->title );
      manager.BeginMenu( title );
      // recursion
      VisitItems( context, collection, path, pMenu );
      manager.EndMenu();
   }
   else
   if (const auto pConditionalGroup =
       dynamic_cast<ConditionalGroupItem*>( pItem )) {
      const auto flag = pConditionalGroup->condition();
      if (!flag)
         manager.BeginOccultCommands();
      // recursion
      VisitItems( context, collection, path, pConditionalGroup );
      if (!flag)
         manager.EndOccultCommands();
   }
   else
   if (const auto pGroup =
       dynamic_cast<GroupingItem*>( pItem )) {
      // recursion
      VisitItems( context, collection, path, pGroup );
   }
   else
   if (dynamic_cast<SeparatorItem*>( pItem )) {
      manager.AddSeparator();
   }
   else
   if (const auto pSpecial =
       dynamic_cast<SpecialItem*>( pItem )) {
      const auto pCurrentMenu = manager.CurrentMenu();
      wxASSERT( pCurrentMenu );
      pSpecial->fn( project, *pCurrentMenu );
   }
   else
      wxASSERT( false );
}

}

namespace Registry {

void VisitTopItem( void *context, BaseItem *pTopItem )
{
   std::vector< MenuTable::BaseItemSharedPtr > computedItems;
   CollectedItems collection{ {}, computedItems };
   Path emptyPath;
   VisitItem( context, collection, emptyPath, pTopItem );
}

}

/// CreateMenusAndCommands builds the menus, and also rebuilds them after
/// changes in configured preferences - for example changes in key-bindings
/// affect the short-cut key legend that appears beside each command,

MenuTable::BaseItemSharedPtr FileMenu();

MenuTable::BaseItemSharedPtr EditMenu();

MenuTable::BaseItemSharedPtr SelectMenu();

MenuTable::BaseItemSharedPtr ViewMenu();

MenuTable::BaseItemSharedPtr TransportMenu();

MenuTable::BaseItemSharedPtr TracksMenu();

MenuTable::BaseItemSharedPtr GenerateMenu();
MenuTable::BaseItemSharedPtr EffectMenu();
MenuTable::BaseItemSharedPtr AnalyzeMenu();
MenuTable::BaseItemSharedPtr ToolsMenu();

MenuTable::BaseItemSharedPtr WindowMenu();

MenuTable::BaseItemSharedPtr ExtraMenu();

MenuTable::BaseItemSharedPtr HelpMenu();

// Table of menu factories.
// TODO:  devise a registration system instead.
static const auto menuTree = MenuTable::Items( MenuPathStart
   , FileMenu()
   , EditMenu()
   , SelectMenu()
   , ViewMenu()
   , TransportMenu()
   , TracksMenu()
   , GenerateMenu()
   , EffectMenu()
   , AnalyzeMenu()
   , ToolsMenu()
   , WindowMenu()
   , ExtraMenu()
   , HelpMenu()
);

void MenuCreator::CreateMenusAndCommands(AudacityProject &project)
{
   auto &commandManager = CommandManager::Get( project );

   // The list of defaults to exclude depends on
   // preference wxT("/GUI/Shortcuts/FullDefaults"), which may have changed.
   commandManager.SetMaxList();

   auto menubar = commandManager.AddMenuBar(wxT("appmenu"));
   wxASSERT(menubar);

   Registry::VisitTopItem( &project, menuTree.get() );

   ProjectWindow::Get( project ).SetMenuBar(menubar.release());

   mLastFlags = AlwaysEnabledFlag;

#if defined(__WXDEBUG__)
//   c->CheckDups();
#endif
}

// TODO: This surely belongs in CommandManager?
void MenuManager::ModifyUndoMenuItems(AudacityProject &project)
{
   wxString desc;
   auto &undoManager = UndoManager::Get( project );
   auto &commandManager = CommandManager::Get( project );
   int cur = undoManager.GetCurrentState();

   if (undoManager.UndoAvailable()) {
      undoManager.GetShortDescription(cur, &desc);
      commandManager.Modify(wxT("Undo"),
                             wxString::Format(_("&Undo %s"),
                                              desc));
      commandManager.Enable(wxT("Undo"), project.UndoAvailable());
   }
   else {
      commandManager.Modify(wxT("Undo"),
                            _("&Undo"));
   }

   if (undoManager.RedoAvailable()) {
      undoManager.GetShortDescription(cur+1, &desc);
      commandManager.Modify(wxT("Redo"),
                             wxString::Format(_("&Redo %s"),
                                              desc));
      commandManager.Enable(wxT("Redo"), project.RedoAvailable());
   }
   else {
      commandManager.Modify(wxT("Redo"),
                            _("&Redo"));
      commandManager.Enable(wxT("Redo"), false);
   }
}

void MenuCreator::RebuildMenuBar(AudacityProject &project)
{
   // On OSX, we can't rebuild the menus while a modal dialog is being shown
   // since the enabled state for menus like Quit and Preference gets out of
   // sync with wxWidgets idea of what it should be.
#if defined(__WXMAC__) && defined(__WXDEBUG__)
   {
      wxDialog *dlg =
         wxDynamicCast(wxGetTopLevelParent(wxWindow::FindFocus()), wxDialog);
      wxASSERT((!dlg || !dlg->IsModal()));
   }
#endif

   // Delete the menus, since we will soon recreate them.
   // Rather oddly, the menus don't vanish as a result of doing this.
   {
      auto &window = ProjectWindow::Get( project );
      std::unique_ptr<wxMenuBar> menuBar{ window.GetMenuBar() }; // DestroyPtr ?
      window.DetachMenuBar();
      // menuBar gets deleted here
   }

   CommandManager::Get( project ).PurgeData();

   CreateMenusAndCommands(project);

   ModuleManager::Get().Dispatch(MenusRebuilt);
}

CommandFlag MenuManager::GetFocusedFrame(AudacityProject &project)
{
   wxWindow *w = wxWindow::FindFocus();

   while (w) {
      if (w == ToolManager::Get( project ).GetTopDock()) {
         return TopDockHasFocus;
      }

      if (w == &AdornedRulerPanel::Get( project ))
         return RulerHasFocus;

      if (dynamic_cast<NonKeystrokeInterceptingWindow*>(w)) {
         return TrackPanelHasFocus;
      }
      if (w == ToolManager::Get( project ).GetBotDock()) {
         return BotDockHasFocus;
      }

      w = w->GetParent();
   }

   return AlwaysEnabledFlag;
}


CommandFlag MenuManager::GetUpdateFlags
(AudacityProject &project, bool checkActive)
{
   // This method determines all of the flags that determine whether
   // certain menu items and commands should be enabled or disabled,
   // and returns them in a bitfield.  Note that if none of the flags
   // have changed, it's not necessary to even check for updates.
   auto flags = AlwaysEnabledFlag;
   // static variable, used to remember flags for next time.
   static auto lastFlags = flags;

   // if (auto focus = wxWindow::FindFocus()) {
   auto &window = ProjectWindow::Get( project );
   if (wxWindow * focus = &window) {
      while (focus && focus->GetParent())
         focus = focus->GetParent();
      if (focus && !static_cast<wxTopLevelWindow*>(focus)->IsIconized())
         flags |= NotMinimizedFlag;
   }

   // These flags are cheap to calculate.
   if (!gAudioIO->IsAudioTokenActive(project.GetAudioIOToken()))
      flags |= AudioIONotBusyFlag;
   else
      flags |= AudioIOBusyFlag;

   if( gAudioIO->IsPaused() )
      flags |= PausedFlag;
   else
      flags |= NotPausedFlag;

   // quick 'short-circuit' return.
   if ( checkActive && !window.IsActive() ){
      const auto checkedFlags = 
         NotMinimizedFlag | AudioIONotBusyFlag | AudioIOBusyFlag |
         PausedFlag | NotPausedFlag;
      // short cirucit return should preserve flags that have not been calculated.
      flags = (lastFlags & ~checkedFlags) | flags;
      lastFlags = flags;
      return flags;
   }

   auto &viewInfo = ViewInfo::Get( project );
   const auto &selectedRegion = viewInfo.selectedRegion;

   if (!selectedRegion.isPoint())
      flags |= TimeSelectedFlag;

   auto &tracks = TrackList::Get( project );
   auto trackRange = tracks.Any();
   if ( trackRange )
      flags |= TracksExistFlag;
   trackRange.Visit(
      [&](LabelTrack *lt) {
         flags |= LabelTracksExistFlag;

         if (lt->GetSelected()) {
            flags |= TracksSelectedFlag;
            for (int i = 0; i < lt->GetNumLabels(); i++) {
               const LabelStruct *ls = lt->GetLabel(i);
               if (ls->getT0() >= selectedRegion.t0() &&
                   ls->getT1() <= selectedRegion.t1()) {
                  flags |= LabelsSelectedFlag;
                  break;
               }
            }
         }

         if (lt->IsTextSelected()) {
            flags |= CutCopyAvailableFlag;
         }
      },
      [&](WaveTrack *t) {
         flags |= WaveTracksExistFlag;
         flags |= PlayableTracksExistFlag;
         if (t->GetSelected()) {
            flags |= TracksSelectedFlag;
            // TODO: more-than-two-channels
            if (TrackList::Channels(t).size() > 1) {
               flags |= StereoRequiredFlag;
            }
            flags |= WaveTracksSelectedFlag;
            flags |= AudioTracksSelectedFlag;
         }
         if( t->GetEndTime() > t->GetStartTime() )
            flags |= HasWaveDataFlag;
      }
#if defined(USE_MIDI)
      ,
      [&](NoteTrack *nt) {
         flags |= NoteTracksExistFlag;
#ifdef EXPERIMENTAL_MIDI_OUT
         flags |= PlayableTracksExistFlag;
#endif

         if (nt->GetSelected()) {
            flags |= TracksSelectedFlag;
            flags |= NoteTracksSelectedFlag;
            flags |= AudioTracksSelectedFlag; // even if not EXPERIMENTAL_MIDI_OUT
         }
      }
#endif
   );

   if((AudacityProject::msClipT1 - AudacityProject::msClipT0) > 0.0)
      flags |= ClipboardFlag;

   auto &undoManager = UndoManager::Get( project );

   if (undoManager.UnsavedChanges() || !project.IsProjectSaved())
      flags |= UnsavedChangesFlag;

   if (!mLastEffect.empty())
      flags |= HasLastEffectFlag;

   if (project.UndoAvailable())
      flags |= UndoAvailableFlag;

   if (project.RedoAvailable())
      flags |= RedoAvailableFlag;

   if (ViewInfo::Get( project ).ZoomInAvailable() && (flags & TracksExistFlag))
      flags |= ZoomInAvailableFlag;

   if (ViewInfo::Get( project ).ZoomOutAvailable() && (flags & TracksExistFlag))
      flags |= ZoomOutAvailableFlag;

   // TextClipFlag is currently unused (Jan 2017, 2.1.3 alpha)
   // and LabelTrack::IsTextClipSupported() is quite slow on Linux,
   // so disable for now (See bug 1575).
   // if ((flags & LabelTracksExistFlag) && LabelTrack::IsTextClipSupported())
   //    flags |= TextClipFlag;

   flags |= GetFocusedFrame(project);

   const auto &playRegion = viewInfo.playRegion;
   if (playRegion.Locked())
      flags |= PlayRegionLockedFlag;
   else if (!playRegion.Empty())
      flags |= PlayRegionNotLockedFlag;

   if (flags & AudioIONotBusyFlag) {
      if (flags & TimeSelectedFlag) {
         if (flags & TracksSelectedFlag) {
            flags |= CutCopyAvailableFlag;
         }
      }
   }

   if (wxGetApp().GetRecentFiles()->GetCount() > 0)
      flags |= HaveRecentFiles;

   if (project.IsSyncLocked())
      flags |= IsSyncLockedFlag;
   else
      flags |= IsNotSyncLockedFlag;

   if (!EffectManager::Get().RealtimeIsActive())
      flags |= IsRealtimeNotActiveFlag;

      if (!window.IsCapturing())
      flags |= CaptureNotBusyFlag;

   ControlToolBar *bar = project.GetControlToolBar();
   if (bar->ControlToolBar::CanStopAudioStream())
      flags |= CanStopAudioStreamFlag;

   lastFlags = flags;
   return flags;
}

void MenuManager::ModifyAllProjectToolbarMenus()
{
   AProjectArray::iterator i;
   for (i = gAudacityProjects.begin(); i != gAudacityProjects.end(); ++i) {
      auto &project = **i;
      MenuManager::Get(project).ModifyToolbarMenus(project);
   }
}

void MenuManager::ModifyToolbarMenus(AudacityProject &project)
{
   // Refreshes can occur during shutdown and the toolmanager may already
   // be deleted, so protect against it.
   auto &toolManager = ToolManager::Get( project );

   auto &commandManager = CommandManager::Get( project );

   commandManager.Check(wxT("ShowScrubbingTB"),
                         toolManager.IsVisible(ScrubbingBarID));
   commandManager.Check(wxT("ShowDeviceTB"),
                         toolManager.IsVisible(DeviceBarID));
   commandManager.Check(wxT("ShowEditTB"),
                         toolManager.IsVisible(EditBarID));
   commandManager.Check(wxT("ShowMeterTB"),
                         toolManager.IsVisible(MeterBarID));
   commandManager.Check(wxT("ShowRecordMeterTB"),
                         toolManager.IsVisible(RecordMeterBarID));
   commandManager.Check(wxT("ShowPlayMeterTB"),
                         toolManager.IsVisible(PlayMeterBarID));
   commandManager.Check(wxT("ShowMixerTB"),
                         toolManager.IsVisible(MixerBarID));
   commandManager.Check(wxT("ShowSelectionTB"),
                         toolManager.IsVisible(SelectionBarID));
#ifdef EXPERIMENTAL_SPECTRAL_EDITING
   commandManager.Check(wxT("ShowSpectralSelectionTB"),
                         toolManager.IsVisible(SpectralSelectionBarID));
#endif
   commandManager.Check(wxT("ShowToolsTB"),
                         toolManager.IsVisible(ToolsBarID));
   commandManager.Check(wxT("ShowTranscriptionTB"),
                         toolManager.IsVisible(TranscriptionBarID));
   commandManager.Check(wxT("ShowTransportTB"),
                         toolManager.IsVisible(TransportBarID));

   // Now, go through each toolbar, and call EnableDisableButtons()
   for (int i = 0; i < ToolBarCount; i++) {
      toolManager.GetToolBar(i)->EnableDisableButtons();
   }

   // These don't really belong here, but it's easier and especially so for
   // the Edit toolbar and the sync-lock menu item.
   bool active;
   gPrefs->Read(wxT("/AudioIO/SoundActivatedRecord"),&active, false);
   commandManager.Check(wxT("SoundActivation"), active);
#ifdef EXPERIMENTAL_AUTOMATED_INPUT_LEVEL_ADJUSTMENT
   gPrefs->Read(wxT("/AudioIO/AutomatedInputLevelAdjustment"),&active, false);
   commandManager.Check(wxT("AutomatedInputLevelAdjustmentOnOff"), active);
#endif

   active = TracksPrefs::GetPinnedHeadPreference();
   commandManager.Check(wxT("PinnedHead"), active);

#ifdef EXPERIMENTAL_DA
   gPrefs->Read(wxT("/AudioIO/Duplex"),&active, false);
#else
   gPrefs->Read(wxT("/AudioIO/Duplex"),&active, true);
#endif
   commandManager.Check(wxT("Overdub"), active);
   gPrefs->Read(wxT("/AudioIO/SWPlaythrough"),&active, false);
   commandManager.Check(wxT("SWPlaythrough"), active);
   gPrefs->Read(wxT("/GUI/SyncLockTracks"), &active, false);
   project.SetSyncLock(active);
   commandManager.Check(wxT("SyncLock"), active);
   gPrefs->Read(wxT("/GUI/TypeToCreateLabel"),&active, true);
   commandManager.Check(wxT("TypeToCreateLabel"), active);
}

// checkActive is a temporary hack that should be removed as soon as we
// get multiple effect preview working
void MenuManager::UpdateMenus(AudacityProject &project, bool checkActive)
{
   //ANSWER-ME: Why UpdateMenus only does active project?
   //JKC: Is this test fixing a bug when multiple projects are open?
   //so that menu states work even when different in different projects?
   if (&project != GetActiveProject())
      return;

   auto flags = MenuManager::Get(project).GetUpdateFlags(project, checkActive);
   auto flags2 = flags;

   // We can enable some extra items if we have select-all-on-none.
   //EXPLAIN-ME: Why is this here rather than in GetUpdateFlags()?
   //ANSWER: Because flags2 is used in the menu enable/disable.
   //The effect still needs flags to determine whether it will need
   //to actually do the 'select all' to make the command valid.
   if (mWhatIfNoSelection != 0)
   {
      if ((flags & TracksExistFlag))
      {
         flags2 |= TracksSelectedFlag;
         if ((flags & WaveTracksExistFlag))
         {
            flags2 |= TimeSelectedFlag
                   |  WaveTracksSelectedFlag
                   |  CutCopyAvailableFlag;
         }
      }
   }

   if( mStopIfWasPaused )
   {
      if( flags & PausedFlag ){
         flags2 |= AudioIONotBusyFlag;
      }
   }

   // Return from this function if nothing's changed since
   // the last time we were here.
   if (flags == mLastFlags)
      return;
   mLastFlags = flags;

   auto &commandManager = CommandManager::Get( project );

   commandManager.EnableUsingFlags(flags2 , NoFlagsSpecified);

   // With select-all-on-none, some items that we don't want enabled may have
   // been enabled, since we changed the flags.  Here we manually disable them.
   // 0 is grey out, 1 is Autoselect, 2 is Give warnings.
   if (mWhatIfNoSelection != 0)
   {
      if (!(flags & TimeSelectedFlag) | !(flags & TracksSelectedFlag))
      {
         commandManager.Enable(wxT("SplitCut"), false);
         commandManager.Enable(wxT("SplitDelete"), false);
      }
      if (!(flags & WaveTracksSelectedFlag))
      {
         commandManager.Enable(wxT("Split"), false);
      }
      if (!(flags & TimeSelectedFlag) | !(flags & WaveTracksSelectedFlag))
      {
         commandManager.Enable(wxT("ExportSel"), false);
         commandManager.Enable(wxT("SplitNew"), false);
      }
      if (!(flags & TimeSelectedFlag) | !(flags & AudioTracksSelectedFlag))
      {
         commandManager.Enable(wxT("Trim"), false);
      }
   }

#if 0
   if (flags & CutCopyAvailableFlag) {
      GetCommandManager()->Enable(wxT("Copy"), true);
      GetCommandManager()->Enable(wxT("Cut"), true);
   }
#endif

   MenuManager::ModifyToolbarMenus(project);
}

/// The following method moves to the previous track
/// selecting and unselecting depending if you are on the start of a
/// block or not.

void MenuCreator::RebuildAllMenuBars()
{
   for( size_t i = 0; i < gAudacityProjects.size(); i++ ) {
      AudacityProject *p = gAudacityProjects[i].get();

      MenuManager::Get(*p).RebuildMenuBar(*p);
#if defined(__WXGTK__)
      // Workaround for:
      //
      //   http://bugzilla.audacityteam.org/show_bug.cgi?id=458
      //
      // This workaround should be removed when Audacity updates to wxWidgets 3.x which has a fix.
      wxRect r = p->GetRect();
      p->SetSize(wxSize(1,1));
      p->SetSize(r.GetSize());
#endif
   }
}

bool MenuManager::ReportIfActionNotAllowed
( AudacityProject &project,
  const wxString & Name, CommandFlag & flags, CommandFlag flagsRqd, CommandFlag mask )
{
   bool bAllowed = TryToMakeActionAllowed( project, flags, flagsRqd, mask );
   if( bAllowed )
      return true;
   auto &cm = CommandManager::Get( project );
   cm.TellUserWhyDisallowed( Name, flags & mask, flagsRqd & mask);
   return false;
}


/// Determines if flags for command are compatible with current state.
/// If not, then try some recovery action to make it so.
/// @return whether compatible or not after any actions taken.
bool MenuManager::TryToMakeActionAllowed
( AudacityProject &project,
  CommandFlag & flags, CommandFlag flagsRqd, CommandFlag mask )
{
   bool bAllowed;

   if( !flags )
      flags = MenuManager::Get(project).GetUpdateFlags(project);

   bAllowed = ((flags & mask) == (flagsRqd & mask));
   if( bAllowed )
      return true;

   // Why is action not allowed?
   // 1's wherever a required flag is missing.
   auto MissingFlags = (~flags & flagsRqd) & mask;

   if( mStopIfWasPaused && (MissingFlags & AudioIONotBusyFlag ) ){
      project.StopIfPaused();
      // Hope this will now reflect stopped audio.
      flags = MenuManager::Get(project).GetUpdateFlags(project);
      bAllowed = ((flags & mask) == (flagsRqd & mask));
      if( bAllowed )
         return true;
   }

   //We can only make the action allowed if we select audio when no selection.
   // IF not set up to select all audio when none, THEN return with failure.
   if( mWhatIfNoSelection != 1 )
      return false;

   // Some effects disallow autoselection.
   if( flagsRqd & NoAutoSelect )
      return false;

   // Why is action still not allowed?
   // 0's wherever a required flag is missing (or is don't care)
   MissingFlags = (flags & ~flagsRqd) & mask;

   // IF selecting all audio won't do any good, THEN return with failure.
   if( !(flags & WaveTracksExistFlag) )
      return false;
   // returns if mask wants a zero in some flag and that's not present.
   // logic seems a bit peculiar and worth revisiting.
   if( (MissingFlags & ~( TimeSelectedFlag | WaveTracksSelectedFlag)) )
      return false;

   // This was 'DoSelectSomething()'.  
   // This made autoselect more confusing.
   // When autoselect triggers, it might not select all audio in all tracks.
   // So changed to DoSelectAll.
   SelectActions::DoSelectAll(project);
   flags = MenuManager::Get(project).GetUpdateFlags(project);
   bAllowed = ((flags & mask) == (flagsRqd & mask));
   return bAllowed;
}
