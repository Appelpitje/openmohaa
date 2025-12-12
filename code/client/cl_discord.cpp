/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// cl_discord.cpp -- Discord Rich Presence integration

#include "client.h"
#include "cl_discord.h"

#ifdef USE_DISCORD_RPC
#include "../thirdparty/discord-rpc/include/discord_rpc.h"
#include <cstring>
#include <ctime>

// Discord Application ID - Replace with your own from Discord Developer Portal
// Create an application at: https://discord.com/developers/applications
#define DISCORD_APP_ID "1449151797166870722"

// Update interval in milliseconds (don't spam Discord API)
#define DISCORD_UPDATE_INTERVAL 5000

static cvar_t *cl_discordRichPresence = NULL;
static qboolean discord_initialized = qfalse;
static int64_t discord_startTime = 0;
static int discord_lastUpdateTime = 0;
static connstate_t discord_lastState = CA_UNINITIALIZED;
static char discord_lastMapName[MAX_QPATH] = {0};

/*
====================
Discord_HandleReady

Called when Discord connection is established
====================
*/
static void Discord_HandleReady(const DiscordUser* user)
{
    Com_Printf("Discord: Connected as %s#%s\n", user->username, user->discriminator);
}

/*
====================
Discord_HandleDisconnected

Called when Discord connection is lost
====================
*/
static void Discord_HandleDisconnected(int errorCode, const char* message)
{
    Com_Printf("Discord: Disconnected (%d: %s)\n", errorCode, message);
}

/*
====================
Discord_HandleError

Called when Discord encounters an error
====================
*/
static void Discord_HandleError(int errorCode, const char* message)
{
    Com_Printf("Discord: Error (%d: %s)\n", errorCode, message);
}

/*
====================
CL_DiscordInit

Initialize Discord Rich Presence
====================
*/
void CL_DiscordInit(void)
{
    DiscordEventHandlers handlers;

    cl_discordRichPresence = Cvar_Get("cl_discordRichPresence", "0", CVAR_ARCHIVE);

    if (!cl_discordRichPresence->integer) {
        Com_Printf("Discord Rich Presence disabled (set cl_discordRichPresence 1 to enable)\n");
        return;
    }

    Com_Printf("----- Discord Rich Presence Initialization -----\n");

    memset(&handlers, 0, sizeof(handlers));
    handlers.ready = Discord_HandleReady;
    handlers.disconnected = Discord_HandleDisconnected;
    handlers.errored = Discord_HandleError;

    Discord_Initialize(DISCORD_APP_ID, &handlers, 1, NULL);

    discord_initialized = qtrue;
    discord_startTime = (int64_t)time(NULL);
    discord_lastUpdateTime = 0;
    discord_lastState = CA_UNINITIALIZED;
    discord_lastMapName[0] = '\0';

    Com_Printf("Discord Rich Presence initialized\n");
}

