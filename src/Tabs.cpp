
#include "BaseUtil.h"
#include "Tabs.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "resource.h"
#include "SumatraPDF.h"
#include "TableOfContents.h"
#include "WindowInfo.h"
#include "WinUtil.h"



#ifdef OWN_TAB_DRAWING
class TabPainter
{
    Vec<WCHAR *> text;
    PathData *data;
    int width, height, tabbarHeight;
    HWND hwnd;
    struct {
        COLORREF background, highlight, select, outline, text, x_highlight, x_click, x_line;
    } color;

public:
    int current, highlighted, xClicked, xHighlighted;

    TabPainter(HWND wnd, int tabWidth, int tabHeight, int tabBarHeight) : hwnd(wnd), data(NULL) {
        current = highlighted = xClicked = xHighlighted = -1;
        width = height = tabbarHeight = 0;
        Reshape(tabWidth, tabHeight, tabBarHeight);
        EvaluateColors();
    }

    ~TabPainter() {
        delete data;
        DeleteAll();
    }

    // Calculates tab's elements, based on its width and height.
    // Generates a GraphicsPath, which is used for painting the tab, etc.
    bool Reshape(int dx, int dy, int dh=0) {
        if (width == dx && height == dy && (!dh || tabbarHeight == dh))
            return false;
        width = dx; height = dy;
        if (dh) tabbarHeight = dh;

        GraphicsPath shape;
        // define tab's body
        int c = int((float)height * 0.6f + 0.5f); // size of bounding square for the arc
        shape.AddArc(0, 0, c, c, 180.0f, 90.0f);
        shape.AddArc(width - c, 0, c, c, 270.0f, 90.0f);
        shape.AddLine(width, height, 0, height);
        shape.CloseFigure();
        shape.SetMarker();
        // define "x"'s circle
        c = height > 17 ? 14 : int((float)height * 0.78f + 0.5f); // size of bounding square for the circle
        Point p(width - c - 3, (height - c) / 2); // circle's position
        shape.AddEllipse(p.X, p.Y, c, c);
        shape.SetMarker();
        // define "x"
        int o = int((float)c * 0.286f + 0.5f); // "x"'s offset
        shape.AddLine(p.X+o, p.Y+o, p.X+c-o, p.Y+c-o);
        shape.StartFigure();
        shape.AddLine(p.X+c-o, p.Y+o, p.X+o, p.Y+c-o);
        shape.SetMarker();

        delete data;
        data = new PathData();
        shape.GetPathData(data);
        return true;
    }

    // Finds the index of the tab, which contains the given point.
    int IndexFromPoint(int x, int y, bool *inXbutton) {
        Point point(x, y);
        Graphics graphics(hwnd);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);
        iterator.NextMarker(&shape);

