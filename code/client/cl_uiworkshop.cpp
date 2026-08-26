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

#include "cl_uiworkshop.h"
#include "../qcommon/localization.h"

// Format helper
static std::string FormatItemSize(int64_t bytes) {
    char buf[32];
    if (bytes >= 1024 * 1024 * 1024) {
        Com_sprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        Com_sprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        Com_sprintf(buf, sizeof(buf), "%.0f KB", bytes / 1024.0);
    } else if (bytes > 0) {
        Com_sprintf(buf, sizeof(buf), "%lld B", (long long)bytes);
    } else {
        return "-";
    }
    return std::string(buf);
}

// ===========================================================================
// WorkshopListItem
// ===========================================================================

WorkshopListItem::WorkshopListItem(const WorkshopItem& item)
    : m_item(item)
{
}

int WorkshopListItem::getListItemValue(int which) const {
    return (int)m_item.id;
}

griditemtype_t WorkshopListItem::getListItemType(int which) const {
    return griditemtype_t::TYPE_STRING;
}

str WorkshopListItem::getListItemString(int which) const {
    switch (which) {
    case 0:
        return m_item.title.c_str();
    case 1:
        return m_item.contentType.c_str();
    case 2:
        return FormatItemSize(m_item.fileSize).c_str();
    case 3:
        return m_item.isInstalled ? "[INSTALLED]" : "";
    default:
        return "";
    }
}

void WorkshopListItem::DrawListItem(int iColumn, const UIRect2D& drawRect, bool bSelected, UIFont *pFont) {
}

qboolean WorkshopListItem::IsHeaderEntry() const {
    return qfalse;
}

// ===========================================================================
// UIPowellsLockerWorkshop Events & Declarations
// ===========================================================================

Event UIPowellsLockerWorkshop::EV_Workshop_ItemSelected("workshop_item_selected", EV_DEFAULT, NULL, NULL, "Workshop item selected");
Event UIPowellsLockerWorkshop::EV_Workshop_ItemDoubleClicked("workshop_item_double_clicked", EV_DEFAULT, NULL, NULL, "Workshop item double clicked");
Event UIPowellsLockerWorkshop::EV_Workshop_TabFeatured("workshop_tab_featured", EV_DEFAULT, NULL, NULL, "Workshop switch to featured tab");
Event UIPowellsLockerWorkshop::EV_Workshop_TabMaps("workshop_tab_maps", EV_DEFAULT, NULL, NULL, "Workshop switch to maps tab");
Event UIPowellsLockerWorkshop::EV_Workshop_TabMods("workshop_tab_mods", EV_DEFAULT, NULL, NULL, "Workshop switch to mods tab");
Event UIPowellsLockerWorkshop::EV_Workshop_TabCollections("workshop_tab_collections", EV_DEFAULT, NULL, NULL, "Workshop switch to collections tab");
Event UIPowellsLockerWorkshop::EV_Workshop_TabInstalled("workshop_tab_installed", EV_DEFAULT, NULL, NULL, "Workshop switch to installed tab");
Event UIPowellsLockerWorkshop::EV_Workshop_ToggleGameFilter("workshop_toggle_game_filter", EV_DEFAULT, NULL, NULL, "Workshop toggle game filter");
Event UIPowellsLockerWorkshop::EV_Workshop_Search("workshop_search", EV_DEFAULT, NULL, NULL, "Workshop trigger search");
Event UIPowellsLockerWorkshop::EV_Workshop_Install("workshop_install", EV_DEFAULT, NULL, NULL, "Workshop install item");
Event UIPowellsLockerWorkshop::EV_Workshop_Play("workshop_play", EV_DEFAULT, NULL, NULL, "Workshop play item");
Event UIPowellsLockerWorkshop::EV_Workshop_Uninstall("workshop_uninstall", EV_DEFAULT, NULL, NULL, "Workshop uninstall item");
Event UIPowellsLockerWorkshop::EV_Workshop_CancelDownload("workshop_cancel_download", EV_DEFAULT, NULL, NULL, "Workshop cancel download");

