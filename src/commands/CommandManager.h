/**********************************************************************

  Audacity: A Digital Audio Editor

  CommandManager.h

  Brian Gunlogson
  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_COMMAND_MANAGER__
#define __AUDACITY_COMMAND_MANAGER__

#include "../Experimental.h"

#include "audacity/Types.h"

#include "../ClientData.h"
#include "CommandFunctors.h"
#include "CommandFlag.h"

#include "../MemoryX.h"
#include "Keyboard.h"
#include <vector>
#include <wx/hashmap.h>

#include "../xml/XMLTagHandler.h"

#include "audacity/Types.h"

#include <unordered_map>

class wxMenu;
class wxMenuBar;
class wxArrayString;
class TranslatedInternalString;
using CommandParameter = CommandID;

struct MenuBarListEntry
{
   MenuBarListEntry(const wxString &name_, wxMenuBar *menubar_);
   ~MenuBarListEntry();

   wxString name;
   wxWeakRef<wxMenuBar> menubar; // This structure does not assume memory ownership!
};

struct SubMenuListEntry
{
   SubMenuListEntry(const wxString &name_, std::unique_ptr<wxMenu> &&menu_);
   SubMenuListEntry(SubMenuListEntry &&that);
   ~SubMenuListEntry();

   wxString name;
   std::unique_ptr<wxMenu> menu;
};

struct CommandListEntry
{
   int id;
   CommandID name;
   wxString longLabel;
   NormalizedKeyString key;
   NormalizedKeyString defaultKey;
   wxString label;
   wxString labelPrefix;
   wxString labelTop;
   wxMenu *menu;
   CommandHandlerFinder finder;
   CommandFunctorPointer callback;
   CommandParameter parameter;
   bool multi;
   int index;
   int count;
   bool enabled;
   bool skipKeydown;
   bool wantKeyup;
   bool isGlobal;
   bool isOccult;
   bool isEffect;
   bool hasDialog;
   CommandFlag flags;
   CommandMask mask;
};

using MenuBarList = std::vector < MenuBarListEntry >;

// to do: remove the extra indirection when Mac compiler moves to newer version
using SubMenuList = std::vector < std::unique_ptr<SubMenuListEntry> >;

// This is an array of pointers, not structures, because the hash maps also point to them,
// so we don't want the structures to relocate with vector operations.
using CommandList = std::vector<std::unique_ptr<CommandListEntry>>;

namespace std
{
   template<> struct hash< NormalizedKeyString > {
      size_t operator () (const NormalizedKeyString &str) const // noexcept
      {
         auto &stdstr = str.Raw(); // no allocations, a cheap fetch
         using Hasher = std::hash< wxString >;
         return Hasher{}( stdstr );
      }
   };
}

using CommandKeyHash = std::unordered_map<NormalizedKeyString, CommandListEntry*>;
using CommandNameHash = std::unordered_map<wxString, CommandListEntry*>;
using CommandNumericIDHash = std::unordered_map<int, CommandListEntry*>;

class AudacityProject;
class CommandContext;

class AUDACITY_DLL_API CommandManager final
   : public XMLTagHandler
   , public ClientData::Base
{
 public:
   static CommandManager &Get( AudacityProject &project );
   static const CommandManager &Get( const AudacityProject &project );

   //
   // Constructor / Destructor
   //

   CommandManager();
   virtual ~CommandManager();

   CommandManager(const CommandManager&) PROHIBITED;
   CommandManager &operator= (const CommandManager&) PROHIBITED;

   void SetMaxList();
   void PurgeData();

   //
   // Creating menus and adding commands
   //

   std::unique_ptr<wxMenuBar> AddMenuBar(const wxString & sMenu);

   // You may either called SetCurrentMenu later followed by ClearCurrentMenu,
   // or else BeginMenu followed by EndMenu.  Don't mix them.
   wxMenu *BeginMenu(const wxString & tName);
   void EndMenu();

   // For specifying unusual arguments in AddItem
   struct Options
   {
      // type of a function that determines checkmark state
      using CheckFn = std::function< bool() >;

      Options() {}
      // Allow implicit construction from an accelerator string, which is
      // a very common case
      Options( const wxChar *accel_ ) : accel{ accel_ } {}
      // A two-argument constructor for another common case
      Options(
         const wxChar *accel_,
         const wxString &longName_ /* usually untranslated */ )
      : accel{ accel_ }, longName{ longName_ } {}

      Options &&Accel (const wxChar *value) &&
         { accel = value; return std::move(*this); }
      Options &&IsEffect (bool value = true) &&
         { bIsEffect = value; return std::move(*this); }
      Options &&Parameter (const CommandParameter &value) &&
         { parameter = value; return std::move(*this); }
      Options &&Mask (CommandMask value) &&
         { mask = value; return std::move(*this); }
      Options &&LongName (const wxString &value /* usually untranslated */ ) &&
         { longName = value; return std::move(*this); }
      Options &&IsGlobal () &&
         { global = true; return std::move(*this); }
      Options &&IsTranslated ( bool value = true ) &&
         { translated = value; return std::move(*this); }
      // Affirm that the command has a dialog, regardless of the name:
      Options &&Interactive ( bool value = true ) &&
         { interactive = value; return std::move(*this); }

      // Specify a constant check state
      Options &&CheckState (bool value) && {
         checker = value
            ? [](){ return true; }
            : [](){ return false; }
           ;
           return std::move(*this);
      }
      // CheckTest is overloaded
      // Take arbitrary predicate
      Options &&CheckTest (const CheckFn &fn) &&
         { checker = fn; return std::move(*this); }
      // Take a preference path
      Options &&CheckTest (const wxChar *key, bool defaultValue) && {
         checker = MakeCheckFn( key, defaultValue );
         return std::move(*this);
      }

      const wxChar *accel{ wxT("") };
      CheckFn checker; // default value means it's not a check item
      bool bIsEffect{ false };
      CommandParameter parameter{};
      CommandMask mask{ NoFlagsSpecified };
      wxString longName{}; // usually untranslated
      bool global{ false };
      bool translated{ false };
      // If defaulted, deduce whether there is a dialog from ellipsis in the
      // name:
      bool interactive{ false };

   private:
      static CheckFn MakeCheckFn( const wxString key, bool defaultValue );
   };

   void AddItemList(const CommandID & name,
                    const ComponentInterfaceSymbol items[],
                    size_t nItems,
                    CommandHandlerFinder finder,
                    CommandFunctorPointer callback,
                    CommandFlag flags,
                    bool bIsEffect = false);

   void AddItem(const CommandID & name,
                const wxChar *label_in, // untranslated
                CommandHandlerFinder finder,
                CommandFunctorPointer callback,
                CommandFlag flags,
                const Options &options = {});

   void AddSeparator();

   // A command doesn't actually appear in a menu but might have a
   // keyboard shortcut.
   void AddCommand(const CommandID &name,
                   const wxChar *label,
                   CommandHandlerFinder finder,
                   CommandFunctorPointer callback,
                   CommandFlag flags,
                   const Options &options = {});

   void PopMenuBar();
   void BeginOccultCommands();
   void EndOccultCommands();


   void SetCommandFlags(const CommandID &name, CommandFlag flags, CommandMask mask);

   //
   // Modifying menus
   //

   void EnableUsingFlags(CommandFlag flags, CommandMask mask);
   void Enable(const wxString &name, bool enabled);
   void Check(const CommandID &name, bool checked);
   void Modify(const wxString &name, const wxString &newLabel);

   // You may either called SetCurrentMenu later followed by ClearCurrentMenu,
   // or else BeginMenu followed by EndMenu.  Don't mix them.
   void SetCurrentMenu(wxMenu * menu);
   void ClearCurrentMenu();

   //
   // Modifying accelerators
   //

   void SetKeyFromName(const CommandID &name, const NormalizedKeyString &key);
   void SetKeyFromIndex(int i, const NormalizedKeyString &key);

   //
   // Executing commands
   //

   // "permit" allows filtering even if the active window isn't a child of the project.
   // Lyrics and MixerTrackCluster classes use it.
   bool FilterKeyEvent(AudacityProject *project, const wxKeyEvent & evt, bool permit = false);
   bool HandleMenuID(int id, CommandFlag flags, CommandMask mask);
   bool HandleTextualCommand(const CommandID & Str, const CommandContext & context, CommandFlag flags, CommandMask mask);

   //
   // Accessing
   //

   void GetCategories(wxArrayString &cats);
   void GetAllCommandNames(CommandIDs &names, bool includeMultis) const;
   void GetAllCommandLabels(
      wxArrayString &labels, std::vector<bool> &vHasDialog,
      bool includeMultis) const;
   void GetAllCommandData(
      CommandIDs &names,
      std::vector<NormalizedKeyString> &keys,
      std::vector<NormalizedKeyString> &default_keys,
      wxArrayString &labels, wxArrayString &categories,
#if defined(EXPERIMENTAL_KEY_VIEW)
      wxArrayString &prefixes,
#endif
      bool includeMultis);

   // Each command is assigned a numerical ID for use in wxMenu and wxEvent,
   // which need not be the same across platforms or sessions
   CommandID GetNameFromNumericID( int id );

   wxString GetLabelFromName(const CommandID &name);
   wxString GetPrefixedLabelFromName(const CommandID &name);
   wxString GetCategoryFromName(const CommandID &name);
   NormalizedKeyString GetKeyFromName(const CommandID &name) const;
   NormalizedKeyString GetDefaultKeyFromName(const CommandID &name);

   bool GetEnabled(const CommandID &name);