        graphics.TranslateTransform(1.0f, REAL(tabbarHeight - height - 1));
        for (size_t i = 0; i < Count(); i++) {
            Point pt(point);
            graphics.TransformPoints( CoordinateSpaceWorld, CoordinateSpaceDevice, &pt, 1);
            if (shape.IsVisible(pt, &graphics)) {
                iterator.NextMarker(&shape);
                *inXbutton = shape.IsVisible(pt, &graphics) ? true : false;
                return i;
            }
            graphics.TranslateTransform(REAL(width + 1), 0.0f);
        }
        *inXbutton = false;
        return -1;
    }

    // Invalidates the tab's region in the client area.
    void Invalidate(int index) {
        if (index < 0) return;

        Graphics graphics(hwnd);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);
        iterator.NextMarker(&shape);
        Region region(&shape);

        graphics.TranslateTransform(REAL((width + 1) * index) + 1.0f, REAL(tabbarHeight - height - 1));
        HRGN hRgn = region.GetHRGN(&graphics);
        InvalidateRgn(hwnd, hRgn, FALSE);
        DeleteObject(hRgn);
    }

    // Paints the tabs that intersect the window's update rectangle.
    void Paint(HDC hdc, RECT &rc) {
        ClientRect rClient(hwnd);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, rClient.dx, rClient.dy);
        HDC memDC = CreateCompatibleDC(hdc);
        DeleteObject(SelectObject(memDC, hMemBmp));
        IntersectClipRect(memDC, rc.left, rc.top, rc.right, rc.bottom);

        // paint the background
        FillRect(memDC, &rc, GetSysColorBrush(TAB_COLOR_BG));

        Graphics graphics(memDC);
        graphics.SetCompositingMode(CompositingModeSourceOver);
        graphics.SetCompositingQuality(CompositingQualityHighQuality);
        graphics.SetSmoothingMode(SmoothingModeHighQuality);
        graphics.SetPageUnit(UnitPixel);
        GraphicsPath shapes(data->Points, data->Types, data->Count);
        GraphicsPath shape;
        GraphicsPathIterator iterator(&shapes);

        Color c;
        SolidBrush br(c);
        Pen pen(c, 1.0f);

        Font f(memDC, gDefaultGuiFont);
        RectF layout(3.0f, 0.0f, REAL(width - 20), (REAL)height);
        StringFormat sf(StringFormat::GenericDefault());
        sf.SetFormatFlags(StringFormatFlagsNoWrap);
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);

        graphics.TranslateTransform(1.0f, REAL(tabbarHeight - height - 1));
        for (size_t i = 0; i < Count(); i++) {
            if (graphics.IsVisible(0, 0, width + 1, height + 1)) {
                // paint tab's body
                iterator.NextMarker(&shape);
                if (current == (int)i)
                    graphics.FillPath(LoadBrush(br, c, color.select), &shape);
                else if (highlighted == (int)i)
                    graphics.FillPath(LoadBrush(br, c, color.highlight), &shape);
                graphics.DrawPath(LoadPen(pen, c, color.outline, 1.0f), &shape);

                // draw tab's text
                graphics.DrawString(text.At(i), -1, &f, layout, &sf, LoadBrush(br, c, color.text));

                // paint "x"'s circle
                iterator.NextMarker(&shape);
                if (xClicked == (int)i)
                    graphics.FillPath(LoadBrush(br, c, color.x_click), &shape);
                else if (xHighlighted == (int)i)
                    graphics.FillPath(LoadBrush(br, c, color.x_highlight), &shape);

                // paint "x"
                iterator.NextMarker(&shape);
                if (xClicked == (int)i || xHighlighted == (int)i)
                    LoadPen(pen, c, color.x_line, 2.0f);
                else
                    LoadPen(pen, c, color.outline, 2.0f);
                graphics.DrawPath(&pen, &shape);

                iterator.Rewind();
            }
            graphics.TranslateTransform(REAL(width + 1), 0.0f);
        }
        // draw the line at the bottom of the tab bar
        graphics.DrawLine(LoadPen(pen, c, color.outline, 1.0f), 0, height, rc.right, height);

        BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, memDC, rc.left, rc.top, SRCCOPY);

        DeleteDC(memDC);
        DeleteObject(hMemBmp);
    }

    // Evaluates the colors for the tab's elements.
    void EvaluateColors() {
        COLORREF bg  = GetSysColor(TAB_COLOR_BG);
        COLORREF txt = GetSysColor(TAB_COLOR_TEXT);
        if (bg == color.background && txt == color.text)
            return;

        color.background  = bg;
        color.text        = txt;

        int sign = 230.0f < GetLightness(color.background) ? -1 : 1;

        color.select      = AdjustLightness2(color.background, sign * 25.0f);
        color.highlight   = AdjustLightness2(color.background, sign * 15.0f);

        sign = GetLightness(color.text) < GetLightness(color.background) ? -1 : 1;

        color.outline     = AdjustLightness2(color.background, sign * 60.0f);
        color.x_line      = COL_CLOSE_X_HOVER;
        color.x_highlight = COL_CLOSE_HOVER_BG;
        color.x_click     = AdjustLightness2(color.x_highlight, -10.0f);
    }

    size_t Count() {
        return text.Count();
    }

    void Insert(size_t index, const WCHAR *t) {
        text.InsertAt(index, str::Dup(t));
    }

    bool Set(size_t index, const WCHAR *t) {
        if (index < Count()) {
            free(text.At(index));
            text.At(index) = str::Dup(t);
            return true;
        }
        return false;
    }

    bool Delete(size_t index) {
        if (index < Count()) {
            free(text.At(index));
            text.RemoveAt(index);
            return true;
        }
        return false;
    }

    void DeleteAll() {
        for (size_t i = 0; i < Count(); i++) {
            free(text.At(i));
        }
        text.Reset();
    }

private:
    float GetLightness(COLORREF c) {
        BYTE R = GetRValueSafe(c), G = GetGValueSafe(c), B = GetBValueSafe(c);
        BYTE M = max(max(R, G), B), m = min(min(R, G), B);
        return (M + m) / 2.0f;
    }

    // Adjusts lightness by 1/255 units.
    COLORREF AdjustLightness2(COLORREF c, float units) {
        float lightness = GetLightness(c);
        units = limitValue(units, -lightness, 255.0f - lightness);
        if (0.0f == lightness)
            return RGB(BYTE(units + 0.5f), BYTE(units + 0.5f), BYTE(units + 0.5f));
        return AdjustLightness(c, 1.0f + units / lightness);
    }

    Brush *LoadBrush(SolidBrush &b, Color &c, COLORREF col) {
        c.SetFromCOLORREF(col);
        b.SetColor(c);
        return &b;
    }

    Pen *LoadPen(Pen &p, Color &c, COLORREF col, REAL width) {
        c.SetFromCOLORREF(col);
        p.SetColor(c);
        p.SetWidth(width);
        return &p;
    }
};


