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

// cl_discord.h -- Discord Rich Presence integration

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Discord Rich Presence.
 * Should be called during client initialization (CL_Init).
 */
void CL_DiscordInit(void);

/**
 * Update Discord Rich Presence status.
 * Should be called each frame (CL_Frame) to process callbacks
 * and update presence based on current game state.
 */
void CL_DiscordUpdate(void);

/**
 * Shutdown Discord Rich Presence.
 * Should be called during client shutdown (CL_Shutdown).
 */
void CL_DiscordShutdown(void);

#ifdef __cplusplus
}
#endif