#if defined(__WXDEBUG__)
   void CheckDups();
#endif

   //
   // Loading/Saving
   //

   void WriteXML(XMLWriter &xmlFile) const /* not override */;
   void TellUserWhyDisallowed(const wxString & Name, CommandFlag flagsGot, CommandFlag flagsRequired);

   ///
   /// Formatting summaries that include shortcut keys
   ///
   wxString DescribeCommandsAndShortcuts
   (
       // If a shortcut key is defined for the command, then it is appended,
       // parenthesized, after the translated name.
       const TranslatedInternalString commands[], size_t nCommands) const;

   // Sorted list of the shortcut keys to be exluded from the standard defaults
   static const std::vector<NormalizedKeyString> &ExcludedList();

private:

   //
   // Creating menus and adding commands
   //

   int NextIdentifier(int ID);
   CommandListEntry *NewIdentifier(const CommandID & name,
                                   const wxString & label,
                                   wxMenu *menu,
                                   CommandHandlerFinder finder,
                                   CommandFunctorPointer callback,
                                   const CommandID &nameSuffix,
                                   int index,
                                   int count,
                                   const Options &options);
   
   void AddGlobalCommand(const CommandID &name,
                         const wxChar *label, // untranslated
                         CommandHandlerFinder finder,
                         CommandFunctorPointer callback,
                         const Options &options = {});

   //
   // Executing commands
   //

   bool HandleCommandEntry(const CommandListEntry * entry, CommandFlag flags, CommandMask mask, const wxEvent * evt = NULL);

   //
   // Modifying
   //

   void Enable(CommandListEntry *entry, bool enabled);
   wxMenu *BeginMainMenu(const wxString & tName);
   void EndMainMenu();
   wxMenu* BeginSubMenu(const wxString & tName);
   void EndSubMenu();

   //
   // Accessing
   //

   wxMenuBar * CurrentMenuBar() const;
   wxMenuBar * GetMenuBar(const wxString & sMenu) const;
   wxMenu * CurrentSubMenu() const;