struct NotifyThreadData {
    HWND  wnd;
    UINT  code;
    int   index1;
    int   index2;

    NotifyThreadData(HWND wnd, UINT code, int index1=-1, int index2=-1) : 
                        wnd(wnd), code(code), index1(index1), index2(index2) {}
    NotifyThreadData() {}
};

// Notifies the parent window about specific events in the tab bar.
DWORD WINAPI NotifyThread(LPVOID data)
{
    NotifyThreadData *ntd = (NotifyThreadData *)data;
    LRESULT result = SendMessage(GetParent(ntd->wnd), WM_NOTIFY, IDC_TABBAR, (LPARAM)ntd);
    if (result == FALSE && (ntd->code == TCN_SELCHANGING || ntd->code == T_CLOSING)) {
        // Return the response as a WM_USER message to the tabbar's WndProc.
        PostMessage(ntd->wnd, WM_USER, ntd->code, 0);
    }
    delete ntd;
    return 0;
}


WNDPROC DefWndProcTabBar;
LRESULT CALLBACK WndProcTabBar(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    size_t index;
    LPTCITEM tcs;
    static bool isMouseInClientArea = false;
    static LPARAM mouseCoordinates;
    static bool isDragging = false;
    static int nextTab = -1;
    static TabPainter tab(hwnd, TAB_WIDTH, TAB_HEIGHT, TABBAR_HEIGHT);

    switch (msg) {
    case TCM_INSERTITEM:
        index = wParam;
        tcs = (LPTCITEM)lParam;
        tab.Insert(index, tcs->pszText);
        if ((int)index <= tab.current)
            tab.current++;
        tab.xClicked = -1;
        if (isMouseInClientArea)
            PostMessage(hwnd, WM_MOUSEMOVE, 0, mouseCoordinates);
        InvalidateRgn(hwnd, NULL, FALSE);
        break;

    case TCM_SETITEM:
        index = wParam;
        tcs = (LPTCITEM)lParam;
        if (TCIF_TEXT & tcs->mask) {
            if (tab.Set(index, tcs->pszText))
                tab.Invalidate(index);
        }
        break;

    case TCM_DELETEITEM:
        index = wParam;
        if (tab.Delete(index)) {
            if ((int)index < tab.current)
                tab.current--;
            else if ((int)index == tab.current)
                tab.current = -1;
            tab.xClicked = -1;
            if (isMouseInClientArea)
                PostMessage(hwnd, WM_MOUSEMOVE, 0, mouseCoordinates);
            if (tab.Count())
                InvalidateRgn(hwnd, NULL, FALSE);
        }
        break;

    case TCM_DELETEALLITEMS:
        tab.DeleteAll();
        tab.current = tab.highlighted = tab.xClicked = tab.xHighlighted = -1;
        break;

    case TCM_SETITEMSIZE:
        if (tab.Reshape(LOWORD(lParam) - 1, HIWORD(lParam) - 1)) {
            tab.xClicked = -1;
            if (isMouseInClientArea)
                PostMessage(hwnd, WM_MOUSEMOVE, 0, mouseCoordinates);
            if (tab.Count())
                InvalidateRgn(hwnd, NULL, FALSE);
        }
        break;

    case TCM_GETCURSEL:
        return tab.current;

    case TCM_SETCURSEL:
        {
            index = wParam;
            if (index >= tab.Count()) return -1;
            int previous = tab.current;
            if ((int)index != tab.current) {
                tab.Invalidate(tab.current);
                tab.Invalidate(index);
                tab.current = index;
            }
            return previous;
        }

    case WM_NCHITTEST:
        return HTCLIENT;

    case WM_MOUSELEAVE:
        PostMessage(hwnd, WM_MOUSEMOVE, 0xFF, 0);
        return 0;

    case WM_MOUSEMOVE:
        {
            mouseCoordinates = lParam;

            if (!isMouseInClientArea) {
                isMouseInClientArea = true;
                // Track the mouse for leaving the client area.
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
            }
            if (wParam == 0xFF)     // The mouse left the client area.
                isMouseInClientArea = false;

            bool inX = false;
            int hl = wParam == 0xFF ? -1 : tab.IndexFromPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &inX);
            if (isDragging && hl == -1)
                // preserve the highlighted tab if it's dragged outside the tabs' area
                hl = tab.highlighted;
            if (tab.highlighted != hl) {
                if (isDragging) {
                    // send notification if the highlighted tab is dragged over another
                    NotifyThreadData *ntd = new NotifyThreadData(hwnd, T_DRAG, tab.highlighted, hl);
                    CloseHandle(CreateThread(NULL, 0, NotifyThread, ntd, 0, NULL));
                }

                tab.Invalidate(hl);
                tab.Invalidate(tab.highlighted);
                tab.highlighted = hl;
            }
            int xHl = inX && !isDragging ? hl : -1;
            if (tab.xHighlighted != xHl) {
                tab.Invalidate(xHl);
                tab.Invalidate(tab.xHighlighted);
                tab.xHighlighted = xHl;
            }
            if (!inX)
                tab.xClicked = -1;
        }
        return 0;

    case WM_LBUTTONDOWN:
        bool inX;
        nextTab = tab.IndexFromPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &inX);
        if (inX) {
            // send request to close the tab
            NotifyThreadData *ntd = new NotifyThreadData(hwnd, T_CLOSING, nextTab);
            CloseHandle(CreateThread(NULL, 0, NotifyThread, ntd, 0, NULL));
        }
        else if (nextTab != -1) {
            if (nextTab != tab.current) {
                // send request to select tab
                NotifyThreadData *ntd = new NotifyThreadData(hwnd, TCN_SELCHANGING);
                CloseHandle(CreateThread(NULL, 0, NotifyThread, ntd, 0, NULL));
            }
            isDragging = true;
            SetCapture(hwnd);
        }
        return 0;

    case WM_USER:
        if (T_CLOSING == wParam) {
            // if we have permission to close the tab
            tab.Invalidate(nextTab);
            tab.xClicked = nextTab;
        }
        else if (TCN_SELCHANGING == wParam) {
            // if we have permission to select the tab
            tab.Invalidate(tab.current);
            tab.Invalidate(nextTab);
            tab.current = nextTab;
            // send notification that the tab is selected
            NotifyThreadData *ntd = new NotifyThreadData(hwnd, TCN_SELCHANGE);
            CloseHandle(CreateThread(NULL, 0, NotifyThread, ntd, 0, NULL));
        }
        return 0;

    case WM_LBUTTONUP:
        if (tab.xClicked != -1) {
            // send notification that the tab is closed
            NotifyThreadData *ntd = new NotifyThreadData(hwnd, T_CLOSE, tab.xClicked);
            CloseHandle(CreateThread(NULL, 0, NotifyThread, ntd, 0, NULL));
            tab.Invalidate(tab.xClicked);
            tab.xClicked = -1;
        }
        if (isDragging) {
            isDragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
        RECT rc;
        GetUpdateRect(hwnd, &rc, FALSE);

        hdc = wParam ? (HDC)wParam : BeginPaint(hwnd, &ps);
        ValidateRect(hwnd, NULL);

        tab.EvaluateColors();
        tab.Paint(hdc, rc);

        if (!wParam) EndPaint(hwnd, &ps);
        return 0;
    }
    return CallWindowProc(DefWndProcTabBar, hwnd, msg, wParam, lParam);
}
#endif //OWN_TAB_DRAWING


