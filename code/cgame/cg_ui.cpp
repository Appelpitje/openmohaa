/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

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

// DESCRIPTION:
// UI features

#include "cg_local.h"
#include "../corepp/str.h"
#include "../client/keycodes.h"

void CG_MessageMode_f(void)
{
    if (!cg_chat->integer) {
        cgi.Printf("Multiplayer chat is disabled (cg_chat is 0).\n");
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cgi.UI_ToggleDMMessageConsole(300);
}

void CG_MessageMode_All_f(void)
{
    if (!cg_chat->integer) {
        cgi.Printf("Multiplayer chat is disabled (cg_chat is 0).\n");
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cgi.UI_ToggleDMMessageConsole(100);
}

void CG_MessageMode_Team_f(void)
{
    if (!cg_chat->integer) {
        cgi.Printf("Multiplayer chat is disabled (cg_chat is 0).\n");
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cgi.UI_ToggleDMMessageConsole(200);
}

void CG_MessageMode_Private_f(void)
{
    int clientNum;

    if (!cg_chat->integer) {
        cgi.Printf("Multiplayer chat is disabled (cg_chat is 0).\n");
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    clientNum = atoi(cgi.Argv(1));
    if (clientNum < 1 || clientNum >= MAX_CLIENTS) {
        cgi.Printf(HUD_MESSAGE_CHAT_WHITE "Message Error: %s is a bad client number\n", cgi.Argv(1));
        return;
    }

    cgi.UI_ToggleDMMessageConsole(clientNum);
}

void CG_MessageSingleAll_f(void)
{
    if (!cg_chat->integer) {
        cgi.Printf("Multiplayer chat is disabled (cg_chat is 0).\n");
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    if (cgi.Argc() > 1) {
        cgi.SendClientCommand(va("dmmessage 0 %s\n", cgi.Args()));
    } else {
        cgi.UI_ToggleDMMessageConsole(-100);
    }
}

void CG_MessageSingleTeam_f(void)
{
    if (!cg_chat->integer) {
        cgi.Printf("Multiplayer chat is disabled (cg_chat is 0).\n");
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    if (cgi.Argc() > 1) {
        cgi.SendClientCommand(va("dmmessage -1 %s\n", cgi.Args()));
    } else {
        cgi.UI_ToggleDMMessageConsole(-200);
    }
}

void CG_MessageSingleClient_f(void)
{
    int clientNum;

    if (!cg_chat->integer) {
        cgi.Printf("Multiplayer chat is disabled (cg_chat is 0).\n");
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    clientNum = atoi(cgi.Argv(1));
    if (clientNum < 1 || clientNum > MAX_CLIENTS) {
        cgi.Printf(HUD_MESSAGE_CHAT_WHITE "Message Error: %s is a bad client number\n", cgi.Argv(1));
        return;
    }

    if (cgi.Argc() > 2) {
        int i;
        str sString;

        sString = "dmmessage ";
        sString += va("%i", clientNum);

        // copy the rest
        for (i = 2; i < cgi.Argc(); i++) {
            sString += va(" %s", cgi.Argv(i));
        }

        sString += "\n";
        cgi.SendClientCommand(sString.c_str());
    } else {
        cgi.UI_ToggleDMMessageConsole(-clientNum);
    }
}

void CG_InstaMessageMain_f(void)
{
    if (!voiceChat->integer || !cg_chat->integer) {
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cg.iInstaMessageMenu = -1;
}

void CG_InstaMessageGroupA_f(void)
{
    if (!voiceChat->integer || !cg_chat->integer) {
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cg.iInstaMessageMenu = 1;
}

void CG_InstaMessageGroupB_f(void)
{
    if (!voiceChat->integer || !cg_chat->integer) {
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cg.iInstaMessageMenu = 2;
}

void CG_InstaMessageGroupC_f(void)
{
    if (!voiceChat->integer || !cg_chat->integer) {
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cg.iInstaMessageMenu = 3;
}

void CG_InstaMessageGroupD_f(void)
{
    if (!voiceChat->integer || !cg_chat->integer) {
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cg.iInstaMessageMenu = 4;
}

void CG_InstaMessageGroupE_f(void)
{
    if (!voiceChat->integer || !cg_chat->integer) {
        return;
    }

    if (!cgs.gametype) {
        return;
    }

    cg.iInstaMessageMenu = 5;
}

void CG_HudPrint_f(void)
{
    cgi.Printf("\x1%s", cgi.Argv(1));
}

void CG_Chat_f(void)
{
    if (cgi.Argc() < 2) {
        if (cg_chat && cg_chat->integer) {
            cgi.Cvar_Set("cg_chat", "0");
            cgi.Printf("Multiplayer chat is now disabled.\n");
        } else {
            cgi.Cvar_Set("cg_chat", "1");
            cgi.Printf("Multiplayer chat is now enabled.\n");
        }
        return;
    }

    const char *arg = cgi.Argv(1);
    if (!Q_stricmp(arg, "1") || !Q_stricmp(arg, "on") || !Q_stricmp(arg, "enable") || !Q_stricmp(arg, "true")) {
        cgi.Cvar_Set("cg_chat", "1");
        cgi.Printf("Multiplayer chat is now enabled.\n");
    } else if (!Q_stricmp(arg, "0") || !Q_stricmp(arg, "off") || !Q_stricmp(arg, "disable") || !Q_stricmp(arg, "false")) {
        cgi.Cvar_Set("cg_chat", "0");
        cgi.Printf("Multiplayer chat is now disabled.\n");
    } else if (!Q_stricmp(arg, "toggle")) {
        if (cg_chat && cg_chat->integer) {
            cgi.Cvar_Set("cg_chat", "0");
            cgi.Printf("Multiplayer chat is now disabled.\n");
        } else {
            cgi.Cvar_Set("cg_chat", "1");
            cgi.Printf("Multiplayer chat is now enabled.\n");
        }
    } else {
        cgi.Printf("Usage: chat [0|1|on|off|enable|disable|toggle]\n");
    }
}

void CG_ToggleChat_f(void)
{
    if (cg_chat && cg_chat->integer) {
        cgi.Cvar_Set("cg_chat", "0");
        cgi.Printf("Multiplayer chat is now disabled.\n");
    } else {
        cgi.Cvar_Set("cg_chat", "1");
        cgi.Printf("Multiplayer chat is now enabled.\n");
    }
}

void CG_EnableChat_f(void)
{
    cgi.Cvar_Set("cg_chat", "1");
    cgi.Printf("Multiplayer chat is now enabled.\n");
}

void CG_DisableChat_f(void)
{
    cgi.Cvar_Set("cg_chat", "0");
    cgi.Printf("Multiplayer chat is now disabled.\n");
}

qboolean CG_CheckCaptureKey(int key, qboolean down, unsigned int time)
{
    char minKey = '1', maxKey = '9';

    if (!cg.iInstaMessageMenu || !down) {
        return qfalse;
    }

    if (cg_protocol >= protocol_e::PROTOCOL_MOHTA_MIN) {
        maxKey = '8';
    }

    if (key < minKey || key > maxKey) {
        if (key == K_ESCAPE || key == '0') {
            cg.iInstaMessageMenu = 0;
            return qtrue;
        }
        return qfalse;
    }

    if (cg.iInstaMessageMenu == -1) {
        if (key > '6') {
            cg.iInstaMessageMenu = 0;
        } else {
            cg.iInstaMessageMenu = key - '0';
        }
    } else if (cg.iInstaMessageMenu > 0) {
        cgi.SendClientCommand(va("dmmessage 0 *%i%i\n", cg.iInstaMessageMenu, key - '0'));
        cg.iInstaMessageMenu = 0;
    }

    return qtrue;
}
