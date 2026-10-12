#include "app.hpp"
#include <shlwapi.h>
#include <commoncontrols.h>
#include <cmath>

using namespace Gdiplus;
namespace ft {
Panel::Panel(App& a,Group g):app(a),group(std::move(g)) {}
Panel::~Panel() {
    if(tooltip) DestroyWindow(tooltip);
    if(folderWatch!=INVALID_HANDLE_VALUE) FindCloseChangeNotification(folderWatch);
    if(hwnd) {if(registered) RevokeDragDrop(hwnd); DestroyWindow(hwnd);}
}
bool Panel::create() {
    validate(group); scale=GetDpiForSystem()/96.f; clampToMonitor(group,scale);
    hwnd=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW,L"FencesT.Panel",wide(group.name).c_str(),WS_POPUP|WS_THICKFRAME,
        group.x,group.y,static_cast<int>((group.width+inset*2)*scale),static_cast<int>(((group.collapsed ? header : group.height)+inset*2)*scale),
        nullptr,nullptr,app.instance,this);
    if(!hwnd) return false;
    scale=GetDpiForWindow(hwnd)/96.f;
    registered=SUCCEEDED(RegisterDragDrop(hwnd,this));
    tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
        CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,hwnd,nullptr,app.instance,nullptr);
    if(tooltip) {
        TOOLINFOW tool{}; tool.cbSize=sizeof(tool); tool.uFlags=TTF_IDISHWND|TTF_SUBCLASS; tool.hwnd=hwnd;
        tool.uId=reinterpret_cast<UINT_PTR>(hwnd); tool.lpszText=LPSTR_TEXTCALLBACKW;
        SendMessageW(tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool));
        SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,350); SendMessageW(tooltip,TTM_SETDELAYTIME,TTDT_INITIAL,500);
    }
    BOOL exclude=TRUE; DwmSetWindowAttribute(hwnd,DWMWA_EXCLUDED_FROM_PEEK,&exclude,sizeof(exclude));
    background=loadImage(wide(group.image)); refreshItems(); updateWatch(); resizeFromModel();
    if(app.visible) ShowWindow(hwnd,SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE); return true;
}
void Panel::resizeFromModel() {
    SetWindowPos(hwnd,nullptr,group.x,group.y,static_cast<int>((group.width+inset*2)*scale),static_cast<int>(((group.collapsed ? header : group.height)+inset*2)*scale),SWP_NOACTIVATE|SWP_NOZORDER);
    invalidateBackdrop(); render();
}
void Panel::apply(bool reload) {
    validate(group); SetWindowTextW(hwnd,wide(group.name).c_str());
    if(reload) background=loadImage(wide(group.image));
    invalidateBackdrop(); clampScroll(); render(); app.changed();
}
Gdiplus::Bitmap* Panel::iconFor(const Item& item,std::unique_ptr<Bitmap>& storage) {
    const auto path=wide(item.path); SHFILEINFOW info{}; HICON icon=nullptr;
    if(SHGetFileInfoW(path.c_str(),0,&info,sizeof(info),SHGFI_SYSICONINDEX|SHGFI_LARGEICON)) {
        IImageList* list=nullptr;
        if(SUCCEEDED(SHGetImageList(SHIL_EXTRALARGE,IID_PPV_ARGS(&list)))) {
            list->GetIcon(info.iIcon,ILD_TRANSPARENT,&icon); list->Release();
        }
    }
    if(!icon && SHGetFileInfoW(path.c_str(),0,&info,sizeof(info),SHGFI_ICON|SHGFI_LARGEICON)) icon=info.hIcon;
    if(icon) {storage.reset(Bitmap::FromHICON(icon)); DestroyIcon(icon); if(storage->GetLastStatus()!=Ok) storage.reset();}
    return storage.get();
}
void Panel::refreshItems() {
    entries.clear(); std::vector<Item> items=group.items;
    if(!group.folder.empty()) {
        items.clear(); std::error_code error;
        std::filesystem::directory_iterator it(std::filesystem::path(wide(group.folder)),std::filesystem::directory_options::skip_permission_denied,error),end;
        while(!error && it!=end && items.size()<maxItems) {
            auto path=it->path().wstring(); DWORD attr=GetFileAttributesW(path.c_str());
            if(attr!=INVALID_FILE_ATTRIBUTES && !(attr&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))) items.push_back({utf8(path),utf8(labelFor(path))});
            it.increment(error);
        }
        std::sort(items.begin(),items.end(),[](const Item& a,const Item& b){return StrCmpLogicalW(wide(a.label).c_str(),wide(b.label).c_str())<0;});
    }
    for(auto& item:items) {Entry entry; entry.item=std::move(item); iconFor(entry.item,entry.icon); entries.push_back(std::move(entry));}
    hovered=selected=-1; clampScroll(); render();
}
void Panel::updateWatch() {
    if(folderWatch!=INVALID_HANDLE_VALUE) FindCloseChangeNotification(folderWatch);
    folderWatch=INVALID_HANDLE_VALUE;
    if(!group.folder.empty()) folderWatch=FindFirstChangeNotificationW(wide(group.folder).c_str(),FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES);
}
void Panel::addPaths(const std::vector<std::wstring>& paths) {
    if(!group.folder.empty()) {showError(hwnd,L"Эта группа показывает содержимое папки. Добавляйте файлы в саму папку через Проводник."); return;}
    bool added=false;
    for(auto& path:paths) {
        auto encoded=utf8(path);
        bool exists=std::any_of(group.items.begin(),group.items.end(),[&](const Item& i){return _wcsicmp(wide(i.path).c_str(),path.c_str())==0;});
        if(!exists) added=addItem(group,{std::move(encoded),utf8(labelFor(path))})||added;
    }
    if(added) {refreshItems(); app.changed(); if(app.settings) app.settings->refresh();}
}
int Panel::cellWidth() const {return std::max(78,group.iconSize+30);}
int Panel::cellHeight() const {return group.iconSize+(group.showLabels ? 43 : 20);}
int Panel::columns() const {return std::max(1,(group.width-28)/cellWidth());}
int Panel::visibleRows() const {return std::max(1,(group.height-header-12)/cellHeight());}
void Panel::clampScroll() {
    int rows=(static_cast<int>(entries.size())+columns()-1)/columns(); scroll=std::clamp(scroll,0,std::max(0,rows-visibleRows()));
}
RectF Panel::itemRect(int index) const {
    int cols=columns(); float width=(group.width-28.f)/cols;
    return RectF(inset+14+(index%cols)*width,inset+header+4.f+(index/cols-scroll)*cellHeight(),width,static_cast<float>(cellHeight()));
}
int Panel::hitItem(int x,int y) const {
    if(group.collapsed || y<inset+header || y>inset+group.height-8) return -1;
    for(size_t i=0;i<entries.size();++i) if(itemRect(static_cast<int>(i)).Contains(static_cast<float>(x),static_cast<float>(y))) return static_cast<int>(i);
    return -1;
}
void Panel::activate(int i) {if(i>=0 && i<static_cast<int>(entries.size())) openItem(hwnd,entries[i].item);}
void Panel::toggleCollapse() {group.collapsed=!group.collapsed; resizeFromModel(); app.changed();}
void Panel::menu(POINT pt) {
    POINT client=pt; ScreenToClient(hwnd,&client); int hit=hitItem(static_cast<int>(client.x/scale),static_cast<int>(client.y/scale));
    HMENU menu=CreatePopupMenu();
    if(hit>=0) {
        AppendMenuW(menu,MF_STRING,201,L"Открыть"); AppendMenuW(menu,MF_STRING,202,L"Показать в Проводнике");
        if(group.folder.empty()) AppendMenuW(menu,MF_STRING,203,L"Убрать ярлык из группы");
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    }
    AppendMenuW(menu,MF_STRING,204,L"Оформление и настройки…");
    AppendMenuW(menu,MF_STRING,205,group.collapsed ? L"Развернуть группу" : L"Свернуть группу");
    AppendMenuW(menu,MF_STRING|(group.showLabels ? MF_CHECKED : 0),206,L"Показывать названия");
    AppendMenuW(menu,MF_STRING|(group.locked ? MF_CHECKED : 0),207,L"Заблокировать положение");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    if(group.folder.empty()) {AppendMenuW(menu,MF_STRING,208,L"Добавить файлы или приложения…"); AppendMenuW(menu,MF_STRING,209,L"Добавить ярлык папки…");}
    else {AppendMenuW(menu,MF_STRING,215,L"Открыть исходную папку"); AppendMenuW(menu,MF_STRING,210,L"Обновить содержимое");}
    AppendMenuW(menu,MF_STRING,211,L"Новая группа"); AppendMenuW(menu,MF_STRING,212,L"Дублировать группу");
    auto hide=L"Скрыть все группы\t"+hotkeyText(app.hotkeys[0]);
    AppendMenuW(menu,MF_STRING,213,hide.c_str()); AppendMenuW(menu,MF_STRING,214,L"Удалить группу…");
    SetForegroundWindow(hwnd); int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,hwnd,nullptr); DestroyMenu(menu);
    switch(choice) {
    case 201: activate(hit); break;
    case 202: if(hit>=0) {auto args=L"/select,\""+wide(entries[hit].item.path)+L"\""; ShellExecuteW(hwnd,L"open",L"explorer.exe",args.c_str(),nullptr,SW_SHOWNORMAL);} break;
    case 203: if(hit>=0 && hit<static_cast<int>(group.items.size())) {group.items.erase(group.items.begin()+hit); refreshItems(); app.changed(); if(app.settings) app.settings->refresh();} break;
    case 204: app.openSettings(group.id); break;
    case 205: toggleCollapse(); break;
    case 206: group.showLabels=!group.showLabels; apply(); if(app.settings) app.settings->refresh(); break;
    case 207: group.locked=!group.locked; apply(); if(app.settings) app.settings->refresh(); break;
    case 208: addPaths(pickFiles(hwnd)); break;
    case 209: addPaths(pickFiles(hwnd,true)); break;
    case 210: refreshItems(); break;
    case 211: app.addGroup(); break;
    case 212: app.addGroup(&group); break;
    case 213: app.toggleVisible(); break;
    case 214: PostMessageW(app.controller,WM_APP+20,group.id,0); break;
    case 215: openItem(hwnd,{group.folder,{}}); break;
    }
    PostMessageW(hwnd,WM_NULL,0,0);
}
LRESULT CALLBACK Panel::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* p=reinterpret_cast<Panel*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {p=static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); p->hwnd=hwnd; SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
    if(!p) return DefWindowProcW(hwnd,msg,wp,lp);
    int x=static_cast<int>(GET_X_LPARAM(lp)/p->scale), y=static_cast<int>(GET_Y_LPARAM(lp)/p->scale);
    switch(msg) {
    case WM_NCCALCSIZE: return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); return 0;}
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ScreenToClient(hwnd,&point);
        int cx=static_cast<int>(point.x/p->scale),cy=static_cast<int>(point.y/p->scale);
        if(!p->group.locked && !p->group.collapsed && cx>inset+p->group.width-22 && cy>inset+p->group.height-22) return HTBOTTOMRIGHT;
        return HTCLIENT;
    }
    case WM_GETMINMAXINFO: {
        auto* m=reinterpret_cast<MINMAXINFO*>(lp); m->ptMinTrackSize={static_cast<LONG>(280*p->scale),static_cast<LONG>((p->group.collapsed ? header+20 : 180)*p->scale)};
        m->ptMaxTrackSize={static_cast<LONG>(1620*p->scale),static_cast<LONG>(1420*p->scale)}; return 0;
    }
    case WM_MOUSEACTIVATE: return MA_ACTIVATE;
    case WM_LBUTTONDOWN: {
        SetFocus(hwnd);
        if(y<header+inset) {
            if(x>inset+p->group.width-48) {POINT pt; GetCursorPos(&pt); p->menu(pt);}
            else if(x>inset+p->group.width-88) p->toggleCollapse();
            else if(!p->group.locked) {ReleaseCapture(); SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,0);}
        } else {p->selected=p->hitItem(x,y); p->render();}
        return 0;
    }
    case WM_LBUTTONDBLCLK: if(y<header+inset) p->toggleCollapse(); else p->activate(p->hitItem(x,y)); return 0;
    case WM_MOUSEMOVE: {
        int hit=p->hitItem(x,y); if(hit!=p->hovered) {p->hovered=hit; p->render();}
        if(!p->tracking) {TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,hwnd,0}; TrackMouseEvent(&t); p->tracking=true;} return 0;
    }
    case WM_MOUSELEAVE: p->tracking=false; p->hovered=-1; p->render(); return 0;
    case WM_NOTIFY: {
        auto* notification=reinterpret_cast<NMHDR*>(lp);
        if(notification->code==TTN_GETDISPINFOW) {
            auto* info=reinterpret_cast<NMTTDISPINFOW*>(lp);
            p->tipText=p->hovered>=0 && p->hovered<static_cast<int>(p->entries.size()) ? wide(p->entries[p->hovered].item.label) : L"";
            info->lpszText=p->tipText.data();
        } return 0;
    }
    case WM_MOUSEWHEEL: p->scroll-=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA; p->clampScroll(); p->render(); return 0;
    case WM_CONTEXTMENU: {POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; if(point.x==-1) GetCursorPos(&point); p->menu(point); return 0;}
    case WM_ENTERSIZEMOVE: p->moving=true; return 0;
    case WM_MOVE: {
        RECT r; GetWindowRect(hwnd,&r); p->group.x=r.left; p->group.y=r.top; p->invalidateBackdrop(); SetTimer(hwnd,1,16,nullptr); return 0;
    }
    case WM_SIZE:
        if(wp!=SIZE_MINIMIZED && LOWORD(lp)>0) {
            p->group.width=std::max(260,static_cast<int>(LOWORD(lp)/p->scale)-inset*2);
            if(!p->group.collapsed) p->group.height=std::max(160,static_cast<int>(HIWORD(lp)/p->scale)-inset*2);
            p->invalidateBackdrop(); p->clampScroll(); SetTimer(hwnd,1,16,nullptr);
        } return 0;
    case WM_TIMER: if(wp==1) {KillTimer(hwnd,1); p->render();} return 0;
    case WM_EXITSIZEMOVE: {
        p->moving=false;
        if(p->app.snap && !(GetAsyncKeyState(VK_SHIFT)&0x8000)) {
            int grid=static_cast<int>(16*p->scale); p->group.x=static_cast<int>(std::round(p->group.x/static_cast<double>(grid)))*grid;
            p->group.y=static_cast<int>(std::round(p->group.y/static_cast<double>(grid)))*grid;
        }
        clampToMonitor(p->group,p->scale); p->resizeFromModel(); p->app.changed(); return 0;
    }
    case WM_DPICHANGED: {
        // Keep DIP dimensions stable when moving between monitors.
        int width=p->group.width,height=p->group.height; p->scale=HIWORD(wp)/96.f;
        auto* r=reinterpret_cast<RECT*>(lp); p->group.x=r->left; p->group.y=r->top; p->group.width=width; p->group.height=height;
        p->resizeFromModel(); p->app.changed(); return 0;
    }
    case WM_KEYDOWN:
        if(wp==VK_RETURN) p->activate(p->selected);
        else if(wp==VK_F2) p->app.openSettings(p->group.id);
        else if(wp==VK_F5) p->refreshItems();
        else if(wp==VK_ESCAPE) {p->selected=-1; p->render();}
        else if(wp==VK_LEFT || wp==VK_RIGHT || wp==VK_UP || wp==VK_DOWN) {
            int delta=wp==VK_LEFT ? -1 : wp==VK_RIGHT ? 1 : wp==VK_UP ? -p->columns() : p->columns();
            if(!p->entries.empty()) {p->selected=std::clamp(p->selected<0 ? 0 : p->selected+delta,0,static_cast<int>(p->entries.size())-1);
                int row=p->selected/p->columns(); if(row<p->scroll) p->scroll=row; if(row>=p->scroll+p->visibleRows()) p->scroll=row-p->visibleRows()+1; p->render();}
        } return 0;
    case WM_CLOSE: p->app.toggleVisible(); return 0;
    case WM_NCDESTROY: SetWindowLongPtrW(hwnd,GWLP_USERDATA,0); p->hwnd=nullptr; break;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