CLASS_DECLARATION(UIFloatingWindow, UIPowellsLockerWorkshop, NULL) {
    {&W_Deactivated,                                    &UIPowellsLockerWorkshop::OnClose},
    {&UIPowellsLockerWorkshop::EV_Workshop_ItemSelected,       &UIPowellsLockerWorkshop::OnItemSelected},
    {&UIPowellsLockerWorkshop::EV_Workshop_ItemDoubleClicked,  &UIPowellsLockerWorkshop::OnItemDoubleClicked},
    {&UIPowellsLockerWorkshop::EV_Workshop_TabFeatured,        &UIPowellsLockerWorkshop::OnTabFeatured},
    {&UIPowellsLockerWorkshop::EV_Workshop_TabMaps,            &UIPowellsLockerWorkshop::OnTabMaps},
    {&UIPowellsLockerWorkshop::EV_Workshop_TabMods,            &UIPowellsLockerWorkshop::OnTabMods},
    {&UIPowellsLockerWorkshop::EV_Workshop_TabCollections,     &UIPowellsLockerWorkshop::OnTabCollections},
    {&UIPowellsLockerWorkshop::EV_Workshop_TabInstalled,       &UIPowellsLockerWorkshop::OnTabInstalled},
    {&UIPowellsLockerWorkshop::EV_Workshop_ToggleGameFilter,   &UIPowellsLockerWorkshop::OnToggleGameFilter},
    {&UIPowellsLockerWorkshop::EV_Workshop_Search,             &UIPowellsLockerWorkshop::OnSearch},
    {&UIPowellsLockerWorkshop::EV_Workshop_Install,            &UIPowellsLockerWorkshop::OnInstall},
    {&UIPowellsLockerWorkshop::EV_Workshop_Play,               &UIPowellsLockerWorkshop::OnPlay},
    {&UIPowellsLockerWorkshop::EV_Workshop_Uninstall,          &UIPowellsLockerWorkshop::OnUninstall},
    {&UIPowellsLockerWorkshop::EV_Workshop_CancelDownload,     &UIPowellsLockerWorkshop::OnCancelDownload},
    {NULL,                                              NULL}
};

UIPowellsLockerWorkshop::UIPowellsLockerWorkshop()
    : m_listbox(nullptr)
    , m_btnFeatured(nullptr)
    , m_btnMaps(nullptr)
    , m_btnMods(nullptr)
    , m_btnCollections(nullptr)
    , m_btnInstalled(nullptr)
    , m_btnGameFilter(nullptr)
    , m_btnSearch(nullptr)
    , m_lblTitle(nullptr)
    , m_lblMeta(nullptr)
    , m_lblDescription(nullptr)
    , m_btnInstall(nullptr)
    , m_btnPlay(nullptr)
    , m_btnUninstall(nullptr)
    , m_lblStatus(nullptr)
    , m_btnCancel(nullptr)
    , m_btnClose(nullptr)
    , m_selectedIndex(-1)
    , m_currentType(WorkshopContentType::ALL)
    , m_currentGameType("ALL")
    , m_gameFilterIndex(0)
    , m_lastProgressUpdate(0)
{
    AddFlag(WF_ALWAYS_TOP);
}

