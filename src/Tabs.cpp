
#include "BaseUtil.h"
#include "Tabs.h"

#include "resource.h"
#include "SumatraPDF.h"
#include "TableOfContents.h"
#include "WindowInfo.h"
#include "WinUtil.h"


void CreateTabbar(WindowInfo *win)
{
    win->hwndTabBar = CreateWindow(WC_TABCONTROL, L"", 
        WS_CHILD | WS_CLIPSIBLINGS /*| WS_VISIBLE*/ | 
        TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT, 
        0, 0, 0, 0, 
        win->hwndFrame, (HMENU)IDC_TABBAR, ghinst, NULL);

    SetWindowFont(win->hwndTabBar, gDefaultGuiFont, FALSE);
    TabCtrl_SetItemSize(win->hwndTabBar, TAB_WIDTH, TAB_HEIGHT);

    win->tabSelectionHistory = new Vec<TabData *>();
}


// Saves some of the document's data from the WindowInfo to the TabData.
void SaveTabData(WindowInfo *win, TabData **tdata)
{
    if (*tdata == NULL) *tdata = new TabData();

    UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(win));
    // This update is already done in UpdateCurrentFileDisplayStateForWin, 
    // if there is record found in the gFileHistory.
    if (win->tocLoaded && !gFileHistory.Find(win->loadedFilePath)) {
        win->tocState.Reset();
        HTREEITEM hRoot = TreeView_GetRoot(win->hwndTocTree);
        if (hRoot)
            UpdateTocExpansionState(win, hRoot);
    }
    (*tdata)->tocState = win->tocState;
    (*tdata)->showToc = win->tocVisible;

    (*tdata)->dm = win->dm;
    win->dm = NULL;
    if (!(*tdata)->title)
        (*tdata)->title = win::GetText(win->hwndFrame);
    (*tdata)->userAnnots = win->userAnnots;
    win->userAnnots = NULL;
    (*tdata)->userAnnotsModified = win->userAnnotsModified;
}


// Must be called when the active tab is loosing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentTabData(WindowInfo *win)
{
    if (!win) return;

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    if (current != -1) {
        TCITEM tcs;
        tcs.mask = TCIF_PARAM;
        if (TabCtrl_GetItem(win->hwndTabBar, current, &tcs)) {
            if (win->IsDocLoaded()) {
                // we use the lParam member of the TCITEM structure of the tab, to save the TabData pointer in
                SaveTabData(win, (TabData **)&tcs.lParam);
                TabCtrl_SetItem(win->hwndTabBar, current, &tcs);

                // update the selectoin history
                win->tabSelectionHistory->Remove((TabData *)tcs.lParam);
                win->tabSelectionHistory->Push((TabData *)tcs.lParam);
            }
            else {
                DeleteTabData((TabData *)tcs.lParam, false);
                TabCtrl_DeleteItem(win->hwndTabBar, current);
                UpdateTabWidth(win);
            }
        }
    }
}


// Gets the TabData pointer from the lParam member of the TCITEM structure of the tab.
TabData *GetTabData(HWND tabbarHwnd, int tabIndex)
{
    TCITEM tcs;
    tcs.mask = TCIF_PARAM;
    if (TabCtrl_GetItem(tabbarHwnd, tabIndex, &tcs))
        return (TabData *)tcs.lParam;
    return NULL;
}


int FindTabIndex(HWND tabbarHwnd, TabData *tdata)
{
    if (!tabbarHwnd || !tdata) return -1;
    int count = TabCtrl_GetItemCount(tabbarHwnd);

    for(int i = 0; i < count; i++) {
        if (tdata == GetTabData(tabbarHwnd, i))
            return i;
    }
    return -1;
}


void DeleteTabData(TabData *tdata, bool deleteModel)
{
    if (tdata) {
        if (deleteModel) {
            delete tdata->dm;
            delete tdata->userAnnots;
        }
        free(tdata->title);
        delete tdata;
    }
}


// On load of a new document we insert a new tab item in the tab bar.
// Its text is the name of the opened file.
void TabsOnLoadedDoc(WindowInfo *win)
{
    if (!win) return;

    //ScopedMem<WCHAR> filename(str::Dup(path::GetBaseName(win->dm->FilePath())));
    ScopedMem<WCHAR> filename(str::Dup(path::GetBaseName(win->loadedFilePath)));

    TCITEM tcs;
    tcs.mask = TCIF_TEXT | TCIF_PARAM;
    tcs.pszText = filename;
    tcs.lParam = NULL;
    int count = TabCtrl_GetItemCount(win->hwndTabBar);

    if (-1 != TabCtrl_InsertItem(win->hwndTabBar, count, &tcs)) {
        TabCtrl_SetCurSel(win->hwndTabBar, count);
        UpdateTabWidth(win);

        if (!count) ShowOrHideTabbar(win, SW_SHOW);
    }
}


// Called when we're closing a document or when we're quitting.
void TabsOnCloseWindow(WindowInfo *win, bool cleanUp)
{
    int count = TabCtrl_GetItemCount(win->hwndTabBar);
    if (count) {
        TabData *tdata;

        if (cleanUp) {
            for(int i = 0; i < count; i++) {
                tdata = GetTabData(win->hwndTabBar, i);
                if (tdata)
                    DeleteTabData(tdata, win->dm != tdata->dm);
            }
            TabCtrl_DeleteAllItems(win->hwndTabBar);
            delete win->tabSelectionHistory;
            return;
        }

        int current = TabCtrl_GetCurSel(win->hwndTabBar);
        tdata = GetTabData(win->hwndTabBar, current);
        win->tabSelectionHistory->Remove(tdata);
        DeleteTabData(tdata, false);
        TabCtrl_DeleteItem(win->hwndTabBar, current);
        UpdateTabWidth(win);
        if (count > 1) {
            tdata = win->tabSelectionHistory->Pop();
            LoadModelIntoTab(win, tdata);
            TabCtrl_SetCurSel(win->hwndTabBar, FindTabIndex(win->hwndTabBar, tdata));
        }
    }
}


// On tab selection, we save the data for the tab which is loosing selection and
// load the data of the selected tab into the WindowInfo.
LRESULT TabsOnNotify(WindowInfo *win, UINT notification)
{
    switch(notification) {
    case TCN_SELCHANGING:
        // TODO: Should we allow the switch of the tab if we are in process of printing?

        SaveCurrentTabData(win);
        return FALSE;

    case TCN_SELCHANGE:
        {
            int current = TabCtrl_GetCurSel(win->hwndTabBar);
            LoadModelIntoTab(win, GetTabData(win->hwndTabBar, current));
        }
        break;
    }
    return TRUE;
}


void ShowOrHideTabbar(WindowInfo *win, int command)
{
    ShowWindow(win->hwndTabBar, command);
    ClientRect rect(win->hwndFrame);
    SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
}


void UpdateTabWidth(WindowInfo *win)
{
    ClientRect rect(win->hwndFrame);
    int count = TabCtrl_GetItemCount(win->hwndTabBar);
    if (count) {
        int tabWidth = (rect.dx - 3) / count;
        TabCtrl_SetItemSize(win->hwndTabBar, TAB_WIDTH < tabWidth ? TAB_WIDTH : tabWidth, TAB_HEIGHT);
    }
}

