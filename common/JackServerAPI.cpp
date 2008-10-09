/*
Copyright (C) 2001-2003 Paul Davis
Copyright (C) 2004-2008 Grame

This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "JackSystemDeps.h"
#include "JackGraphManager.h"
#include "JackInternalClient.h"
#include "JackServer.h"
#include "JackDebugClient.h"
#include "JackServerGlobals.h"
#include "JackTools.h"
#include "JackCompilerDeps.h"
#include "JackLockedEngine.h"

#ifdef __cplusplus
extern "C"
{
#endif

    EXPORT jack_client_t * jack_client_open_aux (const char *client_name,
            jack_options_t options,
            jack_status_t *status, va_list ap);
    EXPORT jack_client_t * jack_client_open (const char *client_name,
            jack_options_t options,
            jack_status_t *status, ...);
    EXPORT int jack_client_close (jack_client_t *client);
    EXPORT int jack_get_client_pid (const char *name);

#ifdef __cplusplus
}
#endif

using namespace Jack;

// beware!!! things can go nasty if one client is started with JackNoStartServer and another without it
static bool g_nostart;

EXPORT jack_client_t* jack_client_open_aux(const char* ext_client_name, jack_options_t options, jack_status_t* status, va_list ap)
{
    jack_varargs_t va;		/* variable arguments */
    jack_status_t my_status;
    JackClient* client;
    char client_name[JACK_CLIENT_NAME_SIZE + 1];

    if (ext_client_name == NULL) {
        jack_error("jack_client_open called with a NULL client_name");
        return NULL;
    }

    jack_log("jack_client_open %s", ext_client_name);
    JackTools::RewriteName(ext_client_name, client_name);

    if (status == NULL)			/* no status from caller? */
        status = &my_status;	/* use local status word */
    *status = (jack_status_t)0;

    /* validate parameters */
    if ((options & ~JackOpenOptions)) {
        int my_status1 = *status | (JackFailure | JackInvalidOption);
        *status = (jack_status_t)my_status1;
        return NULL;
    }

    /* parse variable arguments */
    if (ap) {
        jack_varargs_parse(options, ap, &va);
    } else {
        jack_varargs_init(&va);
    }

    g_nostart = (options & JackNoStartServer) != 0;
    if (!g_nostart) {
        if (!JackServerGlobals::Init()) { // jack server initialisation
            int my_status1 = (JackFailure | JackServerError);
            *status = (jack_status_t)my_status1;
            return NULL;
        }
    }

    if (JACK_DEBUG) {
        client = new JackDebugClient(new JackInternalClient(JackServer::fInstance, GetSynchroTable())); // Debug mode
    } else {
        client = new JackInternalClient(JackServer::fInstance, GetSynchroTable());
    }

    int res = client->Open(va.server_name, client_name, options, status);
    if (res < 0) {
        delete client;
        if (!g_nostart) {
            JackServerGlobals::Destroy(); // jack server destruction
        }
        int my_status1 = (JackFailure | JackServerError);
        *status = (jack_status_t)my_status1;
        return NULL;
    } else {
        return (jack_client_t*)client;
    }
}

EXPORT jack_client_t* jack_client_open(const char* ext_client_name, jack_options_t options, jack_status_t* status, ...)
{
    assert(gOpenMutex);
    gOpenMutex->Lock();
    va_list ap;
    va_start(ap, status);
    jack_client_t* res = jack_client_open_aux(ext_client_name, options, status, ap);
    va_end(ap);
    gOpenMutex->Unlock();
    return res;
}

EXPORT int jack_client_close(jack_client_t* ext_client)
{
    assert(gOpenMutex);
    gOpenMutex->Lock();
    int res = -1;
    jack_log("jack_client_close");
    JackClient* client = (JackClient*)ext_client;
    if (client == NULL) {
        jack_error("jack_client_close called with a NULL client");
    } else {
        res = client->Close();
        delete client;
        if (!g_nostart) {
            JackServerGlobals::Destroy();	// jack server destruction
        }
        jack_log("jack_client_close res = %d", res);
    }
    gOpenMutex->Unlock();
    return res;
}

EXPORT int jack_get_client_pid(const char *name)
{
    return (JackServer::fInstance != NULL) 
        ? JackServer::fInstance->GetEngine()->GetClientPID(name)
        : 0;
}

