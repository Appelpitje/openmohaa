# Fork Features & Changes

This repository is a fork of the official [OpenMoHAA](https://github.com/openmoh/openmohaa) engine project, with additional quality-of-life improvements, Discord integration, and bugfixes.

---

## 1. Discord Rich Presence

Integrated Discord Rich Presence (RPC) support that displays your active game status on your Discord profile. Discord RPC is compiled into the client by default, and disabled at runtime until turned on.

<p align="center">
  <img src="misc/discord-rpc-preview.png" alt="Discord Rich Presence Preview" width="400">
</p>

### Features
- **Single-Player**: Shows current mission and map details.
- **Multiplayer**: Shows current server name, map name, gametype, and real-time player count (`current / max players`).
- **Game State**: Shows elapsed playing time.

### Enabling Discord Rich Presence
You can enable or disable Discord Rich Presence at runtime via the in-game console (`~`) or in your config:

```cfg
set cl_discordRichPresence 1   // Enable Discord Rich Presence
set cl_discordRichPresence 0   // Disable Discord Rich Presence
```

### CVars
| CVar | Default | Values | Description |
|------|---------|--------|-------------|
| `cl_discordRichPresence` | `0` | `0` or `1` | `1` enables Discord Rich Presence reporting to the local Discord client; `0` disables it. Saved to archive config. |

### CMake Build Option
Discord RPC is included in the build by default (`USE_DISCORD_RPC=ON`). To disable compilation of Discord RPC entirely at build time:
```bash
cmake -B build -DUSE_DISCORD_RPC=OFF
```

---

## 2. Multiplayer Chat Management

Allows players to completely disable or toggle in-game multiplayer chat and quick voice chat messages for a cleaner HUD or distraction-free gameplay.

### CVars
| CVar | Default | Values | Description |
|------|---------|--------|-------------|
| `cg_chat` | `1` | `0` or `1` | `1` enables multiplayer chat display and sending; `0` hides all chat, voice messages, and disables the chat console. Saved to archive config. |

### Console Commands
You can run these commands from the in-game console (`~`) or bind them to hotkeys:

- **`chat`** - Toggles multiplayer chat on or off when called without arguments.
- **`chat [0|1|on|off|enable|disable|toggle]`** - Explicitly sets or toggles the chat state.
  - Examples: `chat 0`, `chat off`, `chat enable`
- **`togglechat`** - Quickly toggles chat between enabled (`1`) and disabled (`0`).
- **`enablechat`** - Enables multiplayer chat (`cg_chat 1`).
- **`disablechat`** - Disables multiplayer chat (`cg_chat 0`).

#### Keybind Example:
```cfg
bind F10 "togglechat"
```

---

## 3. Mouse Coordinate Scaling Fix

- **Resolution & Window Scaling**: Fixes mouse coordinate mapping when running in windowed mode, non-native aspect ratios, or on High-DPI / Retina displays.
- Ensures crosshair / cursor positioning matches the exact rendered game view without offset.

---

## 4. Platform & Build Compatibility

- **macOS OpenAL Support**: Updated CMake OpenAL resolution logic to ensure internal OpenAL headers (`alext.h`) are utilized on macOS, avoiding missing header errors when building with AppleClang / Xcode.
- **Universal Architecture Defaults**: Ensures native host architecture (`arm64` on Apple Silicon) is built by default without requiring cross-compilation toolchains.