void CreateTabbar(WindowInfo *win)
{
    win->hwndTabBar = CreateWindow(WC_TABCONTROL, L"", 
        WS_CHILD | WS_CLIPSIBLINGS /*| WS_VISIBLE*/ | 
        TCS_FOCUSNEVER | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT, 
        0, 0, 0, 0, 
        win->hwndFrame, (HMENU)IDC_TABBAR, ghinst, NULL);

#ifdef OWN_TAB_DRAWING
    DefWndProcTabBar = (WNDPROC)SetWindowLongPtr(win->hwndTabBar, GWLP_WNDPROC, (LONG_PTR)WndProcTabBar);
#endif //OWN_TAB_DRAWING

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
    win->dm = NULL;         // prevent this data deletion
    if (!(*tdata)->title)
        (*tdata)->title = win::GetText(win->hwndFrame);
    (*tdata)->userAnnots = win->userAnnots;
    win->userAnnots = NULL;    // prevent this data deletion
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

    for (int i = 0; i < count; i++) {
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
            for (int i = 0; i < count; i++) {
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
            ManageFullScreen(win, true);
            UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(win));
            LoadModelIntoTab(win, tdata);
            ManageFullScreen(win, false);
            TabCtrl_SetCurSel(win->hwndTabBar, FindTabIndex(win->hwndTabBar, tdata));
        }
    }
}


