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

#include "cl_workshop.h"
#include "client.h"
#include "../sys/sys_curl.h"
#include "../qcommon/json.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>

using json = nlohmann::json;

// Console Variables
cvar_t *cl_workshop_api_url = nullptr;
cvar_t *cl_workshop_enabled = nullptr;
cvar_t *cl_workshop_auto_mount = nullptr;
cvar_t *cl_workshop_timeout = nullptr;

// Forward declaration of UI launch command
void UI_LaunchWorkshop_f(void);

namespace {

// Helper: URL encoding
static void Workshop_ReplaceSeparators(char *path) {
    if (!path) return;
    for (char *s = path; *s; s++) {
        if (*s == '\\') *s = '/';
    }
}

std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << "%20";
        } else {
            escaped << '%' << std::setw(2) << ((int)(unsigned char)c);
        }
    }
    return escaped.str();
}

// Helper: Formatted file size string
std::string FormatBytes(double bytes) {
    char buf[64];
    if (bytes >= 1024.0 * 1024.0 * 1024.0) {
        Com_sprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024.0 * 1024.0) {
        Com_sprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024.0) {
        Com_sprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    } else {
        Com_sprintf(buf, sizeof(buf), "%.0f B", bytes);
    }
    return std::string(buf);
}

// HTTP Response Buffer
struct HttpResponse {
    std::string body;
    long statusCode = 0;
    bool success = false;
    std::string errorMsg;
};

#ifdef USE_HTTP
size_t Workshop_WriteMemoryCallback(char *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    std::string *mem = static_cast<std::string *>(userp);
    if (mem) {
        mem->append(contents, totalSize);
    }
    return totalSize;
}

struct DownloadStreamContext {
    FILE *file = nullptr;
    std::string tempPath;
    WorkshopDownloadProgress *progress = nullptr;
    std::mutex *progressMutex = nullptr;
    bool *cancelRequested = nullptr;
    std::chrono::steady_clock::time_point lastTime;
    double lastBytes = 0.0;
};

size_t Workshop_WriteFileCallback(char *contents, size_t size, size_t nmemb, void *userp) {
    DownloadStreamContext *ctx = static_cast<DownloadStreamContext *>(userp);
    if (!ctx || !ctx->file) {
        return 0;
    }
    if (ctx->cancelRequested && *ctx->cancelRequested) {
        return 0; // Abort transfer
    }
    return fwrite(contents, size, nmemb, ctx->file);
}

int Workshop_ProgressCallback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
    DownloadStreamContext *ctx = static_cast<DownloadStreamContext *>(clientp);
    if (!ctx) {
        return 0;
    }
    if (ctx->cancelRequested && *ctx->cancelRequested) {
        return 1; // Non-zero return aborts libcurl
    }

    if (ctx->progress && ctx->progressMutex) {
        std::lock_guard<std::mutex> lock(*ctx->progressMutex);
        ctx->progress->bytesCurrent = dlnow;
        ctx->progress->bytesTotal = dltotal;
        if (dltotal > 0.0) {
            ctx->progress->percentage = (float)((dlnow / dltotal) * 100.0);
        }

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - ctx->lastTime;
        if (elapsed.count() >= 0.5) {
            double deltaBytes = dlnow - ctx->lastBytes;
            ctx->progress->speedBytesPerSec = deltaBytes / elapsed.count();
            ctx->lastBytes = dlnow;
            ctx->lastTime = now;
        }

        char msg[256];
        if (dltotal > 0.0) {
            Com_sprintf(msg, sizeof(msg), "Downloading %s: %s / %s (%.1f%%, %s/s)",
                        ctx->progress->filename.c_str(),
                        FormatBytes(dlnow).c_str(),
                        FormatBytes(dltotal).c_str(),
                        ctx->progress->percentage,
                        FormatBytes(ctx->progress->speedBytesPerSec).c_str());
        } else {
            Com_sprintf(msg, sizeof(msg), "Downloading %s: %s (%s/s)",
                        ctx->progress->filename.c_str(),
                        FormatBytes(dlnow).c_str(),
                        FormatBytes(ctx->progress->speedBytesPerSec).c_str());
        }
        ctx->progress->statusMessage = msg;
    }

    return 0;
}
#endif

// Safe JSON helper functions
std::string JsonGetString(const json& j, const std::string& key, const std::string& fallback = "") {
    if (j.contains(key) && !j[key].is_null()) {
        if (j[key].is_string()) {
            return j[key].get<std::string>();
        } else if (j[key].is_number()) {
            return std::to_string(j[key].get<int64_t>());
        }
    }
    return fallback;
}

int64_t JsonGetInt64(const json& j, const std::string& key, int64_t fallback = 0) {
    if (j.contains(key) && !j[key].is_null()) {
        if (j[key].is_number()) {
            return j[key].get<int64_t>();
        } else if (j[key].is_string()) {
            try {
                return std::stoll(j[key].get<std::string>());
            } catch (...) {}
        }
    }
    return fallback;
}

float JsonGetFloat(const json& j, const std::string& key, float fallback = 0.0f) {
    if (j.contains(key) && !j[key].is_null()) {
        if (j[key].is_number()) {
            return j[key].get<float>();
        } else if (j[key].is_string()) {
            try {
                return std::stof(j[key].get<std::string>());
            } catch (...) {}
        }
    }
    return fallback;
}

