/**********************************************************************

  Audacity: A Digital Audio Editor

  LoadCommands.cpp

  Dominic Mazzoni
  James Crook

**************************************************************************//**
\class BuiltinCommandsModule
\brief Internal module to auto register all built in commands.  It is closely
modelled on BuiltinEffectsModule
*****************************************************************************/

#include "../Audacity.h"
#include "LoadCommands.h"

#include "../Prefs.h"

#include "../MemoryX.h"

#include "../effects/EffectManager.h"
#include "Demo.h"

namespace {
const auto PathStart = "Commands";
static Registry::GroupItem &sRegistry()
{
   static Registry::GroupingItem registry{ PathStart };
   return registry;
}
}

struct BuiltinCommandsModule::Entry : Registry::SingleItem {
   Factory factory;
   Entry( const ComponentInterfaceSymbol &name, Factory factory_)
      : SingleItem( name.Internal() ), factory( factory_ ) {}
};

void BuiltinCommandsModule::DoRegistration(
   const ComponentInterfaceSymbol &name, const Factory &factory )
{
   Registry::RegisterItems( sRegistry(), { "" },
      std::make_unique< Entry >( name, factory ) );
}

// ============================================================================
// Module registration entry point
//
// This is the symbol that Audacity looks for when the module is built as a
// dynamic library.
//
// When the module is builtin to Audacity, we use the same function, but it is
// declared static so as not to clash with other builtin modules.
// ============================================================================
DECLARE_MODULE_ENTRY(AudacityModule)
{
   // Create and register the importer
   // Trust the module manager not to leak this
   return safenew BuiltinCommandsModule(moduleManager, path);
}

// ============================================================================
// Register this as a builtin module
// ============================================================================
DECLARE_BUILTIN_MODULE(BuiltinsCommandBuiltin);

///////////////////////////////////////////////////////////////////////////////
//
// BuiltinCommandsModule
//
///////////////////////////////////////////////////////////////////////////////

BuiltinCommandsModule::BuiltinCommandsModule(ModuleManagerInterface *moduleManager,
                                           const wxString *path)
{
   mModMan = moduleManager;
   if (path)
   {
      mPath = *path;
   }
}

BuiltinCommandsModule::~BuiltinCommandsModule()
{
   mPath.clear();
}

// ============================================================================
// ComponentInterface implementation
// ============================================================================

PluginPath BuiltinCommandsModule::GetPath()
{
   return mPath;
}

ComponentInterfaceSymbol BuiltinCommandsModule::GetSymbol()
{
   return XO("Builtin Commands");
}

VendorSymbol BuiltinCommandsModule::GetVendor()
{
   return XO("The Audacity Team");
}

wxString BuiltinCommandsModule::GetVersion()
{
   // This "may" be different if this were to be maintained as a separate DLL
   return AUDACITY_VERSION_STRING;
}

wxString BuiltinCommandsModule::GetDescription()
{
   return _("Provides builtin commands to Audacity");
}

// ============================================================================
// ModuleInterface implementation
// ============================================================================

bool BuiltinCommandsModule::Initialize()
{
   struct Visitor : Registry::Visitor{
      CommandHash *pCommands;
      void Visit( Registry::SingleItem& item, const wxArrayString& ) override
      {
         auto pEntry = static_cast< Entry* >( &item );
         auto path = wxString(BUILTIN_GENERIC_COMMAND_PREFIX) + pEntry->name;
         (*pCommands)[ path ] = pEntry;
      }
   } visitor;
   visitor.pCommands = &mCommands;
   Registry::GroupingItem top{ PathStart };
   Registry::Visit( visitor, &top, &sRegistry() );
   return true;
}

void BuiltinCommandsModule::Terminate()
{
   // Nothing to do here
   return;
}

const FileExtensions &BuiltinCommandsModule::GetFileExtensions()
{
   static FileExtensions empty;
   return empty;
}

bool BuiltinCommandsModule::AutoRegisterPlugins(PluginManagerInterface & pm)
{
   wxString ignoredErrMsg;
   for (const auto &pair : mCommands)
   {
      const auto &path = pair.first;
      if (!pm.IsPluginRegistered(path))
      {
         // No checking of error ?
         // Uses Generic Registration, not Default.
         // Registers as TypeGeneric, not TypeEffect.
         DiscoverPluginsAtPath(path, ignoredErrMsg,
            PluginManagerInterface::AudacityCommandRegistrationCallback);
      }
   }

   // We still want to be called during the normal registration process
   return false;
}

PluginPaths BuiltinCommandsModule::FindPluginPaths(PluginManagerInterface & WXUNUSED(pm))
{
   PluginPaths names;
   for ( const auto &pair : mCommands )
      names.push_back( pair.first );
   return names;
}

unsigned BuiltinCommandsModule::DiscoverPluginsAtPath(
   const PluginPath & path, wxString &errMsg,
   const RegistrationCallback &callback)
{
   errMsg.clear();
   auto Command = Instantiate(path);
   if (Command)
   {
      if (callback){
         callback(this, Command.get());
      }
      return 1;
   }

   errMsg = _("Unknown built-in command name");
   return 0;
}

bool BuiltinCommandsModule::IsPluginValid(const PluginPath & path, bool bFast)
{
   // bFast is unused as checking in the list is fast.
   static_cast<void>(bFast); // avoid unused variable warning
   return mCommands.find( path ) != mCommands.end();
}

ComponentInterface *BuiltinCommandsModule::CreateInstance(const PluginPath & path)
{
   // Acquires a resource for the application.
   // Safety of this depends on complementary calls to DeleteInstance on the module manager side.
   return Instantiate(path).release();
}

void BuiltinCommandsModule::DeleteInstance(ComponentInterface *instance)
{
   // Releases the resource.
   std::unique_ptr < AudacityCommand > {
      dynamic_cast<AudacityCommand *>(instance)
   };
}

// ============================================================================
// BuiltinCommandsModule implementation
// ============================================================================

std::unique_ptr<AudacityCommand> BuiltinCommandsModule::Instantiate(const PluginPath & path)
{
   wxASSERT(path.StartsWith(BUILTIN_GENERIC_COMMAND_PREFIX));
   auto iter = mCommands.find( path );
   if ( iter != mCommands.end() )
      return iter->second->factory();

   wxASSERT( false );
   return nullptr;
}