public:
   wxMenu * CurrentMenu() const;
private:
   wxString GetLabel(const CommandListEntry *entry) const;
   wxString GetLabelWithDisabledAccel(const CommandListEntry *entry) const;

   //
   // Loading/Saving
   //

   bool HandleXMLTag(const wxChar *tag, const wxChar **attrs) override;
   void HandleXMLEndTag(const wxChar *tag) override;
   XMLTagHandler *HandleXMLChild(const wxChar *tag) override;

private:
   // mMaxList only holds shortcuts that should not be added (by default)
   // and is sorted.
   std::vector<NormalizedKeyString> mMaxListOnly;

   MenuBarList  mMenuBarList;
   SubMenuList  mSubMenuList;
   CommandList  mCommandList;
   CommandNameHash  mCommandNameHash;
   CommandKeyHash mCommandKeyHash;
   CommandNumericIDHash  mCommandNumericIDHash;
   int mCurrentID;
   int mXMLKeysRead;

   bool mbSeparatorAllowed; // false at the start of a menu and immediately after a separator.

   wxString mCurrentMenuName;
   std::unique_ptr<wxMenu> uCurrentMenu;
   wxMenu *mCurrentMenu {};

   bool bMakingOccultCommands;
   std::unique_ptr< wxMenuBar > mTempMenuBar;
};