// On tab selection, we save the data for the tab which is loosing selection and
// load the data of the selected tab into the WindowInfo.
LRESULT TabsOnNotify(WindowInfo *win, LPARAM lparam)
{
#ifdef OWN_TAB_DRAWING
    NotifyThreadData *data = (NotifyThreadData *)lparam;
#else
    LPNMHDR data = (LPNMHDR)lparam;
#endif //OWN_TAB_DRAWING

    switch(data->code) {
    case TCN_SELCHANGING:
        // TODO: Should we allow the switch of the tab if we are in process of printing?

        ManageFullScreen(win, true);
        SaveCurrentTabData(win);
        return FALSE;

    case TCN_SELCHANGE:
        {
            int current = TabCtrl_GetCurSel(win->hwndTabBar);
            LoadModelIntoTab(win, GetTabData(win->hwndTabBar, current));
            ManageFullScreen(win, false);
        }
        break;

#ifdef OWN_TAB_DRAWING
    case T_CLOSING:
        // allow the closure
        return FALSE;

    case T_CLOSE:
        {
            int current = TabCtrl_GetCurSel(win->hwndTabBar);
            if (data->index1 == current) {
                CloseWindow(win, false);
            }
            else {
                TabData *tdata = GetTabData(win->hwndTabBar, data->index1);
                win->tabSelectionHistory->Remove(tdata);
                DeleteTabData(tdata, true);
                TabCtrl_DeleteItem(win->hwndTabBar, data->index1);
                UpdateTabWidth(win);
            }
        }
        break;

    case T_DRAG:
        SwapTabs(win, data->index1, data->index2);
        break;
#endif //OWN_TAB_DRAWING
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


// Selects the next tab.
void TabsOnCtrlTab(WindowInfo *win)
{
    int count = TabCtrl_GetItemCount(win->hwndTabBar);
    if (count < 2) return;

#ifdef OWN_TAB_DRAWING
    NotifyThreadData ntd;
#else
    NMHDR ntd;
#endif //OWN_TAB_DRAWING

    ntd.code = TCN_SELCHANGING;
    if (FALSE != TabsOnNotify(win, (LPARAM)&ntd)) return;

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    //if (-1 == current) return;

    TabCtrl_SetCurSel(win->hwndTabBar, ++current == count ? 0 : current);
    ntd.code = TCN_SELCHANGE;
    TabsOnNotify(win, (LPARAM)&ntd);
}


void ManageFullScreen(WindowInfo *win, bool exitFullScreen)
{
    static bool prevFullScreen, prevPresentation;

    if (exitFullScreen) {
        prevFullScreen = win->isFullScreen;
        prevPresentation = win->presentation != PM_DISABLED;
        if (prevFullScreen || prevPresentation)
            ExitFullScreen(*win);
    }
    else {     // enter fullscreen
        if (prevFullScreen || prevPresentation)
            EnterFullScreen(*win, prevPresentation);
    }
}


void SwapTabs(WindowInfo *win, int tab1, int tab2)
{
    if (tab1 == tab2 || tab1 < 0 || tab2 < 0)
        return;

    WCHAR buf1[MAX_PATH], buf2[MAX_PATH];
    LPARAM lp;
    TCITEM tcs;
    tcs.mask = TCIF_TEXT | TCIF_PARAM;
    tcs.cchTextMax = MAX_PATH;

    tcs.pszText = buf1;
    if (!TabCtrl_GetItem(win->hwndTabBar, tab1, &tcs))
        return;
    if (tcs.pszText != buf1)
        wcsncpy_s(buf1, tcs.pszText, _TRUNCATE);
    lp = tcs.lParam;

    tcs.pszText = buf2;
    if (!TabCtrl_GetItem(win->hwndTabBar, tab2, &tcs))
        return;
    if (tcs.pszText != buf2)
        wcsncpy_s(buf2, tcs.pszText, _TRUNCATE);

    tcs.pszText = buf2;
    TabCtrl_SetItem(win->hwndTabBar, tab1, &tcs);
    tcs.pszText = buf1;
    tcs.lParam = lp;
    TabCtrl_SetItem(win->hwndTabBar, tab2, &tcs);

    int current = TabCtrl_GetCurSel(win->hwndTabBar);
    if (tab1 == current)
        TabCtrl_SetCurSel(win->hwndTabBar, tab2);
    else if (tab2 == current)
        TabCtrl_SetCurSel(win->hwndTabBar, tab1);
}