UIPowellsLockerWorkshop::~UIPowellsLockerWorkshop() {
    if (m_listbox) { delete m_listbox; m_listbox = nullptr; }
    if (m_btnFeatured) { delete m_btnFeatured; m_btnFeatured = nullptr; }
    if (m_btnMaps) { delete m_btnMaps; m_btnMaps = nullptr; }
    if (m_btnMods) { delete m_btnMods; m_btnMods = nullptr; }
    if (m_btnCollections) { delete m_btnCollections; m_btnCollections = nullptr; }
    if (m_btnInstalled) { delete m_btnInstalled; m_btnInstalled = nullptr; }
    if (m_btnGameFilter) { delete m_btnGameFilter; m_btnGameFilter = nullptr; }
    if (m_btnSearch) { delete m_btnSearch; m_btnSearch = nullptr; }
    if (m_lblTitle) { delete m_lblTitle; m_lblTitle = nullptr; }
    if (m_lblMeta) { delete m_lblMeta; m_lblMeta = nullptr; }
    if (m_lblDescription) { delete m_lblDescription; m_lblDescription = nullptr; }
    if (m_btnInstall) { delete m_btnInstall; m_btnInstall = nullptr; }
    if (m_btnPlay) { delete m_btnPlay; m_btnPlay = nullptr; }
    if (m_btnUninstall) { delete m_btnUninstall; m_btnUninstall = nullptr; }
    if (m_lblStatus) { delete m_lblStatus; m_lblStatus = nullptr; }
    if (m_btnCancel) { delete m_btnCancel; m_btnCancel = nullptr; }
    if (m_btnClose) { delete m_btnClose; m_btnClose = nullptr; }
}

void UIPowellsLockerWorkshop::Create(UIWidget *parent, const UIRect2D& rect, const char *title, const UColor& bgColor, const UColor& fgColor) {
    UIFloatingWindow::Create(parent, rect, title, bgColor, fgColor);

    // Remove or disable minimize button
    for (UIWidget *child = getFirstChild(); child; child = getNextChild(child)) {
        if (strcmp(child->getName(), "minimizebutton") == 0) {
            child->setShow(false);
            break;
        }
    }
}