// Define classes and functions that associate parts of the user interface
// with path names
namespace Registry {

   // Items in the registry form an unordered tree, but each may also describe a
   // desired insertion point among its peers.  The request might not be honored
   // (as when the other name is not found, or when more than one item requests
   // the same ordering), but this is not treated as an error.
   struct OrderingHint
   {
      // The default Unspecified hint is just like End, except that in case the
      // item is delegated to by a SharedItem or ComputedItem, the delegating
      // item's hint will be used instead
      enum Type : int {
         Unspecified,
         Begin, End, Before, After
      } type{ Unspecified };

      // name of some other BaseItem; significant only when type is Before or
      // After:
      wxString name;

      OrderingHint() {}
      OrderingHint( Type type_, const wxString &name_ = {} )
         : type(type_), name(name_) {}
   };

   // TODO C++17: maybe use std::variant (discriminated unions) to achieve
   // polymorphism by other means, not needing unique_ptr and dynamic_cast
   // and using less heap.
   // Most items in the table will be the large ones describing commands, so the
   // waste of space in unions for separators and sub-menus should not be
   // large.
   struct BaseItem {
      // declare at least one virtual function so dynamic_cast will work
      explicit
      BaseItem( const wxString &internalName )
         : name{ internalName }
      {}
      virtual ~BaseItem();

      const wxString name;

      OrderingHint orderingHint;
   };
   using BaseItemPtr = std::unique_ptr<BaseItem>;
   using BaseItemSharedPtr = std::shared_ptr<BaseItem>;
   using BaseItemPtrs = std::vector<BaseItemPtr>;
   

   // An item that delegates to another held in a shared pointer; this allows
   // static tables of items to be computed once and reused
   // The name of the delegate is significant for path calculations, but the
   // SharedItem's ordering hint is used if the delegate has none
   struct SharedItem final : BaseItem {
      explicit SharedItem( const BaseItemSharedPtr &ptr_ )
         : BaseItem{ wxEmptyString }
         , ptr{ ptr_ }
      {}
      ~SharedItem() override;

      BaseItemSharedPtr ptr;
   };

   // A convenience function
   inline std::unique_ptr<SharedItem> Shared( const BaseItemSharedPtr &ptr )
      { return std::make_unique<SharedItem>( ptr ); }

   // An item that computes some other item to substitute for it, each time
   // the ComputedItem is visited
   // The name of the substitute is significant for path calculations, but the
   // ComputedItem's ordering hint is used if the substitute has none
   struct ComputedItem final : BaseItem {
      // The type of functions that generate descriptions of items.
      // Return type is a shared_ptr to let the function decide whether to
      // recycle the object or rebuild it on demand each time.
      // Return value from the factory may be null
      using Factory = std::function< BaseItemSharedPtr( void * ) >;

      explicit ComputedItem( const Factory &factory_ )
         : BaseItem( wxEmptyString )
         , factory{ factory_ }
      {}
      ~ComputedItem() override;

      Factory factory;
   };

   // Common abstract base class for items that are not groups
   struct SingleItem : BaseItem {
      using BaseItem::BaseItem;
      ~SingleItem() override = 0;
   };

   // Common abstract base class for items that group other items
   struct GroupItem : BaseItem {
      // Construction from an internal name and a previously built-up
      // vector of pointers
      GroupItem( const wxString &internalName, BaseItemPtrs &&items_ );
      // In-line, variadic constructor that doesn't require building a vector
      template< typename... Args >
         GroupItem( const wxString &internalName, Args&&... args )
         : BaseItem( internalName )
         { Append( std::forward< Args >( args )... ); }
      ~GroupItem() override = 0;

      BaseItemPtrs items;

   private:
      // nullary overload grounds the recursion
      void Append() {}
      // recursive overload
      template< typename Arg, typename... Args >
         void Append( Arg &&arg, Args&&... moreArgs )
         {
            // Dispatch one argument to the proper overload of AppendOne.
            // std::forward preserves rvalue/lvalue distinction of the actual
            // argument of the constructor call; that is, it inserts a
            // std::move() if and only if the original argument is rvalue
            AppendOne( std::forward<Arg>( arg ) );
            // recur with the rest of the arguments
            Append( std::forward<Args>(moreArgs)... );
         };

