/*
===========================================================================
Copyright (C) 2026 the OpenMoHAA team

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

#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

enum class WorkshopContentType {
    ALL,
    MAP,
    MOD,
    COLLECTION
};

enum class WorkshopDownloadState {
    IDLE,
    FETCHING_URL,
    DOWNLOADING,
    VERIFYING,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct WorkshopItem {
    int64_t id = 0;
    std::string title;
    std::string author;
    std::string version;
    std::string description;
    std::string shortDescription;
    std::string gameType;        // "MOHAA", "MOHSH", "MOHBT", "ALL"
    std::string contentType;     // "Map", "Mod", "Skin", "Weapon", "Script", "Sound", "Collection"
    std::string category;        // e.g. "Objective", "Deathmatch", "Skin", etc.
    std::string filename;        // e.g. "obj_omaha_v2.pk3"
    std::string mapName;         // e.g. "obj/obj_omaha_v2" (for direct launching)
    std::string previewImageUrl;
    std::string downloadUrl;
    int64_t fileSize = 0;        // in bytes
    float rating = 0.0f;         // 0.0 - 5.0
    int downloadCount = 0;
    bool isInstalled = false;
    std::string localFilePath;
};

struct WorkshopDownloadProgress {
    int64_t itemId = 0;
    std::string title;
    std::string filename;
    WorkshopDownloadState state = WorkshopDownloadState::IDLE;
    double bytesCurrent = 0.0;
    double bytesTotal = 0.0;
    double speedBytesPerSec = 0.0;
    float percentage = 0.0f;
    std::string statusMessage;
    bool autoPlayAfterDownload = false;
    std::string mapNameToLaunch;
};

typedef std::function<void(bool success, const std::vector<WorkshopItem>& items, int totalElements)> WorkshopSearchCallback;
typedef std::function<void(bool success, const WorkshopItem& item)> WorkshopItemCallback;
typedef std::function<void(bool success, const std::string& message)> WorkshopActionCallback;

#ifdef __cplusplus
extern "C" {
#endif

// Engine lifecycle hooks
void CL_Workshop_Init(void);
void CL_Workshop_Frame(void);
void CL_Workshop_Shutdown(void);

#ifdef __cplusplus
}
#endif

class WorkshopManager {
public:
    static WorkshopManager& Instance();

    void Init();
    void Frame();
    void Shutdown();

    // Queries
    void Search(const std::string& query,
                WorkshopContentType type,
                const std::string& gameType,
                int page,
                int size,
                WorkshopSearchCallback callback);

    void GetFeatured(WorkshopSearchCallback callback);

    void GetItemDetails(int64_t id,
                        WorkshopContentType type,
                        WorkshopItemCallback callback);

    void SyncCollection(const std::string& slugOrId,
                        WorkshopSearchCallback callback);

    // Downloads & Management
    void StartDownload(const WorkshopItem& item, bool autoPlay = false);
    void CancelDownload();
    WorkshopDownloadProgress GetCurrentProgress();

    // Local Installations
    void ScanInstalledItems();
    bool IsItemInstalled(const std::string& filename, std::string* outPath = nullptr);
    std::vector<WorkshopItem> GetInstalledItems();
    bool UninstallItem(const std::string& filename);
    bool PlayMap(const std::string& mapName, const std::string& filename = "");

    // Path Helpers
    std::string GetTargetGameDir(const std::string& gameType) const;
    std::string GetFullDownloadPath(const std::string& filename, const std::string& gameType) const;

private:
    WorkshopManager();
    ~WorkshopManager();

    class Impl;
    std::unique_ptr<Impl> pImpl;
};