void UIPowellsLockerWorkshop::FrameInitialized(void) {
    UIFloatingWindow::FrameInitialized();

    UIChildSpaceWidget *cs = getChildSpace();
    if (!cs) return;

    // --- Top Navigation Tabs ---
    // [ Featured ] [ Maps ] [ Mods ] [ Collections ] [ Installed ] | [ Game: ALL ] [ Search ]
    m_btnFeatured = new UIButton();
    m_btnFeatured->InitFrame(cs, UIRect2D(10, 10, 75, 24), 0);
    m_btnFeatured->setTitle("Featured");
    m_btnFeatured->AllowActivate(true);
    m_btnFeatured->Connect(this, W_Button_Pressed, EV_Workshop_TabFeatured);

    m_btnMaps = new UIButton();
    m_btnMaps->InitFrame(cs, UIRect2D(90, 10, 60, 24), 0);
    m_btnMaps->setTitle("Maps");
    m_btnMaps->AllowActivate(true);
    m_btnMaps->Connect(this, W_Button_Pressed, EV_Workshop_TabMaps);

    m_btnMods = new UIButton();
    m_btnMods->InitFrame(cs, UIRect2D(155, 10, 60, 24), 0);
    m_btnMods->setTitle("Mods");
    m_btnMods->AllowActivate(true);
    m_btnMods->Connect(this, W_Button_Pressed, EV_Workshop_TabMods);

    m_btnCollections = new UIButton();
    m_btnCollections->InitFrame(cs, UIRect2D(220, 10, 85, 24), 0);
    m_btnCollections->setTitle("Collections");
    m_btnCollections->AllowActivate(true);
    m_btnCollections->Connect(this, W_Button_Pressed, EV_Workshop_TabCollections);

    m_btnInstalled = new UIButton();
    m_btnInstalled->InitFrame(cs, UIRect2D(310, 10, 75, 24), 0);
    m_btnInstalled->setTitle("Installed");
    m_btnInstalled->AllowActivate(true);
    m_btnInstalled->Connect(this, W_Button_Pressed, EV_Workshop_TabInstalled);

    m_btnGameFilter = new UIButton();
    m_btnGameFilter->InitFrame(cs, UIRect2D(395, 10, 95, 24), 0);
    m_btnGameFilter->setTitle("Game: ALL");
    m_btnGameFilter->AllowActivate(true);
    m_btnGameFilter->Connect(this, W_Button_Pressed, EV_Workshop_ToggleGameFilter);

    m_btnSearch = new UIButton();
    m_btnSearch->InitFrame(cs, UIRect2D(495, 10, 75, 24), 0);
    m_btnSearch->setTitle("Refresh");
    m_btnSearch->AllowActivate(true);
    m_btnSearch->Connect(this, W_Button_Pressed, EV_Workshop_Search);

    // --- Left List Control ---
    m_listbox = new UIListCtrl();
    m_listbox->InitFrame(cs, UIRect2D(10, 42, 330, 290), 0);
    m_listbox->SetDrawHeader(true);
    m_listbox->setFont("facfont-20");
    m_listbox->FrameInitialized();
    m_listbox->AddColumn("Title / Name", 0, 160, false, false);
    m_listbox->AddColumn("Type", 1, 55, false, false);
    m_listbox->AddColumn("Size", 2, 55, false, false);
    m_listbox->AddColumn("Status", 3, 60, false, false);
    m_listbox->AllowActivate(true);
    m_listbox->SetDontLocalize();

    m_listbox->Connect(this, EV_UIListBase_ItemSelected, EV_Workshop_ItemSelected);
    m_listbox->Connect(this, EV_UIListBase_ItemDoubleClicked, EV_Workshop_ItemDoubleClicked);

    // --- Right Detail Panel ---
    m_lblTitle = new UILabel();
    m_lblTitle->InitFrame(cs, UIRect2D(350, 42, 220, 24), 0);
    m_lblTitle->setTitle("Select an item");
    m_lblTitle->setForegroundColor(UColor(1.0f, 0.85f, 0.3f));

    m_lblMeta = new UILabel();
    m_lblMeta->InitFrame(cs, UIRect2D(350, 68, 220, 48), 0);
    m_lblMeta->setTitle("");
    m_lblMeta->setForegroundColor(UHudColor);

    m_lblDescription = new UILabel();
    m_lblDescription->InitFrame(cs, UIRect2D(350, 118, 220, 150), 0);
    m_lblDescription->setTitle("Browse community maps, mods, and collections directly from Powell's Locker.");
    m_lblDescription->setForegroundColor(UColor(0.85f, 0.85f, 0.85f));

    // Detail Action Buttons
    m_btnInstall = new UIButton();
    m_btnInstall->InitFrame(cs, UIRect2D(350, 275, 105, 26), 0);
    m_btnInstall->setTitle("Install");
    m_btnInstall->AllowActivate(true);
    m_btnInstall->Connect(this, W_Button_Pressed, EV_Workshop_Install);

    m_btnPlay = new UIButton();
    m_btnPlay->InitFrame(cs, UIRect2D(465, 275, 105, 26), 0);
    m_btnPlay->setTitle("Play Map");
    m_btnPlay->AllowActivate(true);
    m_btnPlay->Connect(this, W_Button_Pressed, EV_Workshop_Play);

    m_btnUninstall = new UIButton();
    m_btnUninstall->InitFrame(cs, UIRect2D(350, 306, 105, 26), 0);
    m_btnUninstall->setTitle("Uninstall");
    m_btnUninstall->AllowActivate(true);
    m_btnUninstall->Connect(this, W_Button_Pressed, EV_Workshop_Uninstall);

    m_btnClose = new UIButton();
    m_btnClose->InitFrame(cs, UIRect2D(465, 306, 105, 26), 0);
    m_btnClose->setTitle("Close");
    m_btnClose->AllowActivate(true);
    m_btnClose->Connect(this, W_Button_Pressed, W_Deactivated);

    // --- Bottom Status Bar ---
    m_lblStatus = new UILabel();
    m_lblStatus->InitFrame(cs, UIRect2D(10, 340, 470, 24), 0);
    m_lblStatus->setTitle("Ready. Connected to Powell's Locker.");
    m_lblStatus->setForegroundColor(UColor(0.6f, 0.9f, 0.6f));

    m_btnCancel = new UIButton();
    m_btnCancel->InitFrame(cs, UIRect2D(490, 338, 80, 24), 0);
    m_btnCancel->setTitle("Cancel");
    m_btnCancel->AllowActivate(true);
    m_btnCancel->setShow(false);
    m_btnCancel->Connect(this, W_Button_Pressed, EV_Workshop_CancelDownload);

    // Initial load: featured items
    OnTabFeatured(nullptr);
}