      // Move one unique_ptr to an item into our array
      void AppendOne( BaseItemPtr&& ptr );
      // This overload allows a lambda or function pointer in the variadic
      // argument lists without any other syntactic wrapping, and also
      // allows implicit conversions to type Factory.
      // (Thus, a lambda can return a unique_ptr<BaseItem> rvalue even though
      // Factory's return type is shared_ptr, and the needed conversion is
      // appled implicitly.)
      void AppendOne( const ComputedItem::Factory &factory )
      { AppendOne( std::make_unique<ComputedItem>( factory ) ); }
      // This overload lets you supply a shared pointer to an item, directly
      template<typename Subtype>
      void AppendOne( const std::shared_ptr<Subtype> &ptr )
      { AppendOne( std::make_unique<SharedItem>(ptr) ); }
   };

   // Concrete subclass of GroupItem that adds nothing else
   // GroupingItem with an empty name is transparent to item path calculations
   // and propagates its ordering hint if subordinates don't specify hints
   struct GroupingItem final : GroupItem
   {
      using GroupItem::GroupItem;
      ~GroupingItem() override;
   };

   // inline convenience function:

   // Group items can be constructed two ways.
   // Pointers to subordinate items are moved into the result.
   // Null pointers are permitted, and ignored when building the menu.
   // Items are spliced into the enclosing menu.
   // The name is untranslated and may be empty, to make the group transparent
   // in identification of items by path.  Otherwise try to keep the name
   // stable across Audacity versions.
   template< typename... Args >
   inline std::unique_ptr<GroupingItem> Items(
      const wxString &internalName, Args&&... args )
         { return std::make_unique<GroupingItem>( internalName,
            std::forward<Args>(args)... ); }

   // The /-separated path is relative to the GroupItem supplied to
   // RegisterItems.
   // For instance, wxT("Transport/Cursor") to locate an item under a sub-menu
   // of a main menu
   struct Placement {
      wxString path;
      OrderingHint hint;

      Placement( const wxString &path_, const OrderingHint &hint_ = {} )
         : path( path_ ), hint( hint_ )
      {}
   };

   void RegisterItems( GroupItem &registry, const Placement &placement,
      BaseItemPtr &&pItem );
   
   // Define actions to be done in Visit.
   // Default implementations do nothing
   // The supplied path does not include the name of the item
   class Visitor
   {
   public:
      virtual ~Visitor();
      virtual void BeginGroup( GroupItem &item, const wxArrayString &path );
      virtual void EndGroup( GroupItem &item, const wxArrayString &path );
      virtual void Visit( SingleItem &item, const wxArrayString &path );
   };

   // Top-down visitation of all items and groups in a tree rooted in
   // pTopItem, as merged with pRegistry.
   // The merger of the trees is recomputed in each call, not saved.
   // So neither given tree is modified.
   // But there may be a side effect on preferences to remember the ordering
   // imposed on each node of the unordered tree of registered items; each item
   // seen in the registry for the first time is placed somehere, and that
   // ordering should be kept the same thereafter in later runs (which may add
   // yet other previously unknown items).
   // context argument is passed to ComputedItem functors
   void Visit(
      Visitor &visitor,
      BaseItem *pTopItem,
      GroupItem *pRegistry = nullptr, void *context = nullptr );
}

// Define items that populate tables that specifically describe menu trees
namespace MenuTable {
   using namespace Registry;

   // Describes a main menu in the toolbar, or a sub-menu
   struct MenuItem final : GroupItem {
      // Construction from an internal name and a previously built-up
      // vector of pointers
      MenuItem( const wxString &internalName,
         const wxString &title_, BaseItemPtrs &&items_ );
      // In-line, variadic constructor that doesn't require building a vector
      template< typename... Args >
         MenuItem( const wxString &internalName,
            const wxString &title_ /* usually untranslated */, Args&&... args )
            : GroupItem{ internalName, std::forward<Args>(args)... }
            , title{ title_ }
         {}
      ~MenuItem() override;

      wxString title; // translated
      bool translated{ false };
   };

   // Collects other items that are conditionally shown or hidden, but are
   // always available to macro programming
   struct ConditionalGroupItem final : GroupItem {
      using Condition = std::function< bool() >;