// Parse a single workshop item from MapDto or ModDto JSON
WorkshopItem ParseItemFromJson(const json& j, const std::string& defaultType = "Map") {
    WorkshopItem item;
    item.id = JsonGetInt64(j, "vid", JsonGetInt64(j, "id", JsonGetInt64(j, "nid")));
    
    std::string title = JsonGetString(j, "title");
    if (title.empty()) {
        title = JsonGetString(j, "mapName", JsonGetString(j, "modName", JsonGetString(j, "name")));
    }
    item.title = title.empty() ? ("Item #" + std::to_string(item.id)) : title;

    item.author = JsonGetString(j, "mapCreator", JsonGetString(j, "modCreator", JsonGetString(j, "author", JsonGetString(j, "creator", "Unknown"))));
    item.version = JsonGetString(j, "modVersion", JsonGetString(j, "version", "1.0"));
    item.description = JsonGetString(j, "mapDescription", JsonGetString(j, "modDescription", JsonGetString(j, "description")));
    item.shortDescription = JsonGetString(j, "shortDescription");
    if (item.shortDescription.empty() && !item.description.empty()) {
        item.shortDescription = item.description.substr(0, std::min<size_t>(item.description.size(), 120));
    }

    item.gameType = JsonGetString(j, "gameType", "MOHAA");
    if (item.gameType.empty()) item.gameType = "MOHAA";

    item.contentType = JsonGetString(j, "type", JsonGetString(j, "typeOfMod", defaultType));
    item.category = JsonGetString(j, "category", JsonGetString(j, "mapsTheme", item.contentType));

    // Resolve filename
    std::string filename;
    if (j.contains("mapFile") && j["mapFile"].is_object()) {
        filename = JsonGetString(j["mapFile"], "filename");
        item.fileSize = JsonGetInt64(j["mapFile"], "filesize");
    } else if (j.contains("pk3File") && j["pk3File"].is_object()) {
        filename = JsonGetString(j["pk3File"], "filename");
        item.fileSize = JsonGetInt64(j["pk3File"], "filesize");
    } else if (j.contains("file") && j["file"].is_object()) {
        filename = JsonGetString(j["file"], "filename");
        item.fileSize = JsonGetInt64(j["file"], "filesize");
    }

    if (filename.empty()) {
        filename = JsonGetString(j, "filename");
    }
    if (filename.empty()) {
        std::string cleanTitle = item.title;
        for (char& c : cleanTitle) {
            if (!isalnum((unsigned char)c) && c != '_' && c != '-') c = '_';
        }
        filename = cleanTitle + ".pk3";
    }
    item.filename = filename;

    // Map launch name
    std::string rawMapName = JsonGetString(j, "mapName");
    if (rawMapName.empty()) {
        rawMapName = item.filename;
        if (rawMapName.size() > 4 && rawMapName.substr(rawMapName.size() - 4) == ".pk3") {
            rawMapName = rawMapName.substr(0, rawMapName.size() - 4);
        }
    }
    item.mapName = rawMapName;

    // Preview image
    if (j.contains("images") && j["images"].is_array() && !j["images"].empty()) {
        item.previewImageUrl = j["images"][0].get<std::string>();
    } else {
        item.previewImageUrl = JsonGetString(j, "previewThumbnailUrl", JsonGetString(j, "image", JsonGetString(j, "thumbnail")));
    }

    // Direct download link / endpoint
    item.downloadUrl = JsonGetString(j, "downloadUrl", JsonGetString(j, "downloadLink"));
    if (item.fileSize == 0) {
        item.fileSize = JsonGetInt64(j, "fileSizeBytes", JsonGetInt64(j, "filesize", JsonGetInt64(j, "mapSize")));
    }

    item.rating = JsonGetFloat(j, "rating", (float)JsonGetInt64(j, "mapRating", JsonGetInt64(j, "modRating")));
    item.downloadCount = (int)JsonGetInt64(j, "downloads", JsonGetInt64(j, "downloadCount"));

    return item;
}

} // namespace

// ===========================================================================
// Workshop Manager Implementation
// ===========================================================================

class WorkshopManager::Impl {
public:
    Impl() : isRunning(false), activeCancel(false) {}
    ~Impl() { ShutdownWorker(); }

    void Init() {
        cl_workshop_api_url = Cvar_Get("cl_workshop_api_url", "https://api.powellslocker.com", CVAR_ARCHIVE);
        cl_workshop_enabled = Cvar_Get("cl_workshop_enabled", "1", CVAR_ARCHIVE);
        cl_workshop_auto_mount = Cvar_Get("cl_workshop_auto_mount", "1", CVAR_ARCHIVE);
        cl_workshop_timeout = Cvar_Get("cl_workshop_timeout", "30", CVAR_ARCHIVE);

        StartWorker();
        ScanInstalled();
    }

    void Shutdown() {
        ShutdownWorker();
    }

    void Frame() {
        // Dispatch pending main-thread callbacks
        std::vector<std::function<void()>> callbacksToRun;
        {
            std::lock_guard<std::mutex> lock(mainCallbackMutex);
            while (!mainCallbacks.empty()) {
                callbacksToRun.push_back(std::move(mainCallbacks.front()));
                mainCallbacks.pop();
            }
        }

        for (auto& cb : callbacksToRun) {
            if (cb) cb();
        }
    }

    void StartWorker() {
        if (isRunning) return;
        isRunning = true;
        workerThread = std::thread(&Impl::WorkerLoop, this);
    }

    void ShutdownWorker() {
        if (!isRunning) return;
        CancelActiveDownload();

        {
            std::lock_guard<std::mutex> lock(taskMutex);
            isRunning = false;
            taskCv.notify_all();
        }

        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    void PostTask(std::function<void()> task) {
        if (!isRunning) StartWorker();
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            tasks.push(std::move(task));
            taskCv.notify_one();
        }
    }

    void PostMainCallback(std::function<void()> cb) {
        std::lock_guard<std::mutex> lock(mainCallbackMutex);
        mainCallbacks.push(std::move(cb));
    }

    void WorkerLoop() {
        while (isRunning) {
            std::function<void()> currentTask;
            {
                std::unique_lock<std::mutex> lock(taskMutex);
                taskCv.wait(lock, [this] {
                    return !tasks.empty() || !isRunning;
                });

                if (!isRunning) break;
                if (!tasks.empty()) {
                    currentTask = std::move(tasks.front());
                    tasks.pop();
                }
            }

            if (currentTask) {
                currentTask();
            }
        }
    }

