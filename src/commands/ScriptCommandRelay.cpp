/**********************************************************************

   Audacity - A Digital Audio Editor
   Copyright 1999-2018 Audacity Team
   File License: wxWidgets

   Dan Horgan

******************************************************************//**

\file ScriptCommandRelay.cpp
\brief Contains definitions for ScriptCommandRelay

*//****************************************************************//**

\class ScriptCommandRelay
\brief ScriptCommandRelay is just a way to move some of the scripting-specific
code out of ModuleManager.

*//*******************************************************************/

#include "ScriptCommandRelay.h"

#include "CommandTargets.h"
#include "CommandBuilder.h"
#include "AppCommandEvent.h"
#include <wx/app.h>
#include <wx/window.h>
#include <wx/string.h>

// Declare static class members
CommandHandler *ScriptCommandRelay::sCmdHandler;
tpRegScriptServerFunc ScriptCommandRelay::sScriptFn;

void ScriptCommandRelay::SetRegScriptServerFunc(tpRegScriptServerFunc scriptFn)
{
   sScriptFn = scriptFn;
}

void ScriptCommandRelay::SetCommandHandler(CommandHandler &ch)
{
   sCmdHandler = &ch;
}

/// Calls the script function, passing it the function for obeying commands
void ScriptCommandRelay::Run()
{
   wxASSERT( sScriptFn != NULL );
   while( true )
      sScriptFn(&ExecCommand);
}

/// Send a command to a project, to be applied in that context.
void ScriptCommandRelay::PostCommand(
   wxWindow *pWindow, const OldStyleCommandPointer &cmd)
{
   wxASSERT( pWindow );
   wxASSERT(cmd != NULL);
   if ( pWindow ) {
      AppCommandEvent ev;
      ev.SetCommand(cmd);
      pWindow->GetEventHandler()->AddPendingEvent(ev);
   }
}

/// This is the function which actually obeys one command.  Rather than applying
/// the command directly, an event containing a reference to the command is sent
/// to the main (GUI) thread. This is because having more than one thread access
/// the GUI at a time causes problems with wxwidgets.
int ExecCommand(wxString *pIn, wxString *pOut)
{
   {
      CommandBuilder builder(*pIn);
      if (builder.WasValid())
      {
         OldStyleCommandPointer cmd = builder.GetCommand();
         ScriptCommandRelay::PostCommand(wxTheApp->GetTopWindow(), cmd);

         *pOut = wxEmptyString;
      }
      else
      {
         *pOut = wxT("Syntax error!\n");
         *pOut += builder.GetErrorMessage() + wxT("\n");
      }
   }

   // Wait until all responses from the command have been received.
   // The last response is signalled by an empty line.
   wxString msg = ScriptCommandRelay::ReceiveResponse().GetMessage();
   while (msg != wxT("\n"))
   {
      //wxLogDebug( "Msg: %s", msg );
      *pOut += msg + wxT("\n");
      msg = ScriptCommandRelay::ReceiveResponse().GetMessage();
   }

   return 0;
}

/// This is the function which actually obeys one command.  Rather than applying
/// the command directly, an event containing a reference to the command is sent
/// to the main (GUI) thread. This is because having more than one thread access
/// the GUI at a time causes problems with wxwidgets.
int ExecCommand2(wxString *pIn, wxString *pOut)
{
   {
      CommandBuilder builder(*pIn);
      if (builder.WasValid())
      {
         OldStyleCommandPointer cmd = builder.GetCommand();
         AppCommandEvent ev;
         ev.SetCommand(cmd);

         // Use SafelyProcessEvent, which stops exceptions, because this is
         // expected to be reached from within the XLisp runtime
         wxTheApp->SafelyProcessEvent( ev );

         *pOut = wxEmptyString;
      }
      else
      {
         *pOut = wxT("Syntax error!\n");
         *pOut += builder.GetErrorMessage() + wxT("\n");
      }
   }

   // Wait until all responses from the command have been received.
   // The last response is signalled by an empty line.
   wxString msg = ScriptCommandRelay::ReceiveResponse().GetMessage();
   while (msg != wxT("\n"))
   {
      //wxLogDebug( "Msg: %s", msg );
      *pOut += msg + wxT("\n");
      msg = ScriptCommandRelay::ReceiveResponse().GetMessage();
   }

   return 0;
}



// The void * return is actually a Lisp LVAL and will be cast to such as needed.
extern void * ExecForLisp( char * pIn );
extern void * nyq_make_opaque_string( int size, unsigned char *src );
extern void * nyq_reformat_aud_do_response(const wxString & Str);


void * ExecForLisp( char * pIn ){
   wxString Str1( pIn );
   wxString Str2;
   ExecCommand2( &Str1, &Str2 );

   // wxString provides a const char *
   //const char * pStr = static_cast<const char*>(Str2);

   // We'll be passing it as a non-const unsigned char *
   // That 'unsafe' cast is actually safe.  nyq_make_opaque_string is just copying the string.
   void * pResult = nyq_reformat_aud_do_response( Str2 );
   return pResult;
};


/// Gets a response from the queue (may block)
Response ScriptCommandRelay::ReceiveResponse()
{
   return ResponseQueueTarget::sResponseQueue().WaitAndGetResponse();
}