HRESULT Panel::QueryInterface(REFIID iid,void** out) {if(!out) return E_POINTER; *out=nullptr; if(iid==IID_IUnknown || iid==IID_IDropTarget) {*out=static_cast<IDropTarget*>(this); AddRef(); return S_OK;} return E_NOINTERFACE;}
ULONG Panel::AddRef() {return static_cast<ULONG>(InterlockedIncrement(&refs));}
ULONG Panel::Release() {return static_cast<ULONG>(InterlockedDecrement(&refs));}
HRESULT Panel::DragEnter(IDataObject* object,DWORD,POINTL,DWORD* effect) {
    FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL}; acceptsDrop=group.folder.empty() && SUCCEEDED(object->QueryGetData(&format));
    dropHover=acceptsDrop; *effect=acceptsDrop ? ((*effect&DROPEFFECT_LINK) ? DROPEFFECT_LINK : (*effect&DROPEFFECT_COPY)) : DROPEFFECT_NONE; render(); return S_OK;
}
HRESULT Panel::DragOver(DWORD,POINTL,DWORD* effect) {*effect=acceptsDrop ? ((*effect&DROPEFFECT_LINK) ? DROPEFFECT_LINK : (*effect&DROPEFFECT_COPY)) : DROPEFFECT_NONE; return S_OK;}
HRESULT Panel::DragLeave() {dropHover=false; acceptsDrop=false; render(); return S_OK;}
HRESULT Panel::Drop(IDataObject* object,DWORD,POINTL,DWORD* effect) {
    DWORD result=acceptsDrop ? ((*effect&DROPEFFECT_LINK) ? DROPEFFECT_LINK : (*effect&DROPEFFECT_COPY)) : DROPEFFECT_NONE;
    FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL}; STGMEDIUM medium{};
    if(result && SUCCEEDED(object->GetData(&format,&medium))) {
        auto drop=static_cast<HDROP>(medium.hGlobal); UINT count=DragQueryFileW(drop,0xffffffff,nullptr,0); std::vector<std::wstring> paths;
        for(UINT i=0;i<std::min<UINT>(count,512);++i) {UINT len=DragQueryFileW(drop,i,nullptr,0); std::wstring p(len+1,0); DragQueryFileW(drop,i,p.data(),len+1); p.resize(len); paths.push_back(std::move(p));}
        ReleaseStgMedium(&medium); addPaths(paths);
    } else result=DROPEFFECT_NONE;
    *effect=result; DragLeave(); return S_OK;
}
}