void UIPowellsLockerWorkshop::UpdateUIElement(void) {
    UIFloatingWindow::UpdateUIElement();
    UpdateStatusDisplay();
}

void UIPowellsLockerWorkshop::Draw(void) {
    UIFloatingWindow::Draw();
}

void UIPowellsLockerWorkshop::UpdateStatusDisplay() {
    int now = Sys_Milliseconds();
    if (now - m_lastProgressUpdate < 200) {
        return;
    }
    m_lastProgressUpdate = now;

    WorkshopDownloadProgress p = WorkshopManager::Instance().GetCurrentProgress();
    if (p.state == WorkshopDownloadState::DOWNLOADING || p.state == WorkshopDownloadState::FETCHING_URL) {
        if (m_lblStatus) {
            m_lblStatus->setTitle(p.statusMessage.c_str());
            m_lblStatus->setForegroundColor(UColor(1.0f, 0.9f, 0.3f));
        }
        if (m_btnCancel) {
            m_btnCancel->setShow(true);
        }
    } else if (p.state == WorkshopDownloadState::COMPLETED) {
        if (m_lblStatus) {
            m_lblStatus->setTitle("Installation complete!");
            m_lblStatus->setForegroundColor(UColor(0.3f, 1.0f, 0.3f));
        }
        if (m_btnCancel) {
            m_btnCancel->setShow(false);
        }
        RefreshList();
    } else if (p.state == WorkshopDownloadState::FAILED) {
        if (m_lblStatus) {
            m_lblStatus->setTitle(p.statusMessage.c_str());
            m_lblStatus->setForegroundColor(UColor(1.0f, 0.3f, 0.3f));
        }
        if (m_btnCancel) {
            m_btnCancel->setShow(false);
        }
    } else if (p.state == WorkshopDownloadState::CANCELLED) {
        if (m_lblStatus) {
            m_lblStatus->setTitle("Download cancelled.");
            m_lblStatus->setForegroundColor(UColor(0.8f, 0.8f, 0.8f));
        }
        if (m_btnCancel) {
            m_btnCancel->setShow(false);
        }
    }
}

void UIPowellsLockerWorkshop::SetItems(const std::vector<WorkshopItem>& items) {
    m_items = items;
    m_selectedIndex = -1;

    if (!m_listbox) return;

    m_listbox->DeleteAllItems();
    for (const auto& it : m_items) {
        m_listbox->AddItem(new WorkshopListItem(it));
    }

    if (!m_items.empty()) {
        m_selectedIndex = 0;
        UpdateDetailPanel();
    } else {
        if (m_lblTitle) m_lblTitle->setTitle("No items found");
        if (m_lblMeta) m_lblMeta->setTitle("");
        if (m_lblDescription) m_lblDescription->setTitle("Try refining your search or changing the game filter.");
    }
}

void UIPowellsLockerWorkshop::RefreshList() {
    if (m_currentType == WorkshopContentType::ALL) {
        OnTabFeatured(nullptr);
    } else if (m_currentType == WorkshopContentType::MAP) {
        OnTabMaps(nullptr);
    } else if (m_currentType == WorkshopContentType::MOD) {
        OnTabMods(nullptr);
    } else if (m_currentType == WorkshopContentType::COLLECTION) {
        OnTabCollections(nullptr);
    } else {
        OnTabInstalled(nullptr);
    }
}