      // Construction from an internal name and a previously built-up
      // vector of pointers
      ConditionalGroupItem( const wxString &internalName,
         Condition condition_, BaseItemPtrs &&items_ );
      // In-line, variadic constructor that doesn't require building a vector
      template< typename... Args >
         ConditionalGroupItem( const wxString &internalName,
            Condition condition_, Args&&... args )
            : GroupItem{ internalName, std::forward<Args>(args)... }
            , condition{ condition_ }
         {}
      ~ConditionalGroupItem() override;

      Condition condition;
   };

   // Describes a separator between menu items
   struct SeparatorItem final : SingleItem
   {
      SeparatorItem() : SingleItem{ wxEmptyString } {}
      ~SeparatorItem() override;
   };

   // usage:
   //   auto scope = FinderScope( findCommandHandler );
   //   return Items( ... );
   //
   // or:
   //   return FinderScope( findCommandHandler )
   //      .Eval( Items( ... ) );
   //
   // where findCommandHandler names a function.
   // This is used before a sequence of many calls to Command() and
   // CommandGroup(), so that the finder argument need not be specified
   // in each call.
   class FinderScope : ValueRestorer< CommandHandlerFinder >
   {
      static CommandHandlerFinder sFinder;

   public:
      static CommandHandlerFinder DefaultFinder() { return sFinder; }

      explicit
      FinderScope( CommandHandlerFinder finder )
         : ValueRestorer( sFinder, finder )
      {}

      // See usage comment above about this pass-through function
      template< typename Value > Value&& Eval( Value &&value ) const
         { return std::forward<Value>(value); }
   };

   // Describes one command in a menu
   struct CommandItem final : SingleItem {
      CommandItem(const CommandID &name_,
               const wxString &label_in_, // untranslated
               CommandHandlerFinder finder_,
               CommandFunctorPointer callback_,
               CommandFlag flags_,
               const CommandManager::Options &options_);

      // Takes a pointer to member function directly, and delegates to the
      // previous constructor; useful within the lifetime of a FinderScope
      template< typename Handler >
      CommandItem(const wxString &name_,
               const wxString &label_in_, // untranslated
               void (Handler::*pmf)(const CommandContext&),
               CommandFlag flags_,
               const CommandManager::Options &options_,
               CommandHandlerFinder finder = FinderScope::DefaultFinder())
         : CommandItem(name_, label_in_,
            finder, static_cast<CommandFunctorPointer>(pmf),
            flags_, options_)
      {}

      ~CommandItem() override;

      const wxString label_in; // untranslated
      CommandHandlerFinder finder;
      CommandFunctorPointer callback;
      CommandFlag flags;
      CommandManager::Options options;
   };

   // Describes several successive commands in a menu that are closely related
   // and dispatch to one common callback, which will be passed a number
   // in the CommandContext identifying the command
   struct CommandGroupItem final : SingleItem {
      CommandGroupItem(const wxString &name_,
               std::initializer_list< ComponentInterfaceSymbol > items_,
               CommandHandlerFinder finder_,
               CommandFunctorPointer callback_,
               CommandFlag flags_,
               bool isEffect_);

      // Takes a pointer to member function directly, and delegates to the
      // previous constructor; useful within the lifetime of a FinderScope
      template< typename Handler >
      CommandGroupItem(const wxString &name_,
               std::initializer_list< ComponentInterfaceSymbol > items_,
               void (Handler::*pmf)(const CommandContext&),
               CommandFlag flags_,
               bool isEffect_,
               CommandHandlerFinder finder = FinderScope::DefaultFinder())
         : CommandGroupItem(name_, items_,
            finder, static_cast<CommandFunctorPointer>(pmf),
            flags_, isEffect_)
      {}

      ~CommandGroupItem() override;

      const std::vector<ComponentInterfaceSymbol> items;
      CommandHandlerFinder finder;
      CommandFunctorPointer callback;
      CommandFlag flags;
      bool isEffect;
   };

   // For manipulating the enclosing menu or sub-menu directly,
   // adding any number of items, not using the CommandManager
   struct SpecialItem final : SingleItem
   {
      using Appender = std::function< void( AudacityProject&, wxMenu& ) >;

      explicit SpecialItem( const wxString &internalName, const Appender &fn_ )
      : SingleItem{ internalName }
      , fn{ fn_ }
      {}
      ~SpecialItem() override;

      Appender fn;
   };

   // The following, and Shared() and Items(), are the functions to use directly
   // in writing table definitions.

