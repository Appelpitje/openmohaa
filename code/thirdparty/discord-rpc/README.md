# Discord RPC Library for OpenMoHAA

This directory contains the Discord Rich Presence library for OpenMoHAA.

## Setup Instructions

### 1. Download the Discord RPC Library

Download the Discord RPC library from the official releases:
https://github.com/discord/discord-rpc/releases

Choose the latest release and download the appropriate archive for your platform.

### 2. Extract the Library Files

Extract the archive and copy files to this directory:

#### Windows (64-bit)
```
lib/win64-dynamic/discord-rpc.lib
lib/win64-dynamic/discord-rpc.dll
```

#### Windows (32-bit)
```
lib/win32-dynamic/discord-rpc.lib
lib/win32-dynamic/discord-rpc.dll
```

#### macOS
```
lib/osx-dynamic/libdiscord-rpc.dylib
```

#### Linux
```
lib/linux-dynamic/libdiscord-rpc.so
```

### 3. Directory Structure

After setup, your directory structure should look like:
```
code/thirdparty/discord-rpc/
├── include/
│   ├── discord_rpc.h
│   └── discord_register.h
├── lib/
│   ├── win64-dynamic/       (Windows 64-bit)
│   │   ├── discord-rpc.dll
│   │   └── discord-rpc.lib
│   ├── win32-dynamic/       (Windows 32-bit)
│   │   ├── discord-rpc.dll
│   │   └── discord-rpc.lib
│   ├── osx-dynamic/         (macOS)
│   │   └── libdiscord-rpc.dylib
│   └── linux-dynamic/       (Linux)
│       └── libdiscord-rpc.so
└── README.md
```

### 4. Configure Discord Application ID

1. Go to the [Discord Developer Portal](https://discord.com/developers/applications)
2. Create a new application or use an existing one
3. Copy the Application ID
4. Update the `DISCORD_APP_ID` define in `code/client/cl_discord.cpp`

### 5. Upload Rich Presence Assets (Optional)

You can upload custom images for your Rich Presence:

1. In the Discord Developer Portal, go to your application
2. Navigate to "Rich Presence" → "Art Assets"
3. Upload images with keys:
   - `openmohaa_logo` - Main game logo (1024x1024 recommended)
   - `playing` - Small icon for playing state

## Build Options

To disable Discord Rich Presence, configure CMake with:
```
cmake -DUSE_DISCORD_RPC=OFF ..
```

## Runtime Configuration

Discord Rich Presence can be enabled/disabled at runtime:
```
set cl_discordRichPresence 1  # Enable
set cl_discordRichPresence 0  # Disable
```

The setting is saved to your config file.