void UIPowellsLockerWorkshop::UpdateDetailPanel() {
    if (m_selectedIndex < 0 || m_selectedIndex >= (int)m_items.size()) {
        return;
    }

    const WorkshopItem& it = m_items[m_selectedIndex];

    if (m_lblTitle) {
        m_lblTitle->setTitle(it.title.c_str());
    }

    if (m_lblMeta) {
        char metaBuf[256];
        Com_sprintf(metaBuf, sizeof(metaBuf),
                    "Author: %s | Ver: %s\nGame: %s | Category: %s\nSize: %s | Downloads: %d",
                    it.author.c_str(),
                    it.version.c_str(),
                    it.gameType.c_str(),
                    it.category.c_str(),
                    FormatItemSize(it.fileSize).c_str(),
                    it.downloadCount);
        m_lblMeta->setTitle(metaBuf);
    }

    if (m_lblDescription) {
        std::string desc = it.description.empty() ? it.shortDescription : it.description;
        if (desc.empty()) desc = "No description provided for this item.";
        m_lblDescription->setTitle(desc.c_str());
    }

    if (m_btnInstall) {
        m_btnInstall->setTitle(it.isInstalled ? "Reinstall" : "Install");
    }

    if (m_btnPlay) {
        m_btnPlay->setShow(it.contentType == "Map" || (it.contentType == "PK3" && !it.mapName.empty()));
    }

    if (m_btnUninstall) {
        m_btnUninstall->setShow(it.isInstalled);
    }
}

// ===========================================================================
// Event Handlers
// ===========================================================================

void UIPowellsLockerWorkshop::OnItemSelected(Event *ev) {
    if (!m_listbox) return;
    int cur = m_listbox->getCurrentItem();
    if (cur >= 0 && cur < (int)m_items.size()) {
        m_selectedIndex = cur;
        UpdateDetailPanel();
    }
}

void UIPowellsLockerWorkshop::OnItemDoubleClicked(Event *ev) {
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_items.size()) {
        const WorkshopItem& it = m_items[m_selectedIndex];
        if (it.isInstalled && (it.contentType == "Map" || !it.mapName.empty())) {
            OnPlay(nullptr);
        } else {
            OnInstall(nullptr);
        }
    }
}

void UIPowellsLockerWorkshop::OnTabFeatured(Event *ev) {
    m_currentType = WorkshopContentType::ALL;
    if (m_lblStatus) m_lblStatus->setTitle("Loading featured items...");

    WorkshopManager::Instance().GetFeatured([this](bool success, const std::vector<WorkshopItem>& items, int total) {
        SetItems(items);
        if (m_lblStatus) {
            char buf[128];
            Com_sprintf(buf, sizeof(buf), "Loaded %d featured items.", (int)items.size());
            m_lblStatus->setTitle(buf);
            m_lblStatus->setForegroundColor(UColor(0.6f, 0.9f, 0.6f));
        }
    });
}

void UIPowellsLockerWorkshop::OnTabMaps(Event *ev) {
    m_currentType = WorkshopContentType::MAP;
    if (m_lblStatus) m_lblStatus->setTitle("Searching maps...");

    WorkshopManager::Instance().Search("", WorkshopContentType::MAP, m_currentGameType, 0, 20, [this](bool success, const std::vector<WorkshopItem>& items, int total) {
        SetItems(items);
        if (m_lblStatus) {
            char buf[128];
            Com_sprintf(buf, sizeof(buf), "Loaded %d maps (Game: %s).", (int)items.size(), m_currentGameType.c_str());
            m_lblStatus->setTitle(buf);
            m_lblStatus->setForegroundColor(UColor(0.6f, 0.9f, 0.6f));
        }
    });
}

void UIPowellsLockerWorkshop::OnTabMods(Event *ev) {
    m_currentType = WorkshopContentType::MOD;
    if (m_lblStatus) m_lblStatus->setTitle("Searching mods...");

    WorkshopManager::Instance().Search("", WorkshopContentType::MOD, m_currentGameType, 0, 20, [this](bool success, const std::vector<WorkshopItem>& items, int total) {
        SetItems(items);
        if (m_lblStatus) {
            char buf[128];
            Com_sprintf(buf, sizeof(buf), "Loaded %d mods (Game: %s).", (int)items.size(), m_currentGameType.c_str());
            m_lblStatus->setTitle(buf);
            m_lblStatus->setForegroundColor(UColor(0.6f, 0.9f, 0.6f));
        }
    });
}