/*
====================
CL_DiscordUpdatePresence

Update the Discord presence based on current game state
====================
*/
static void CL_DiscordUpdatePresence(void)
{
    DiscordRichPresence presence;
    char stateBuffer[128];
    char detailsBuffer[128];
    const char *serverInfo;
    const char *mapMessage;
    const char *serverName = NULL;
    int numClients = 0;
    int maxClients = 0;

    memset(&presence, 0, sizeof(presence));

    // Set the start timestamp for "elapsed" display
    presence.startTimestamp = discord_startTime;

    // Note: largeImageKey/smallImageKey require assets uploaded to Discord Developer Portal
    // Leave them as NULL to show presence without custom images

    switch (clc.state) {
        case CA_DISCONNECTED:
        case CA_UNINITIALIZED:
            Q_strncpyz(stateBuffer, "In Main Menu", sizeof(stateBuffer));
            presence.state = stateBuffer;
            presence.details = "Idle";
            break;

        case CA_CONNECTING:
        case CA_CHALLENGING:
        case CA_CONNECTED:
            Q_strncpyz(stateBuffer, "Connecting to server...", sizeof(stateBuffer));
            presence.state = stateBuffer;
            if (clc.servername[0]) {
                Com_sprintf(detailsBuffer, sizeof(detailsBuffer), "Server: %s", clc.servername);
                presence.details = detailsBuffer;
            }
            break;

        case CA_LOADING:
        case CA_PRIMED:
            Q_strncpyz(stateBuffer, "Loading...", sizeof(stateBuffer));
            presence.state = stateBuffer;
            
            // Try to get full map name from CS_MESSAGE (worldspawn message)
            mapMessage = cl.gameState.stringData + cl.gameState.stringOffsets[2]; // CS_MESSAGE = 2
            if (mapMessage && mapMessage[0]) {
                Com_sprintf(detailsBuffer, sizeof(detailsBuffer), "Map: %s", mapMessage);
                presence.details = detailsBuffer;
            } else if (cl.mapname[0]) {
                // Fallback to BSP name
                Com_sprintf(detailsBuffer, sizeof(detailsBuffer), "Map: %s", COM_SkipPath((char*)cl.mapname));
                presence.details = detailsBuffer;
            }
            break;

        case CA_ACTIVE:
            // Get server info for hostname and player count
            serverInfo = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
            
            // Get server hostname and build details string (top line)
            if (serverInfo && serverInfo[0]) {
                serverName = Info_ValueForKey(serverInfo, "sv_hostname");
                
                const char *val = Info_ValueForKey(serverInfo, "sv_maxclients");
                if (val && val[0]) {
                    maxClients = atoi(val);
                }
            }
            
            // Details = Server name + IP:port
            if (serverName && serverName[0]) {
                Com_sprintf(detailsBuffer, sizeof(detailsBuffer), "%s (%s)", 
                    serverName, NET_AdrToStringwPort(clc.serverAddress));
            } else if (clc.servername[0]) {
                Com_sprintf(detailsBuffer, sizeof(detailsBuffer), "Server: %s", clc.servername);
            } else {
                Com_sprintf(detailsBuffer, sizeof(detailsBuffer), "Multiplayer");
            }
            presence.details = detailsBuffer;

            // Get full map name from CS_MESSAGE (worldspawn message field)
            mapMessage = cl.gameState.stringData + cl.gameState.stringOffsets[2]; // CS_MESSAGE = 2
            if (mapMessage && mapMessage[0]) {
                Q_strncpyz(stateBuffer, mapMessage, sizeof(stateBuffer));
            } else if (cl.mapname[0]) {
                // Fallback to BSP name without path/extension
                const char *bspName = COM_SkipPath((char*)cl.mapname);
                Q_strncpyz(stateBuffer, bspName, sizeof(stateBuffer));
                // Remove .bsp extension if present
                char *ext = strstr(stateBuffer, ".bsp");
                if (ext) *ext = '\0';
            } else {
                Q_strncpyz(stateBuffer, "In Game", sizeof(stateBuffer));
            }
            presence.state = stateBuffer;

            // Count players by checking CS_PLAYERS configstrings (slot 1684+)
            // CS_PLAYERS = 1684, MAX_CLIENTS = 64
            {
                int i;
                numClients = 0;
                for (i = 0; i < 64; i++) {
                    const char *playerInfo = cl.gameState.stringData + cl.gameState.stringOffsets[1684 + i];
                    if (playerInfo && playerInfo[0]) {
                        numClients++;
                    }
                }
            }

            // Set party size for player count display
            if (numClients > 0 && maxClients > 0) {
                presence.partySize = numClients;
                presence.partyMax = maxClients;
            } else if (maxClients > 0) {
                presence.partySize = 1; // At least the player themselves
                presence.partyMax = maxClients;
            }
            break;

        case CA_CINEMATIC:
            Q_strncpyz(stateBuffer, "Watching Cinematic", sizeof(stateBuffer));
            presence.state = stateBuffer;
            presence.details = "Cutscene";
            break;

        default:
            Q_strncpyz(stateBuffer, "In Game", sizeof(stateBuffer));
            presence.state = stateBuffer;
            break;
    }

    Discord_UpdatePresence(&presence);
}

/*
====================
CL_DiscordUpdate

Update Discord Rich Presence - called every frame
====================
*/
void CL_DiscordUpdate(void)
{
    int currentTime;
    qboolean stateChanged;

    // Check if Discord RPC cvar changed
    if (!cl_discordRichPresence) {
        cl_discordRichPresence = Cvar_Get("cl_discordRichPresence", "0", CVAR_ARCHIVE);
    }

    // Handle enable/disable at runtime
    if (cl_discordRichPresence->integer && !discord_initialized) {
        // User enabled Discord RPC, initialize it
        DiscordEventHandlers handlers;
        memset(&handlers, 0, sizeof(handlers));
        handlers.ready = Discord_HandleReady;
        handlers.disconnected = Discord_HandleDisconnected;
        handlers.errored = Discord_HandleError;

        Discord_Initialize(DISCORD_APP_ID, &handlers, 1, NULL);
        discord_initialized = qtrue;
        discord_startTime = (int64_t)time(NULL);
        discord_lastUpdateTime = 0;
        discord_lastState = CA_UNINITIALIZED;
        Com_Printf("Discord Rich Presence enabled\n");
    } else if (!cl_discordRichPresence->integer && discord_initialized) {
        // User disabled Discord RPC, shut it down
        Discord_ClearPresence();
        Discord_Shutdown();
        discord_initialized = qfalse;
        Com_Printf("Discord Rich Presence disabled\n");
        return;
    }

    if (!discord_initialized) {
        return;
    }

    // Run Discord callbacks
    Discord_RunCallbacks();

    // Check if state changed or enough time passed for an update
    currentTime = Sys_Milliseconds();
    stateChanged = (clc.state != discord_lastState) || 
                   (Q_stricmp(cl.mapname, discord_lastMapName) != 0);

    if (stateChanged || (currentTime - discord_lastUpdateTime) >= DISCORD_UPDATE_INTERVAL) {
        CL_DiscordUpdatePresence();
        
        discord_lastState = clc.state;
        Q_strncpyz(discord_lastMapName, cl.mapname, sizeof(discord_lastMapName));
        discord_lastUpdateTime = currentTime;
    }
}

/*
====================
CL_DiscordShutdown

Shutdown Discord Rich Presence
====================
*/
void CL_DiscordShutdown(void)
{
    if (!discord_initialized) {
        return;
    }

    Com_Printf("Discord Rich Presence shutting down...\n");

    Discord_ClearPresence();
    Discord_Shutdown();

    discord_initialized = qfalse;
}

#else // USE_DISCORD_RPC not defined

// Stub implementations when Discord RPC is disabled
void CL_DiscordInit(void) {}
void CL_DiscordUpdate(void) {}
void CL_DiscordShutdown(void) {}

#endif // USE_DISCORD_RPC