    // HTTP GET Request
    HttpResponse PerformHttpGet(const std::string& url) {
        HttpResponse res;
#ifdef USE_HTTP
        if (!Com_IsCurlImportValid(&curlImport)) {
            res.errorMsg = "cURL is not available in this build.";
            return res;
        }

        CURL *curl = curlImport.qcurl_easy_init();
        if (!curl) {
            res.errorMsg = "Failed to initialize cURL easy handle.";
            return res;
        }

        long timeoutSec = cl_workshop_timeout ? cl_workshop_timeout->integer : 30;
        if (timeoutSec <= 0) timeoutSec = 30;

        curlImport.qcurl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curlImport.qcurl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curlImport.qcurl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
        curlImport.qcurl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
        curlImport.qcurl_easy_setopt(curl, CURLOPT_USERAGENT, "OpenMoHAA-Workshop/1.0");
        curlImport.qcurl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Workshop_WriteMemoryCallback);
        curlImport.qcurl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);

        struct curl_slist *headers = nullptr;
        headers = curlImport.qcurl_slist_append(headers, "Accept: application/json");
        curlImport.qcurl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode code = curlImport.qcurl_easy_perform(curl);
        curlImport.qcurl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res.statusCode);

        if (headers) {
            curlImport.qcurl_slist_free_all(headers);
        }
        curlImport.qcurl_easy_cleanup(curl);

        if (code == CURLE_OK && (res.statusCode >= 200 && res.statusCode < 300)) {
            res.success = true;
        } else {
            res.success = false;
            res.errorMsg = curlImport.qcurl_easy_strerror(code);
        }
#else
        res.errorMsg = "Project compiled without USE_HTTP.";
