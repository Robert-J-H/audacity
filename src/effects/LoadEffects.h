/**********************************************************************

  Audacity: A Digital Audio Editor

  LoadEffects.h

  Dominic Mazzoni

**********************************************************************/

#include "audacity/ModuleInterface.h"

#include <functional>
#include <memory>
#include "../MemoryX.h"
#include "../commands/CommandManager.h"

class Effect;

///////////////////////////////////////////////////////////////////////////////
//
// BuiltinEffectsModule
//
///////////////////////////////////////////////////////////////////////////////

class BuiltinEffectsModule final : public ModuleInterface
{
public:
   BuiltinEffectsModule(ModuleManagerInterface *moduleManager, const wxString *path);
   virtual ~BuiltinEffectsModule();

   using Factory = std::function< std::unique_ptr<Effect> () >;

   // Typically you make a static object of this type in the .cpp file that
   // also implements the Effect subclass.
   template< typename Subclass >
   struct Registration final { Registration( bool excluded = false ) {
      DoRegistration(
         Subclass::Symbol, [](){ return std::make_unique< Subclass >(); },
         excluded );
   } };

   // ComponentInterface implementation

   PluginPath GetPath() override;
   ComponentInterfaceSymbol GetSymbol() override;
   VendorSymbol GetVendor() override;
   wxString GetVersion() override;
   wxString GetDescription() override;

   // ModuleInterface implementation

   bool Initialize() override;
   void Terminate() override;

   const FileExtensions &GetFileExtensions() override;
   FilePath InstallPath() override { return {}; }

   bool AutoRegisterPlugins(PluginManagerInterface & pm) override;
   PluginPaths FindPluginPaths(PluginManagerInterface & pm) override;
   unsigned DiscoverPluginsAtPath(
      const PluginPath & path, wxString &errMsg,
      const RegistrationCallback &callback)
         override;

   bool IsPluginValid(const PluginPath & path, bool bFast) override;

   ComponentInterface *CreateInstance(const PluginPath & path) override;
   void DeleteInstance(ComponentInterface *instance) override;

private:
   // BuiltinEffectModule implementation

   std::unique_ptr<Effect> Instantiate(const PluginPath & path);

private:
   struct Entry;

   static void DoRegistration(
      const ComponentInterfaceSymbol &name, const Factory &factory,
      bool excluded );

   ModuleManagerInterface *mModMan;
   PluginPath mPath;

   PluginPaths mNames;
};