   // Menu items can be constructed two ways, as for group items
   // Items will appear in a main toolbar menu or in a sub-menu.
   // The name is untranslated.  Try to keep the name stable across Audacity
   // versions.
   // If the name of a menu is empty, then subordinate items cannot be located
   // by path.
   template< typename... Args >
   inline std::unique_ptr<MenuItem> Menu(
      const wxString &internalName, const wxString &title, Args&&... args )
         { return std::make_unique<MenuItem>(
            internalName, title, std::forward<Args>(args)... ); }
   inline std::unique_ptr<MenuItem> Menu(
      const wxString &internalName, const wxString &title, BaseItemPtrs &&items )
         { return std::make_unique<MenuItem>(
            internalName, title, std::move( items ) ); }

   // Conditional group items can be constructed two ways, as for group items
   // These items register in the CommandManager but are not shown in menus
   // if the condition evaluates false.
   // The name is untranslated.  Try to keep the name stable across Audacity
   // versions.
   // Name for conditional group must be non-empty.
   template< typename... Args >
   inline std::unique_ptr<ConditionalGroupItem> ConditionalItems(
      const wxString &internalName,
      ConditionalGroupItem::Condition condition, Args&&... args )
         { return std::make_unique<ConditionalGroupItem>(
            internalName, condition, std::forward<Args>(args)... ); }
   inline std::unique_ptr<ConditionalGroupItem> ConditionalItems(
      const wxString &internalName, ConditionalGroupItem::Condition condition,
      BaseItemPtrs &&items )
         { return std::make_unique<ConditionalGroupItem>(
            internalName, condition, std::move( items ) ); }

   // Make either a menu item or just a group, depending on the nonemptiness
   // of the title.
   // The name is untranslated and may be empty, to make the group transparent
   // in identification of items by path.  Otherwise try to keep the name
   // stable across Audacity versions.
   // If the name of a menu is empty, then subordinate items cannot be located
   // by path.
   template< typename... Args >
   inline BaseItemPtr MenuOrItems(
      const wxString &internalName, const wxString &title, Args&&... args )
         {  if ( title.empty() )
               return Items( internalName, std::forward<Args>(args)... );
            else
               return std::make_unique<MenuItem>(
                  internalName, title, std::forward<Args>(args)... ); }
   inline BaseItemPtr MenuOrItems(
      const wxString &internalName,
      const wxString &title, BaseItemPtrs &&items )
         {  if ( title.empty() )
               return Items( internalName, std::move( items ) );
            else
               return std::make_unique<MenuItem>(
                  internalName, title, std::move( items ) ); }

   inline std::unique_ptr<SeparatorItem> Separator()
      { return std::make_unique<SeparatorItem>(); }

   template< typename Handler >
   inline std::unique_ptr<CommandItem> Command(
      const CommandID &name,
      const wxString &label_in, // untranslated
      void (Handler::*pmf)(const CommandContext&),
      CommandFlag flags, const CommandManager::Options &options = {},
      CommandHandlerFinder finder = FinderScope::DefaultFinder())
   {
      return std::make_unique<CommandItem>(
         name, label_in, pmf, flags, options, finder
      );
   }

   template< typename Handler >
   inline std::unique_ptr<CommandGroupItem> CommandGroup(
      const wxString &name,
      std::initializer_list< ComponentInterfaceSymbol > items,
      void (Handler::*pmf)(const CommandContext&),
      CommandFlag flags, bool isEffect = false,
      CommandHandlerFinder finder = FinderScope::DefaultFinder())
   {
      return std::make_unique<CommandGroupItem>(
         name, items, pmf, flags, isEffect, finder
      );
   }

   inline std::unique_ptr<SpecialItem> Special(
      const wxString &name, const SpecialItem::Appender &fn )
         { return std::make_unique<SpecialItem>( name, fn ); }

   // Typically you make a static object of this type in the .cpp file that
   // also defines the added menu actions.
   // pItem can be specified by an expression using the inline functions above.
   struct AttachedItem final
   {
      AttachedItem( const Placement &placement, BaseItemPtr &&pItem );

      AttachedItem( const wxString &path, BaseItemPtr &&pItem )
         // Delegating constructor
         : AttachedItem( Placement{ path }, std::move( pItem ) )
      {}
   };

}

#endif