#endif
        return res;
    }

    std::string GetApiBaseUrl() const {
        if (cl_workshop_api_url && cl_workshop_api_url->string[0]) {
            std::string url = cl_workshop_api_url->string;
            while (!url.empty() && url.back() == '/') url.pop_back();
            return url;
        }
        return "https://api.powellslocker.com";
    }

    void Search(const std::string& query,
                WorkshopContentType type,
                const std::string& gameType,
                int page,
                int size,
                WorkshopSearchCallback callback)
    {
        if (cl_workshop_enabled && !cl_workshop_enabled->integer) {
            PostMainCallback([callback]() {
                if (callback) callback(false, {}, 0);
            });
            return;
        }

        PostTask([this, query, type, gameType, page, size, callback]() {
            std::string baseUrl = GetApiBaseUrl();
            std::vector<WorkshopItem> results;
            int totalElements = 0;
            bool overallSuccess = false;

            std::string gTypeParam;
            if (!gameType.empty() && gameType != "ALL" && gameType != "All") {
                gTypeParam = "&gameType=" + UrlEncode(gameType);
            }

            if (type == WorkshopContentType::MAP) {
                std::string url = baseUrl + "/api/v1/maps?page=" + std::to_string(page) +
                                  "&size=" + std::to_string(size) + gTypeParam;
                if (!query.empty()) {
                    url += "&mapName=" + UrlEncode(query);
                }

                HttpResponse res = PerformHttpGet(url);
                if (res.success) {
                    try {
                        json data = json::parse(res.body);
                        if (data.is_object() && data.contains("content") && data["content"].is_array()) {
                            totalElements = (int)JsonGetInt64(data, "totalElements", data["content"].size());
                            for (const auto& itemJson : data["content"]) {
                                WorkshopItem item = ParseItemFromJson(itemJson, "Map");
                                CheckLocalInstallation(item);
                                results.push_back(item);
                            }
                            overallSuccess = true;
                        } else if (data.is_array()) {
                            totalElements = (int)data.size();
                            for (const auto& itemJson : data) {
                                WorkshopItem item = ParseItemFromJson(itemJson, "Map");
                                CheckLocalInstallation(item);
                                results.push_back(item);
                            }
                            overallSuccess = true;
                        }
                    } catch (const std::exception& e) {
                        Com_DPrintf("Workshop JSON error: %s\n", e.what());
                    }
                }
            } else if (type == WorkshopContentType::MOD) {
                std::string url = baseUrl + "/api/v1/mods?page=" + std::to_string(page) +
                                  "&size=" + std::to_string(size) + gTypeParam;
                if (!query.empty()) {
                    url += "&modName=" + UrlEncode(query);
                }

                HttpResponse res = PerformHttpGet(url);
                if (res.success) {
                    try {
                        json data = json::parse(res.body);
                        if (data.is_object() && data.contains("content") && data["content"].is_array()) {
                            totalElements = (int)JsonGetInt64(data, "totalElements", data["content"].size());
                            for (const auto& itemJson : data["content"]) {
                                WorkshopItem item = ParseItemFromJson(itemJson, "Mod");
                                CheckLocalInstallation(item);
                                results.push_back(item);
                            }
                            overallSuccess = true;
                        }
                    } catch (const std::exception& e) {
                        Com_DPrintf("Workshop JSON error: %s\n", e.what());
                    }
                }
            } else if (type == WorkshopContentType::COLLECTION) {
                std::string url = baseUrl + "/api/v1/collections?page=" + std::to_string(page) +
                                  "&size=" + std::to_string(size) + gTypeParam;
                HttpResponse res = PerformHttpGet(url);
                if (res.success) {
                    try {
                        json data = json::parse(res.body);
                        if (data.is_object() && data.contains("content") && data["content"].is_array()) {
                            totalElements = (int)JsonGetInt64(data, "totalElements", data["content"].size());
                            for (const auto& cJson : data["content"]) {
                                WorkshopItem item;
                                item.id = JsonGetInt64(cJson, "id");
                                item.title = JsonGetString(cJson, "title", "Collection");
                                item.author = JsonGetString(cJson, "authorName", "Unknown");
                                item.description = JsonGetString(cJson, "description");
                                item.gameType = JsonGetString(cJson, "gameType", "MOHAA");
                                item.contentType = "Collection";
                                item.category = "Collection";
                                item.filename = JsonGetString(cJson, "slug", std::to_string(item.id));
                                item.fileSize = JsonGetInt64(cJson, "totalFilesize");
                                results.push_back(item);
                            }
                            overallSuccess = true;
                        }
                    } catch (const std::exception& e) {
                        Com_DPrintf("Workshop JSON error: %s\n", e.what());
                    }
                }
            } else {
                // ALL: Query search endpoint or maps + mods
                std::string url = baseUrl + "/api/v1/search?query=" + UrlEncode(query.empty() ? "a" : query) +
                                  "&limit=" + std::to_string(size);
                HttpResponse res = PerformHttpGet(url);
                if (res.success) {
                    try {
                        json data = json::parse(res.body);
                        if (data.is_object()) {
                            if (data.contains("maps") && data["maps"].is_array()) {
                                for (const auto& mJson : data["maps"]) {
                                    WorkshopItem item = ParseItemFromJson(mJson, "Map");
                                    CheckLocalInstallation(item);
                                    results.push_back(item);
                                }
                            }
                            if (data.contains("mods") && data["mods"].is_array()) {
                                for (const auto& mJson : data["mods"]) {
                                    WorkshopItem item = ParseItemFromJson(mJson, "Mod");
                                    CheckLocalInstallation(item);
                                    results.push_back(item);
                                }
                            }
                            totalElements = (int)results.size();
                            overallSuccess = true;
                        }
                    } catch (const std::exception& e) {
                        Com_DPrintf("Workshop JSON error: %s\n", e.what());
                    }
                }

                // Fallback to maps search if search endpoint failed or empty
                if (results.empty()) {
                    std::string mapUrl = baseUrl + "/api/v1/maps?page=" + std::to_string(page) +
                                         "&size=" + std::to_string(size) + gTypeParam;
                    if (!query.empty()) mapUrl += "&mapName=" + UrlEncode(query);
                    HttpResponse mapRes = PerformHttpGet(mapUrl);
                    if (mapRes.success) {
                        try {
                            json mdata = json::parse(mapRes.body);
                            if (mdata.is_object() && mdata.contains("content") && mdata["content"].is_array()) {
                                totalElements = (int)JsonGetInt64(mdata, "totalElements", mdata["content"].size());
                                for (const auto& itemJson : mdata["content"]) {
                                    WorkshopItem item = ParseItemFromJson(itemJson, "Map");
                                    CheckLocalInstallation(item);
                                    results.push_back(item);
                                }
                                overallSuccess = true;
                            }
                        } catch (...) {}
                    }
                }
            }

            PostMainCallback([callback, overallSuccess, results, totalElements]() {
                if (callback) callback(overallSuccess, results, totalElements);
            });
        });
    }

    void GetFeatured(WorkshopSearchCallback callback) {
        PostTask([this, callback]() {
            std::string baseUrl = GetApiBaseUrl();
            std::vector<WorkshopItem> results;
            bool overallSuccess = false;

            std::string url = baseUrl + "/api/v1/maps?page=0&size=20&sort=downloads,desc";
            HttpResponse res = PerformHttpGet(url);
            if (!res.success) {
                url = baseUrl + "/api/v1/maps/random?limit=15";
                res = PerformHttpGet(url);
            }

            if (res.success) {
                try {
                    json data = json::parse(res.body);
                    if (data.is_object() && data.contains("content") && data["content"].is_array()) {
                        for (const auto& itemJson : data["content"]) {
                            WorkshopItem item = ParseItemFromJson(itemJson, "Map");
                            CheckLocalInstallation(item);
                            results.push_back(item);
                        }
                        overallSuccess = true;
                    } else if (data.is_array()) {
                        for (const auto& itemJson : data) {
                            WorkshopItem item = ParseItemFromJson(itemJson, "Map");
                            CheckLocalInstallation(item);
                            results.push_back(item);
                        }
                        overallSuccess = true;
                    }
                } catch (const std::exception& e) {
                    Com_DPrintf("Workshop JSON error: %s\n", e.what());
                }
            }

            PostMainCallback([callback, overallSuccess, results]() {
                if (callback) callback(overallSuccess, results, (int)results.size());
            });
        });
    }

    void GetItemDetails(int64_t id, WorkshopContentType type, WorkshopItemCallback callback) {
        PostTask([this, id, type, callback]() {
            std::string baseUrl = GetApiBaseUrl();
            std::string endpoint = (type == WorkshopContentType::MOD) ? "/api/v1/mods/" : "/api/v1/maps/";
            std::string url = baseUrl + endpoint + std::to_string(id);

            HttpResponse res = PerformHttpGet(url);
            WorkshopItem item;
            bool success = false;

            if (res.success) {
                try {
                    json data = json::parse(res.body);
                    if (data.is_object()) {
                        item = ParseItemFromJson(data, (type == WorkshopContentType::MOD) ? "Mod" : "Map");
                        CheckLocalInstallation(item);
                        success = true;
                    }
                } catch (const std::exception& e) {
                    Com_DPrintf("Workshop JSON error: %s\n", e.what());
                }
            }

            PostMainCallback([callback, success, item]() {
                if (callback) callback(success, item);
            });
        });
    }

    void SyncCollection(const std::string& slugOrId, WorkshopSearchCallback callback) {
        PostTask([this, slugOrId, callback]() {
            std::string baseUrl = GetApiBaseUrl();
            std::string url = baseUrl + "/api/v1/collections/" + UrlEncode(slugOrId);
            HttpResponse res = PerformHttpGet(url);
            std::vector<WorkshopItem> items;
            bool success = false;

            if (res.success) {
                try {
                    json data = json::parse(res.body);
                    if (data.is_object() && data.contains("items") && data["items"].is_array()) {
                        for (const auto& it : data["items"]) {
                            WorkshopItem item;
                            item.id = JsonGetInt64(it, "targetId");
                            item.title = JsonGetString(it, "name", "Collection Item");
                            item.author = JsonGetString(it, "creator", "Unknown");
                            item.contentType = JsonGetString(it, "targetType", "Mod");
                            item.filename = JsonGetString(it, "filename");
                            item.fileSize = JsonGetInt64(it, "filesize");
                            item.downloadUrl = JsonGetString(it, "downloadUrl");
                            item.previewImageUrl = JsonGetString(it, "thumbnail");
                            CheckLocalInstallation(item);
                            items.push_back(item);
                        }
                        success = true;
                    }
                } catch (const std::exception& e) {
                    Com_DPrintf("Workshop collection JSON error: %s\n", e.what());
                }
            }

            PostMainCallback([callback, success, items]() {
                if (callback) callback(success, items, (int)items.size());
            });
        });
    }

    // =======================================================================
    // Downloads & Streaming
    // =======================================================================

    void StartDownload(const WorkshopItem& item, bool autoPlay) {
        CancelActiveDownload();

        {
            std::lock_guard<std::mutex> lock(downloadMutex);
            activeCancel = false;
            activeProgress.itemId = item.id;
            activeProgress.title = item.title;
            activeProgress.filename = item.filename;
            activeProgress.state = WorkshopDownloadState::FETCHING_URL;
            activeProgress.bytesCurrent = 0.0;
            activeProgress.bytesTotal = (double)item.fileSize;
            activeProgress.percentage = 0.0f;
            activeProgress.speedBytesPerSec = 0.0;
            activeProgress.statusMessage = "Preparing download...";
            activeProgress.autoPlayAfterDownload = autoPlay;
            activeProgress.mapNameToLaunch = item.mapName;
        }

        PostTask([this, item, autoPlay]() {
            ExecuteDownloadTask(item, autoPlay);
        });
    }

    void CancelActiveDownload() {
        std::lock_guard<std::mutex> lock(downloadMutex);
        activeCancel = true;
        if (activeProgress.state == WorkshopDownloadState::DOWNLOADING ||
            activeProgress.state == WorkshopDownloadState::FETCHING_URL) {
            activeProgress.state = WorkshopDownloadState::CANCELLED;
            activeProgress.statusMessage = "Download cancelled.";
        }
    }

    WorkshopDownloadProgress GetCurrentProgress() {
        std::lock_guard<std::mutex> lock(downloadMutex);
        return activeProgress;
    }

    void ExecuteDownloadTask(const WorkshopItem& item, bool autoPlay) {
        std::string finalDownloadUrl = item.downloadUrl;
        std::string baseUrl = GetApiBaseUrl();

        if (finalDownloadUrl.empty()) {
            std::string endpoint = (item.contentType == "Mod" || item.contentType == "Skin" ||
                                     item.contentType == "Weapon" || item.contentType == "Sound" ||
                                     item.contentType == "Script") ? "/api/v1/downloads/mods/" : "/api/v1/downloads/maps/";
            finalDownloadUrl = baseUrl + endpoint + std::to_string(item.id);
        } else if (finalDownloadUrl.find("://") == std::string::npos) {
            if (finalDownloadUrl.front() != '/') finalDownloadUrl = "/" + finalDownloadUrl;
            finalDownloadUrl = baseUrl + finalDownloadUrl;
        }

        std::string targetDir = GetTargetGameDir(item.gameType);
        std::string finalPath = GetFullDownloadPath(item.filename, item.gameType);
        std::string tempPath = finalPath + ".tmp";

        // Create parent directories if needed
        FS_CreatePath(tempPath.c_str());

        {
            std::lock_guard<std::mutex> lock(downloadMutex);
            if (activeCancel) return;
            activeProgress.state = WorkshopDownloadState::DOWNLOADING;
            activeProgress.statusMessage = "Connecting to server...";
        }

        FILE *fp = fopen(tempPath.c_str(), "wb");
        if (!fp) {
            std::lock_guard<std::mutex> lock(downloadMutex);
            activeProgress.state = WorkshopDownloadState::FAILED;
            activeProgress.statusMessage = "Failed to open local destination file.";
            Com_Printf("^1[Workshop] Error: Failed to open %s for writing\n", tempPath.c_str());
            return;
        }

        bool success = false;
        long responseCode = 0;

#ifdef USE_HTTP
        if (Com_IsCurlImportValid(&curlImport)) {
            CURL *curl = curlImport.qcurl_easy_init();
            if (curl) {
                DownloadStreamContext ctx;
                ctx.file = fp;
                ctx.tempPath = tempPath;
                ctx.progress = &activeProgress;
                ctx.progressMutex = &downloadMutex;
                ctx.cancelRequested = &activeCancel;
                ctx.lastTime = std::chrono::steady_clock::now();
                ctx.lastBytes = 0.0;

                long timeoutSec = cl_workshop_timeout ? cl_workshop_timeout->integer : 30;
                if (timeoutSec <= 0) timeoutSec = 30;

                curlImport.qcurl_easy_setopt(curl, CURLOPT_URL, finalDownloadUrl.c_str());
                curlImport.qcurl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curlImport.qcurl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
                curlImport.qcurl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec * 10); // Allow longer for large downloads
                curlImport.qcurl_easy_setopt(curl, CURLOPT_USERAGENT, "OpenMoHAA-Workshop/1.0");
                curlImport.qcurl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Workshop_WriteFileCallback);
                curlImport.qcurl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
                curlImport.qcurl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                curlImport.qcurl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, Workshop_ProgressCallback);
                curlImport.qcurl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &ctx);

                CURLcode res = curlImport.qcurl_easy_perform(curl);
                curlImport.qcurl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
                curlImport.qcurl_easy_cleanup(curl);

                if (res == CURLE_OK && responseCode >= 200 && responseCode < 300 && !activeCancel) {
                    success = true;
                }
            }
        }
