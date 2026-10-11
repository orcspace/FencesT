#pragma once
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <filesystem>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include "model.hpp"

namespace ft {
constexpr UINT WM_TRAY = WM_APP+1, WM_DESKTOP = WM_APP+2, WM_REOPEN = WM_APP+3;
constexpr UINT ID_TRAY=1, ID_SHOW=100, ID_SETTINGS=101, ID_NEW=102, ID_QUIT=103, ID_EXPORT=104, ID_IMPORT=105;
constexpr int inset=10, header=58;
class App; class Panel; class Settings;
std::wstring wide(const std::string& s);
std::string utf8(const std::wstring& s);
std::wstring windowText(HWND hwnd);
std::wstring labelFor(const std::wstring& path);
std::vector<std::wstring> pickFiles(HWND owner, bool folder=false, bool image=false);
std::wstring pickLayout(HWND owner, bool save);
void showError(HWND owner, const std::wstring& text);
void openItem(HWND owner, const Item& item);
std::filesystem::path dataPath();
bool loadState(const std::filesystem::path& file, State& state);
bool saveState(const std::filesystem::path& file, const State& state, bool backup=true);
bool startupEnabled(); bool setStartup(bool enabled);
void clampToMonitor(Group& group, float scale);
std::unique_ptr<Gdiplus::Bitmap> loadImage(const std::wstring& file);
void roundPath(Gdiplus::GraphicsPath& path, Gdiplus::RectF rect, float radius);
void drawText(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::RectF rect, float size,
              Gdiplus::Color color, bool bold=false, bool center=false);
void fillRound(Gdiplus::Graphics& g, Gdiplus::RectF r, float radius, Gdiplus::Color color);
Gdiplus::Color rgb(uint32_t color, BYTE alpha=255);
void blurBitmap(Gdiplus::Bitmap& bitmap, int radius);

struct Entry {
    Item item;
    std::unique_ptr<Gdiplus::Bitmap> icon;
};
struct Wallpaper {
    RECT rect{};
    std::unique_ptr<Gdiplus::Bitmap> image;
};

class Panel final : public IDropTarget {
public:
    App& app; Group group; HWND hwnd=nullptr, tooltip=nullptr; float scale=1;
    std::wstring tipText;
    std::vector<Entry> entries;
    std::unique_ptr<Gdiplus::Bitmap> background, backdrop;
    int hovered=-1, selected=-1, scroll=0; bool dropHover=false, moving=false, tracking=false;
    HANDLE folderWatch=INVALID_HANDLE_VALUE;
    explicit Panel(App& app, Group group); ~Panel();
    bool create(); void render(); void apply(bool reload=false); void refreshItems(); void updateWatch();
    void invalidateBackdrop(); void ensureBackdrop(int w, int h);
    void addPaths(const std::vector<std::wstring>& paths); void menu(POINT point); void activate(int index);
    int hitItem(int x, int y) const; Gdiplus::RectF itemRect(int index) const;
    int columns() const; int cellHeight() const; int cellWidth() const; int visibleRows() const;
    void clampScroll(); void toggleCollapse(); void resizeFromModel();
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override; ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject*,DWORD,POINTL,DWORD*) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD,POINTL,DWORD*) override;
    HRESULT STDMETHODCALLTYPE DragLeave() override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject*,DWORD,POINTL,DWORD*) override;
private:
    LONG refs=1; bool acceptsDrop=false; bool registered=false;
    Gdiplus::Bitmap* iconFor(const Item& item, std::unique_ptr<Gdiplus::Bitmap>& storage);
};

class Settings {
public:
    explicit Settings(App& app):app(app){} ~Settings();
    App& app; HWND hwnd=nullptr; HFONT font=nullptr, titleFont=nullptr; HBRUSH brush=nullptr, fieldBrush=nullptr;
    HBRUSH sideBrush=nullptr;
    float scale=1; uint32_t target=0; bool loading=false; int tab=0;
    std::map<int,HWND> controls;
    std::vector<size_t> filtered;
    void open(uint32_t id=0); void build(); void layout(); void paint(); void refresh(bool list=true);
    void update(int id); void command(int id); void fillItems(); void selectGroup(uint32_t id);
    Panel* panel(); void refreshGroups();
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
};

class App {
public:
    HINSTANCE instance=nullptr; HWND controller=nullptr; HICON icon=nullptr; ULONG_PTR gdiplus=0;
    NOTIFYICONDATAW tray{}; HWINEVENTHOOK foregroundHook=nullptr; UINT taskbarCreated=0;
    bool visible=true, snap=true, dirty=false, closing=false, hotkeyOk=true; uint32_t nextId=1;
    std::vector<std::unique_ptr<Panel>> panels; std::unique_ptr<Settings> settings;
    std::vector<Wallpaper> wallpapers; COLORREF wallpaperColor=RGB(26,32,46);
    bool initialize(HINSTANCE); void shutdown(); void load(); bool save(); void changed();
    State snapshot() const; void restore(const State& state); void createDefaults();
    Panel* find(uint32_t id); void addGroup(const Group* source=nullptr); void deleteGroup(uint32_t id);
    void toggleVisible(); void showPanels(); void openSettings(uint32_t id=0); void trayMenu();
    void exportLayout(); void importLayout(); void reloadWallpaper(); void notify(const wchar_t*,const wchar_t*);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
    static void CALLBACK foreground(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
};
}