void UIPowellsLockerWorkshop::OnTabCollections(Event *ev) {
    m_currentType = WorkshopContentType::COLLECTION;
    if (m_lblStatus) m_lblStatus->setTitle("Searching collections...");

    WorkshopManager::Instance().Search("", WorkshopContentType::COLLECTION, m_currentGameType, 0, 20, [this](bool success, const std::vector<WorkshopItem>& items, int total) {
        SetItems(items);
        if (m_lblStatus) {
            char buf[128];
            Com_sprintf(buf, sizeof(buf), "Loaded %d collections.", (int)items.size());
            m_lblStatus->setTitle(buf);
            m_lblStatus->setForegroundColor(UColor(0.6f, 0.9f, 0.6f));
        }
    });
}

void UIPowellsLockerWorkshop::OnTabInstalled(Event *ev) {
    m_currentType = WorkshopContentType::ALL;
    auto installed = WorkshopManager::Instance().GetInstalledItems();
    SetItems(installed);
    if (m_lblStatus) {
        char buf[128];
        Com_sprintf(buf, sizeof(buf), "%d installed packages found on disk.", (int)installed.size());
        m_lblStatus->setTitle(buf);
        m_lblStatus->setForegroundColor(UColor(0.6f, 0.9f, 0.6f));
    }
}

void UIPowellsLockerWorkshop::OnToggleGameFilter(Event *ev) {
    static const char *filters[] = { "ALL", "MOHAA", "MOHSH", "MOHBT" };
    static const char *labels[]  = { "Game: ALL", "Game: Allied Assault", "Game: Spearhead", "Game: Breakthrough" };

    m_gameFilterIndex = (m_gameFilterIndex + 1) % 4;
    m_currentGameType = filters[m_gameFilterIndex];

    if (m_btnGameFilter) {
        m_btnGameFilter->setTitle(labels[m_gameFilterIndex]);
    }

    RefreshList();
}

void UIPowellsLockerWorkshop::OnSearch(Event *ev) {
    RefreshList();
}

void UIPowellsLockerWorkshop::OnInstall(Event *ev) {
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_items.size()) {
        const WorkshopItem& it = m_items[m_selectedIndex];
        uii.Snd_PlaySound("sound/menu/apply.wav");
        WorkshopManager::Instance().StartDownload(it, false);
    }
}

void UIPowellsLockerWorkshop::OnPlay(Event *ev) {
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_items.size()) {
        const WorkshopItem& it = m_items[m_selectedIndex];
        uii.Snd_PlaySound("sound/menu/apply.wav");

        if (it.isInstalled) {
            WorkshopManager::Instance().PlayMap(it.mapName, it.filename);
            OnClose(nullptr);
        } else {
            WorkshopManager::Instance().StartDownload(it, true);
        }
    }
}

void UIPowellsLockerWorkshop::OnUninstall(Event *ev) {
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_items.size()) {
        const WorkshopItem& it = m_items[m_selectedIndex];
        uii.Snd_PlaySound("sound/menu/apply.wav");
        WorkshopManager::Instance().UninstallItem(it.filename);
        RefreshList();
    }
}

void UIPowellsLockerWorkshop::OnCancelDownload(Event *ev) {
    WorkshopManager::Instance().CancelDownload();
}

void UIPowellsLockerWorkshop::OnClose(Event *ev) {
    PostEvent(EV_Remove, 0);
}

// ===========================================================================
// Launcher Function
// ===========================================================================

void UI_LaunchWorkshop_f(void) {
    UIPowellsLockerWorkshop *dialog = new UIPowellsLockerWorkshop();

    int width = 580;
    int height = 370;
    dialog->Create(
        NULL,
        UIRect2D((uid.vidWidth - width) / 2, (uid.vidHeight - height) / 2, width, height),
        "Powell's Locker Workshop",
        UColor(0.15f, 0.195f, 0.278f),
        UHudColor
    );

    uWinMan.ActivateControl(dialog);
    dialog->Connect(dialog, W_Deactivated, W_Deactivated);
}