#endif
        fclose(fp);

        if (activeCancel) {
            remove(tempPath.c_str());
            std::lock_guard<std::mutex> lock(downloadMutex);
            activeProgress.state = WorkshopDownloadState::CANCELLED;
            activeProgress.statusMessage = "Download cancelled.";
            return;
        }

        if (!success) {
            remove(tempPath.c_str());
            std::lock_guard<std::mutex> lock(downloadMutex);
            activeProgress.state = WorkshopDownloadState::FAILED;
            activeProgress.statusMessage = "Download failed (HTTP " + std::to_string(responseCode) + ").";
            Com_Printf("^1[Workshop] Error: Download failed with HTTP status %ld\n", responseCode);
            return;
        }

        // Rename temp to target PK3
        remove(finalPath.c_str());
        if (rename(tempPath.c_str(), finalPath.c_str()) != 0) {
            std::lock_guard<std::mutex> lock(downloadMutex);
            activeProgress.state = WorkshopDownloadState::FAILED;
            activeProgress.statusMessage = "Failed to rename temp file to .pk3";
            return;
        }

        // Hot mount into virtual filesystem
        if (cl_workshop_auto_mount && cl_workshop_auto_mount->integer) {
            FS_AddPakFile(finalPath.c_str(), targetDir.c_str());
        }

        // Refresh local installation cache
        ScanInstalled();

        {
            std::lock_guard<std::mutex> lock(downloadMutex);
            activeProgress.state = WorkshopDownloadState::COMPLETED;
            activeProgress.percentage = 100.0f;
            activeProgress.statusMessage = "Installation complete!";
        }

        Com_Printf("^2[Workshop] Successfully installed: %s\n", item.filename.c_str());

        // Auto launch map if requested
        if (autoPlay && !item.mapName.empty()) {
            std::string launchCmd = "map " + item.mapName + "\n";
            PostMainCallback([launchCmd]() {
                Cbuf_AddText(launchCmd.c_str());
            });
        }
    }

    // =======================================================================
    // Local File & Installation Management
    // =======================================================================

    std::string GetTargetGameDir(const std::string& gameType) const {
        if (gameType == "MOHSH" || gameType == "Spearhead") return "mainta";
        if (gameType == "MOHBT" || gameType == "Breakthrough") return "maintt";
        return "main";
    }

    std::string GetFullDownloadPath(const std::string& filename, const std::string& gameType) const {
        std::string gameDir = GetTargetGameDir(gameType);
        cvar_t *basepath = Cvar_Get("fs_basepath", "", 0);
        cvar_t *homedatapath = Cvar_Get("fs_homedatapath", "", 0);

        const char *base = (homedatapath && homedatapath->string[0]) ? homedatapath->string :
                           ((basepath && basepath->string[0]) ? basepath->string : ".");

        char fullPath[MAX_OSPATH];
        Com_sprintf(fullPath, sizeof(fullPath), "%s/%s/%s", base, gameDir.c_str(), filename.c_str());
        Workshop_ReplaceSeparators(fullPath);
        return std::string(fullPath);
    }

    void CheckLocalInstallation(WorkshopItem& item) {
        std::string outPath;
        if (IsInstalled(item.filename, &outPath)) {
            item.isInstalled = true;
            item.localFilePath = outPath;
        } else {
            item.isInstalled = false;
            item.localFilePath.clear();
        }
    }

    bool IsInstalled(const std::string& filename, std::string* outPath) {
        std::lock_guard<std::mutex> lock(installedMutex);
        auto it = installedFiles.find(filename);
        if (it != installedFiles.end()) {
            if (outPath) *outPath = it->second;
            return true;
        }
        return false;
    }

    void ScanInstalled() {
        std::lock_guard<std::mutex> lock(installedMutex);
        installedFiles.clear();

        const char *dirs[] = { "main", "mainta", "maintt" };
        cvar_t *basepath = Cvar_Get("fs_basepath", "", 0);
        cvar_t *homedatapath = Cvar_Get("fs_homedatapath", "", 0);

        const char *bases[] = {
            (homedatapath && homedatapath->string[0]) ? homedatapath->string : nullptr,
            (basepath && basepath->string[0]) ? basepath->string : nullptr,
            "."
        };

        for (const char *base : bases) {
            if (!base) continue;
            for (const char *d : dirs) {
                int numFiles = 0;
                char **files = Sys_ListFiles(va("%s/%s", base, d), ".pk3", NULL, &numFiles, qfalse);
                if (files) {
                    for (int i = 0; i < numFiles; i++) {
                        std::string fname = files[i];
                        char full[MAX_OSPATH];
                        Com_sprintf(full, sizeof(full), "%s/%s/%s", base, d, fname.c_str());
                        Workshop_ReplaceSeparators(full);
                        installedFiles[fname] = full;
                    }
                    Sys_FreeFileList(files);
                }
            }
        }
    }

    std::vector<WorkshopItem> GetInstalledList() {
        ScanInstalled();
        std::lock_guard<std::mutex> lock(installedMutex);
        std::vector<WorkshopItem> list;

        for (const auto& pair : installedFiles) {
            WorkshopItem it;
            it.filename = pair.first;
            it.title = pair.first;
            it.localFilePath = pair.second;
            it.isInstalled = true;
            it.contentType = "PK3";
            it.category = "Installed";
            if (it.title.size() > 4 && it.title.substr(it.title.size() - 4) == ".pk3") {
                it.mapName = it.title.substr(0, it.title.size() - 4);
            }
            list.push_back(it);
        }
        return list;
    }

    bool Uninstall(const std::string& filename) {
        std::string path;
        if (!IsInstalled(filename, &path)) {
            return false;
        }

        if (remove(path.c_str()) == 0) {
            ScanInstalled();
            Com_Printf("^2[Workshop] Removed package: %s\n", path.c_str());
            return true;
        }

        Com_Printf("^1[Workshop] Failed to remove package: %s\n", path.c_str());
        return false;
    }

