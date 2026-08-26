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

#include "cl_ui.h"
#include "cl_workshop.h"
#include <vector>

class WorkshopListItem : public UIListCtrlItem
{
public:
    WorkshopItem m_item;

    WorkshopListItem(const WorkshopItem& item);

    int            getListItemValue(int which) const override;
    griditemtype_t getListItemType(int which) const override;
    str            getListItemString(int which) const override;
    void           DrawListItem(int iColumn, const UIRect2D& drawRect, bool bSelected, UIFont *pFont) override;
    qboolean       IsHeaderEntry() const override;
};

class UIPowellsLockerWorkshop : public UIFloatingWindow
{
private:
    // UI Controls
    UIListCtrl       *m_listbox;
    UIButton         *m_btnFeatured;
    UIButton         *m_btnMaps;
    UIButton         *m_btnMods;
    UIButton         *m_btnCollections;
    UIButton         *m_btnInstalled;
    UIButton         *m_btnGameFilter;
    UIButton         *m_btnSearch;

    // Detail Panel Controls
    UILabel          *m_lblTitle;
    UILabel          *m_lblMeta;
    UILabel          *m_lblDescription;
    UIButton         *m_btnInstall;
    UIButton         *m_btnPlay;
    UIButton         *m_btnUninstall;

    // Bottom Status Controls
    UILabel          *m_lblStatus;
    UIButton         *m_btnCancel;
    UIButton         *m_btnClose;

    // State
    std::vector<WorkshopItem> m_items;
    int                       m_selectedIndex;
    WorkshopContentType       m_currentType;
    std::string               m_currentGameType;
    int                       m_gameFilterIndex;
    int                       m_lastProgressUpdate;

public:
    CLASS_PROTOTYPE(UIPowellsLockerWorkshop);

    static Event EV_Workshop_ItemSelected;
    static Event EV_Workshop_ItemDoubleClicked;
    static Event EV_Workshop_TabFeatured;
    static Event EV_Workshop_TabMaps;
    static Event EV_Workshop_TabMods;
    static Event EV_Workshop_TabCollections;
    static Event EV_Workshop_TabInstalled;
    static Event EV_Workshop_ToggleGameFilter;
    static Event EV_Workshop_Search;
    static Event EV_Workshop_Install;
    static Event EV_Workshop_Play;
    static Event EV_Workshop_Uninstall;
    static Event EV_Workshop_CancelDownload;

protected:
    void FrameInitialized(void) override;
    void UpdateUIElement(void) override;
    void Draw(void) override;

    // Event Handlers
    void OnItemSelected(Event *ev);
    void OnItemDoubleClicked(Event *ev);
    void OnTabFeatured(Event *ev);
    void OnTabMaps(Event *ev);
    void OnTabMods(Event *ev);
    void OnTabCollections(Event *ev);
    void OnTabInstalled(Event *ev);
    void OnToggleGameFilter(Event *ev);
    void OnSearch(Event *ev);
    void OnInstall(Event *ev);
    void OnPlay(Event *ev);
    void OnUninstall(Event *ev);
    void OnCancelDownload(Event *ev);
    void OnClose(Event *ev);

    // Helpers
    void RefreshList();
    void UpdateDetailPanel();
    void UpdateStatusDisplay();
    void SetItems(const std::vector<WorkshopItem>& items);

public:
    UIPowellsLockerWorkshop();
    ~UIPowellsLockerWorkshop();

    void Create(UIWidget *parent, const UIRect2D& rect, const char *title, const UColor& bgColor, const UColor& fgColor);
};

void UI_LaunchWorkshop_f(void);
