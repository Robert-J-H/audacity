/**********************************************************************

  Audacity: A Digital Audio Editor

  XMLFileReader.h

  Dominic Mazzoni

**********************************************************************/

#include "../Audacity.h"

#include <memory>
#include <vector>
#include "expat.h"

#include "XMLTagHandler.h"
#include "audacity/Types.h"

class AUDACITY_DLL_API XMLFileReader final {
 public:
   XMLFileReader();
   ~XMLFileReader();

   bool Parse(XMLTagHandlerPtr baseHandler,
              const FilePath &fname);

   wxString GetErrorStr();

   // Callback functions for expat

   static void startElement(void *userData, const char *name,
                            const char **atts);

   static void endElement(void *userData, const char *name);

   static void charHandler(void *userData, const char *s, int len);

 private:
   XML_Parser       mParser;
   XMLTagHandlerPtr   mBaseHandler;
   using Handlers = std::vector<XMLTagHandlerPtr>;
   Handlers mHandler;
   wxString         mErrorStr;
};