private:
    bool isRunning;
    std::thread workerThread;
    std::queue<std::function<void()>> tasks;
    std::mutex taskMutex;
    std::condition_variable taskCv;

    std::queue<std::function<void()>> mainCallbacks;
    std::mutex mainCallbackMutex;

    WorkshopDownloadProgress activeProgress;
    bool activeCancel;
    std::mutex downloadMutex;

    std::map<std::string, std::string> installedFiles;
    std::mutex installedMutex;
};

// ===========================================================================
// WorkshopManager Singleton Wrapper
// ===========================================================================

WorkshopManager::WorkshopManager() : pImpl(std::make_unique<Impl>()) {}
WorkshopManager::~WorkshopManager() = default;

WorkshopManager& WorkshopManager::Instance() {
    static WorkshopManager s_instance;
    return s_instance;
}

void WorkshopManager::Init() { pImpl->Init(); }
void WorkshopManager::Frame() { pImpl->Frame(); }
void WorkshopManager::Shutdown() { pImpl->Shutdown(); }

void WorkshopManager::Search(const std::string& query,
                             WorkshopContentType type,
                             const std::string& gameType,
                             int page,
                             int size,
                             WorkshopSearchCallback callback) {
    pImpl->Search(query, type, gameType, page, size, callback);
}

