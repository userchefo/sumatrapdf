

#define TABBAR_HEIGHT    24
#define TAB_WIDTH        200
#define TAB_HEIGHT       TABBAR_HEIGHT - 3

class DisplayModel;
struct PageAnnotation;
class WindowInfo;

// This is the data, for every opened document, which is preserved between
// tab selections. It's loaded back in the WindowInfo for the currently active document.
struct TabData
{
    DisplayModel *        dm;
    bool                  showToc;
    Vec<int>              tocState;
    WCHAR *               title;
    Vec<PageAnnotation> * userAnnots;
    bool                  userAnnotsModified;

    TabData(): dm(NULL), showToc(false), tocState(0,NULL), title(NULL), userAnnots(NULL), userAnnotsModified(false) {}
};

void SaveTabData(WindowInfo *win, TabData **tdata);
void SaveCurrentTabData(WindowInfo *win);
TabData *GetTabData(HWND tabbarHwnd, int tabIndex);
void DeleteTabData(TabData *tdata, bool deleteModel);
void LoadModelIntoTab(WindowInfo *win, TabData *tdata);

void CreateTabbar(WindowInfo *win);
void TabsOnLoadedDoc(WindowInfo *win);
void TabsOnCloseWindow(WindowInfo *win, bool cleanUp);
LRESULT TabsOnNotify(WindowInfo *win, UINT notification);
void TabsOnCtrlTab(WindowInfo *win);

void ShowOrHideTabbar(WindowInfo *win, int command);
void UpdateTabWidth(WindowInfo *win);
void ManageFullScreen(WindowInfo *win, bool exitFullScreen);