void WorkshopManager::GetFeatured(WorkshopSearchCallback callback) {
    pImpl->GetFeatured(callback);
}

void WorkshopManager::GetItemDetails(int64_t id,
                                     WorkshopContentType type,
                                     WorkshopItemCallback callback) {
    pImpl->GetItemDetails(id, type, callback);
}

void WorkshopManager::SyncCollection(const std::string& slugOrId,
                                     WorkshopSearchCallback callback) {
    pImpl->SyncCollection(slugOrId, callback);
}

void WorkshopManager::StartDownload(const WorkshopItem& item, bool autoPlay) {
    pImpl->StartDownload(item, autoPlay);
}

void WorkshopManager::CancelDownload() {
    pImpl->CancelActiveDownload();
}

WorkshopDownloadProgress WorkshopManager::GetCurrentProgress() {
    return pImpl->GetCurrentProgress();
}

void WorkshopManager::ScanInstalledItems() {
    pImpl->ScanInstalled();
}

bool WorkshopManager::IsItemInstalled(const std::string& filename, std::string* outPath) {
    return pImpl->IsInstalled(filename, outPath);
}

std::vector<WorkshopItem> WorkshopManager::GetInstalledItems() {
    return pImpl->GetInstalledList();
}

bool WorkshopManager::UninstallItem(const std::string& filename) {
    return pImpl->Uninstall(filename);
}

bool WorkshopManager::PlayMap(const std::string& mapName, const std::string& filename) {
    std::string finalMap = mapName;
    if (finalMap.empty() && !filename.empty()) {
        finalMap = filename;
        if (finalMap.size() > 4 && finalMap.substr(finalMap.size() - 4) == ".pk3") {
            finalMap = finalMap.substr(0, finalMap.size() - 4);
        }
    }
    if (!finalMap.empty()) {
        Cbuf_AddText(va("map %s\n", finalMap.c_str()));
        return true;
    }
    return false;
}

std::string WorkshopManager::GetTargetGameDir(const std::string& gameType) const {
    return pImpl->GetTargetGameDir(gameType);
}

std::string WorkshopManager::GetFullDownloadPath(const std::string& filename, const std::string& gameType) const {
    return pImpl->GetFullDownloadPath(filename, gameType);
}

// ===========================================================================
// Console Commands Implementation
// ===========================================================================

static void Locker_Search_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: locker search <query> [map|mod|collection|all] [mohaa|spearhead|breakthrough]\n");
        return;
    }

    std::string query = Cmd_Argv(1);
    WorkshopContentType type = WorkshopContentType::ALL;
    std::string gameType = "ALL";

    if (Cmd_Argc() >= 3) {
        std::string t = Cmd_Argv(2);
        if (t == "map" || t == "maps") type = WorkshopContentType::MAP;
        else if (t == "mod" || t == "mods") type = WorkshopContentType::MOD;
        else if (t == "collection" || t == "collections") type = WorkshopContentType::COLLECTION;
    }

    if (Cmd_Argc() >= 4) {
        std::string g = Cmd_Argv(3);
        if (g == "spearhead" || g == "sh") gameType = "MOHSH";
        else if (g == "breakthrough" || g == "bt") gameType = "MOHBT";
        else if (g == "mohaa" || g == "aa") gameType = "MOHAA";
    }

    Com_Printf("^3[Workshop] Searching for \"%s\"...\n", query.c_str());

    WorkshopManager::Instance().Search(query, type, gameType, 0, 20, [](bool success, const std::vector<WorkshopItem>& items, int total) {
        if (!success || items.empty()) {
            Com_Printf("^1[Workshop] No items found matching the query.\n");
            return;
        }

        Com_Printf("^2===== Powell's Locker Search Results (%d found) =====\n", total);
        for (const auto& it : items) {
            Com_Printf("^5[%lld]^7 %-30s ^3%-10s ^2%-8s ^6%s\n",
                       (long long)it.id,
                       it.title.c_str(),
                       it.contentType.c_str(),
                       FormatBytes((double)it.fileSize).c_str(),
                       it.isInstalled ? "^2[INSTALLED]" : "");
        }
        Com_Printf("^2=====================================================\n");
        Com_Printf("Use 'locker install map <id>' or 'locker play <id>' to download and launch.\n");
    });
}

static void Locker_Featured_f(void) {
    Com_Printf("^3[Workshop] Fetching featured content from Powell's Locker...\n");
    WorkshopManager::Instance().GetFeatured([](bool success, const std::vector<WorkshopItem>& items, int total) {
        if (!success || items.empty()) {
            Com_Printf("^1[Workshop] Failed to load featured items.\n");
            return;
        }

        Com_Printf("^2===== Powell's Locker Featured Items =====\n");
        for (const auto& it : items) {
            Com_Printf("^5[%lld]^7 %-30s ^3%-10s ^2%-8s %s\n",
                       (long long)it.id,
                       it.title.c_str(),
                       it.contentType.c_str(),
                       FormatBytes((double)it.fileSize).c_str(),
                       it.isInstalled ? "^2[INSTALLED]" : "");
        }
        Com_Printf("^2===========================================\n");
    });
}

static void Locker_Install_f(void) {
    if (Cmd_Argc() < 3) {
        Com_Printf("Usage: locker install <map|mod> <id>\n");
        return;
    }

    std::string typeStr = Cmd_Argv(1);
    WorkshopContentType type = (typeStr == "mod" || typeStr == "mods") ? WorkshopContentType::MOD : WorkshopContentType::MAP;
    int64_t id = std::stoll(Cmd_Argv(2));

    Com_Printf("^3[Workshop] Fetching details for item #%lld...\n", (long long)id);

    WorkshopManager::Instance().GetItemDetails(id, type, [id](bool success, const WorkshopItem& item) {
        if (!success) {
            Com_Printf("^1[Workshop] Could not retrieve metadata for item #%lld.\n", (long long)id);
            return;
        }

        Com_Printf("^2[Workshop] Starting download of %s (%s)...\n", item.title.c_str(), item.filename.c_str());
        WorkshopManager::Instance().StartDownload(item, false);
    });
}

static void Locker_Play_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: locker play <id>\n");
        return;
    }

    int64_t id = std::stoll(Cmd_Argv(1));
    Com_Printf("^3[Workshop] Fetching details for map #%lld...\n", (long long)id);

    WorkshopManager::Instance().GetItemDetails(id, WorkshopContentType::MAP, [id](bool success, const WorkshopItem& item) {
        if (!success) {
            Com_Printf("^1[Workshop] Could not retrieve map #%lld.\n", (long long)id);
            return;
        }

        if (item.isInstalled) {
            Com_Printf("^2[Workshop] Map already installed locally. Launching '%s'...\n", item.mapName.c_str());
            WorkshopManager::Instance().PlayMap(item.mapName, item.filename);
        } else {
            Com_Printf("^2[Workshop] Downloading map '%s' before launching...\n", item.title.c_str());
            WorkshopManager::Instance().StartDownload(item, true);
        }
    });
}

static void Locker_Progress_f(void) {
    WorkshopDownloadProgress p = WorkshopManager::Instance().GetCurrentProgress();
    if (p.state == WorkshopDownloadState::IDLE) {
        Com_Printf("[Workshop] No active download.\n");
    } else {
        Com_Printf("^3[Workshop] %s\n", p.statusMessage.c_str());
    }
}

static void Locker_Cancel_f(void) {
    WorkshopManager::Instance().CancelDownload();
    Com_Printf("^3[Workshop] Active download cancelled.\n");
}

static void Locker_ListInstalled_f(void) {
    auto items = WorkshopManager::Instance().GetInstalledItems();
    Com_Printf("^2===== Locally Installed Workshop Content (%d items) =====\n", (int)items.size());
    for (const auto& it : items) {
        Com_Printf("  ^7%-35s ^5%s\n", it.filename.c_str(), it.localFilePath.c_str());
    }
    Com_Printf("^2===========================================================\n");
}

static void Locker_Remove_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: locker remove <filename.pk3>\n");
        return;
    }
    std::string fname = Cmd_Argv(1);
    WorkshopManager::Instance().UninstallItem(fname);
}

static void Locker_Sync_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: locker sync <collection-slug-or-id>\n");
        return;
    }
    std::string slug = Cmd_Argv(1);
    Com_Printf("^3[Workshop] Syncing collection '%s'...\n", slug.c_str());

    WorkshopManager::Instance().SyncCollection(slug, [slug](bool success, const std::vector<WorkshopItem>& items, int total) {
        if (!success || items.empty()) {
            Com_Printf("^1[Workshop] Failed to load collection '%s'.\n", slug.c_str());
            return;
        }

        Com_Printf("^2[Workshop] Collection has %d items. Checking missing files...\n", (int)items.size());
        for (const auto& item : items) {
            if (!item.isInstalled) {
                Com_Printf("^3[Workshop] Downloading missing asset: %s\n", item.filename.c_str());
                WorkshopManager::Instance().StartDownload(item, false);
                break; // Download first missing item in sequence
            }
        }
    });
}

static void Locker_CommandDispatcher_f(void) {
    if (Cmd_Argc() < 2) {
        Com_Printf("Powell's Locker Workshop Commands:\n"
                   "  locker search <query> [map|mod] [mohaa|spearhead|breakthrough]\n"
                   "  locker featured\n"
                   "  locker install <map|mod> <id>\n"
                   "  locker play <id>\n"
                   "  locker progress\n"
                   "  locker cancel\n"
                   "  locker list_installed\n"
                   "  locker remove <filename.pk3>\n"
                   "  locker sync <collection-slug>\n"
                   "  locker ui / workshop\n");
        return;
    }

    std::string sub = Cmd_Argv(1);
    if (sub == "search") {
        Locker_Search_f();
    } else if (sub == "featured") {
        Locker_Featured_f();
    } else if (sub == "install") {
        Locker_Install_f();
    } else if (sub == "play") {
        Locker_Play_f();
    } else if (sub == "progress") {
        Locker_Progress_f();
    } else if (sub == "cancel") {
        Locker_Cancel_f();
    } else if (sub == "list_installed" || sub == "installed") {
        Locker_ListInstalled_f();
    } else if (sub == "remove" || sub == "uninstall") {
        Locker_Remove_f();
    } else if (sub == "sync") {
        Locker_Sync_f();
    } else if (sub == "ui") {
        UI_LaunchWorkshop_f();
    } else {
        Com_Printf("Unknown locker subcommand '%s'. Type 'locker' for help.\n", sub.c_str());
    }
}

// ===========================================================================
// Lifecycle Entry Points
// ===========================================================================

void CL_Workshop_Init(void) {
    WorkshopManager::Instance().Init();

    Cmd_AddCommand("locker", Locker_CommandDispatcher_f);
    Cmd_AddCommand("workshop", UI_LaunchWorkshop_f);
    Cmd_AddCommand("launchworkshop", UI_LaunchWorkshop_f);
}

void CL_Workshop_Frame(void) {
    WorkshopManager::Instance().Frame();
}

void CL_Workshop_Shutdown(void) {
    WorkshopManager::Instance().Shutdown();
}
