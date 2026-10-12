#include "app.hpp"
#include <cmath>
#include <fstream>

using namespace Gdiplus;
namespace ft {
static App* active=nullptr;
static bool shellWindow(HWND window) {
    wchar_t cls[128]{}; GetClassNameW(window,cls,128);
    return wcscmp(cls,L"Progman")==0 || wcscmp(cls,L"WorkerW")==0 || wcscmp(cls,L"Shell_TrayWnd")==0 || wcscmp(cls,L"Shell_SecondaryTrayWnd")==0;
}
static void lowerPanels(App& app) {
    struct Order { App* app; HWND last=nullptr; } order{&app};
    EnumWindows([](HWND window,LPARAM value)->BOOL {
        auto* o=reinterpret_cast<Order*>(value); DWORD pid=0; GetWindowThreadProcessId(window,&pid);
        if(pid==GetCurrentProcessId() || !IsWindowVisible(window) || IsIconic(window) || shellWindow(window)) return TRUE;
        if(GetWindowLongPtrW(window,GWL_EXSTYLE)&(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE)) return TRUE;
        DWORD cloaked=0; DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)); if(cloaked) return TRUE;
        o->last=window; return TRUE;
    },reinterpret_cast<LPARAM>(&order));
    HWND after=order.last ? order.last : HWND_TOP;
    for(auto& p:app.panels) if(!p->moving) {SetWindowPos(p->hwnd,after,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE); after=p->hwnd;}
}
bool App::initialize(HINSTANCE h) {
    active=this; instance=h;
    GdiplusStartupInput input; if(GdiplusStartup(&gdiplus,&input,nullptr)!=Ok) return false;
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES|ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
    {
        Bitmap image(64,64,PixelFormat32bppPARGB); Graphics g(&image); g.SetSmoothingMode(SmoothingModeAntiAlias); g.Clear(Color(0,0,0,0));
        fillRound(g,RectF(3,3,58,58),17,Color(255,160,141,242));
        for(int row=0;row<3;++row) {fillRound(g,RectF(16,17.f+row*11,8,6),2,Color(255,25,24,39)); fillRound(g,RectF(30,17.f+row*11,row==2 ? 10.f : 18.f,6),2,Color(255,25,24,39));}
        image.GetHICON(&icon);
    }
    WNDCLASSEXW cls{}; cls.cbSize=sizeof(cls); cls.hInstance=h; cls.hCursor=LoadCursorW(nullptr,IDC_ARROW); cls.hIcon=icon; cls.hIconSm=icon;
    cls.lpfnWndProc=App::proc; cls.lpszClassName=L"FencesT.Controller"; if(!RegisterClassExW(&cls)) return false;
    cls.lpfnWndProc=Panel::proc; cls.lpszClassName=L"FencesT.Panel"; cls.style=CS_DBLCLKS; if(!RegisterClassExW(&cls)) return false;
    cls.lpfnWndProc=Settings::proc; cls.lpszClassName=L"FencesT.Settings"; cls.style=0; if(!RegisterClassExW(&cls)) return false;
    // Hidden top-level controller receives shell restart and display notifications.
    controller=CreateWindowExW(WS_EX_TOOLWINDOW,L"FencesT.Controller",L"FencesT",WS_POPUP,0,0,0,0,nullptr,nullptr,h,this);
    if(!controller) return false;
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    tray.cbSize=sizeof(tray); tray.hWnd=controller; tray.uID=ID_TRAY; tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP; tray.uCallbackMessage=WM_TRAY; tray.hIcon=icon;
    wcscpy_s(tray.szTip,L"FencesT • Рабочий стол по-твоему"); Shell_NotifyIconW(NIM_ADD,&tray);
    foregroundHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,App::foreground,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    reloadWallpaper(); settings=std::make_unique<Settings>(*this); load(); lowerPanels(*this);
    SetTimer(controller,2,1200,nullptr); return true;
}
State App::snapshot() const {State state; state.nextId=nextId; state.snap=snap; state.hotkeys=hotkeys; for(auto& p:panels) state.groups.push_back(p->group); return state;}
void App::restore(const State& state) {
    panels.clear(); nextId=state.nextId; snap=state.snap; hotkeys=state.hotkeys; registerHotkeys();
    for(const auto& group:state.groups) {auto p=std::make_unique<Panel>(*this,group); if(p->create()) panels.push_back(std::move(p));}
    if(settings && settings->hwnd) settings->refresh();
    lowerPanels(*this);
}
void App::createDefaults() {
    MONITORINFO info{}; info.cbSize=sizeof(info); GetMonitorInfoW(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&info);
    float scale=GetDpiForSystem()/96.f;
    Group apps; apps.id=1; apps.name=utf8(L"Приложения"); apps.width=344; apps.height=236;
    apps.x=info.rcWork.right-static_cast<int>(390*scale); apps.y=info.rcWork.top+static_cast<int>(48*scale);
    wchar_t windows[MAX_PATH]{}; GetWindowsDirectoryW(windows,MAX_PATH);
    apps.items={{utf8(std::wstring(windows)+L"\\explorer.exe"),utf8(L"Проводник")},{utf8(std::wstring(windows)+L"\\system32\\notepad.exe"),utf8(L"Блокнот")}};
    Group files=apps; files.id=2; files.name=utf8(L"Мои файлы"); files.y+=static_cast<int>(280*scale); files.items.clear();
    for(auto [id,title]:std::vector<std::pair<const KNOWNFOLDERID*,const wchar_t*>>{{&FOLDERID_Downloads,L"Загрузки"},{&FOLDERID_Documents,L"Документы"},{&FOLDERID_Pictures,L"Изображения"}}) {
        PWSTR path=nullptr; if(SUCCEEDED(SHGetKnownFolderPath(*id,0,nullptr,&path))) {files.items.push_back({utf8(path),utf8(title)}); CoTaskMemFree(path);}
    }
    State state; state.nextId=3; state.groups={apps,files}; restore(state); changed();
}
void App::load() {
    State state; auto path=dataPath(); auto backup=path; backup+=L".bak";
    if(loadState(path,state)) {restore(state); return;}
    bool existed=std::filesystem::exists(path);
    if(existed) {
        auto damaged=path; damaged+=L".damaged-"+std::to_wstring(GetTickCount64());
        // Keep unreadable data for recovery; never silently overwrite it.
        std::error_code error; std::filesystem::copy_file(path,damaged,std::filesystem::copy_options::none,error);
    }
    if(loadState(backup,state)) {restore(state); changed(); notify(L"Раскладка восстановлена",L"Загружена последняя резервная копия настроек.");}
    else {createDefaults(); if(existed) notify(L"Не удалось прочитать раскладку",L"Созданы начальные группы. Исходный файл сохранён для восстановления.");}
}
bool App::save() {
    bool ok=false; try {ok=saveState(dataPath(),snapshot());} catch(...) {}
    if(ok) dirty=false; else notify(L"Настройки не сохранены",L"Проверьте доступ к папке LocalAppData\\FencesT и свободное место на диске.");
    return ok;
}
void App::changed() {dirty=true; SetTimer(controller,1,450,nullptr);}
void App::registerHotkeys() {
    hotkeyOk=true;
    for(int i=0;i<2;++i) UnregisterHotKey(controller,i+1);
    for(int i=0;i<2;++i) {
        if(hotkeys[i].key && !RegisterHotKey(controller,i+1,hotkeys[i].modifiers|MOD_NOREPEAT,hotkeys[i].key)) hotkeyOk=false;
    }
    if(!hotkeyOk) notify(L"Горячая клавиша недоступна",L"Сочетание занято другой программой или Windows. Выберите другое в настройках FencesT.");
}
bool App::changeHotkeys(const Hotkeys& next,std::wstring& error) {
    if(!validHotkeys(next)) {
        error=next[0].key && next[0]==next[1] ? L"Для двух действий выберите разные сочетания." : L"Выберите модификатор Ctrl, Alt, Shift или Win. Без модификатора допускаются только F-клавиши. F12 недоступна.";
        return false;
    }
    const auto result=replaceHotkeys(hotkeys,next,[&](int i){UnregisterHotKey(controller,i+1);},
        [&](int i,Hotkey h){return RegisterHotKey(controller,i+1,h.modifiers|MOD_NOREPEAT,h.key)!=FALSE;});
    if(!result.success) {
        hotkeyOk=result.restored;
        error=L"Сочетание "+hotkeyText(next[result.failedAction])+L" занято другой программой или Windows. Выберите другое.";
        if(!result.restored) error+=L" Не удалось восстановить прежние сочетания; используйте значок в трее.";
        return false;
    }
    hotkeys=next; hotkeyOk=true; changed();
    if(!save()) {error=L"Сочетания применены, но сохранить их на диск не удалось."; return false;}
    return true;
}
Panel* App::find(uint32_t id) {for(auto& p:panels) if(p->group.id==id) return p.get(); return nullptr;}
void App::addGroup(const Group* source) {
    if(panels.size()>=maxGroups) {showError(settings ? settings->hwnd : nullptr,L"Достигнут лимит: 48 групп."); return;}
    Group group=source ? *source : Group{}; group.id=nextId++;
    if(source) {group.name+=utf8(L" — копия"); group.x+=32; group.y+=32;}
    else {group.name=utf8(L"Новая группа"); POINT point; GetCursorPos(&point); group.x=point.x-100; group.y=point.y-60;}
    group.locked=false; group.collapsed=false; auto panel=std::make_unique<Panel>(*this,group);
    if(!panel->create()) {showError(controller,L"Не удалось создать группу."); return;}
    panels.push_back(std::move(panel)); if(!visible) toggleVisible(); changed(); openSettings(group.id);
}
void App::deleteGroup(uint32_t id) {
    auto p=find(id); if(!p) return;
    HWND owner=settings && IsWindowVisible(settings->hwnd) ? settings->hwnd : p->hwnd;
    if(MessageBoxW(owner,L"Удалить эту группу?\nФайлы и приложения останутся на своих местах.",L"FencesT",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES) return;
    panels.erase(std::remove_if(panels.begin(),panels.end(),[&](auto& panel){return panel->group.id==id;}),panels.end()); changed();
    if(settings) settings->refresh();
}
void App::toggleVisible() {visible=!visible; if(visible) showPanels(); else for(auto& p:panels) ShowWindow(p->hwnd,SW_HIDE);}
void App::showPanels() {if(!visible) return; for(auto& p:panels) {ShowWindow(p->hwnd,SW_SHOWNOACTIVATE); p->render();} lowerPanels(*this);}
void App::openSettings(uint32_t id) {if(settings) settings->open(id);}
void App::notify(const wchar_t* title,const wchar_t* body) {
    auto data=tray; data.uFlags=NIF_INFO; data.dwInfoFlags=NIIF_INFO; wcsncpy_s(data.szInfoTitle,title,_TRUNCATE); wcsncpy_s(data.szInfo,body,_TRUNCATE); Shell_NotifyIconW(NIM_MODIFY,&data);
}
void App::trayMenu() {
    HMENU menu=CreatePopupMenu(); AppendMenuW(menu,MF_STRING,ID_SETTINGS,L"Открыть FencesT"); AppendMenuW(menu,MF_STRING,ID_NEW,L"Новая группа");
    auto showLabel=(visible ? L"Скрыть группы\t" : L"Показать группы\t")+hotkeyText(hotkeys[0]);
    AppendMenuW(menu,MF_STRING,ID_SHOW,showLabel.c_str());
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr); AppendMenuW(menu,MF_STRING,ID_EXPORT,L"Экспорт раскладки…"); AppendMenuW(menu,MF_STRING,ID_IMPORT,L"Восстановить раскладку…");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr); AppendMenuW(menu,MF_STRING,ID_QUIT,L"Выход");
    POINT pt; GetCursorPos(&pt); SetForegroundWindow(controller); TrackPopupMenu(menu,TPM_RIGHTBUTTON,pt.x,pt.y,0,controller,nullptr); DestroyMenu(menu); PostMessageW(controller,WM_NULL,0,0);
}
void App::exportLayout() {
    auto owner=settings ? settings->hwnd : controller; auto path=pickLayout(owner,true); if(path.empty()) return;
    if(!saveState(path,snapshot(),false)) showError(owner,L"Не удалось сохранить резервную копию."); else notify(L"Раскладка экспортирована",L"Настройки и ссылки на файлы сохранены.");
}
void App::importLayout() {
    auto owner=settings ? settings->hwnd : controller; auto path=pickLayout(owner,false); if(path.empty()) return; State state;
    if(!loadState(path,state)) {showError(owner,L"Файл повреждён или имеет неподдерживаемый формат."); return;}
    if(MessageBoxW(owner,L"Заменить текущие группы раскладкой из файла?\nТекущая раскладка будет сохранена в before-import.fencest.",L"Восстановление",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES) return;
    if(!saveState(dataPath().parent_path()/L"before-import.fencest",snapshot(),false)) {showError(owner,L"Не удалось создать копию текущей раскладки. Восстановление отменено."); return;}
    restore(state); changed(); save(); if(settings) settings->refresh();
}
void App::shutdown() {
    closing=true; if(controller) {KillTimer(controller,1); KillTimer(controller,2);} if(dirty) save();
    if(foregroundHook) UnhookWinEvent(foregroundHook);
    foregroundHook=nullptr;
    if(controller) {UnregisterHotKey(controller,1); UnregisterHotKey(controller,2); Shell_NotifyIconW(NIM_DELETE,&tray);}
    settings.reset(); panels.clear(); wallpapers.clear(); if(controller) DestroyWindow(controller); controller=nullptr;
    if(icon) DestroyIcon(icon);
    icon=nullptr;
    if(gdiplus) GdiplusShutdown(gdiplus);
    gdiplus=0; active=nullptr;
}
void CALLBACK App::foreground(HWINEVENTHOOK,DWORD,HWND window,LONG,LONG,DWORD,DWORD) {if(active && !active->closing) PostMessageW(active->controller,WM_DESKTOP,reinterpret_cast<WPARAM>(window),0);}
LRESULT CALLBACK App::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(!app) return DefWindowProcW(hwnd,msg,wp,lp);
    if(msg==app->taskbarCreated && app->taskbarCreated) {Shell_NotifyIconW(NIM_ADD,&app->tray); app->showPanels(); return 0;}
    switch(msg) {
    case WM_TRAY: if(lp==WM_LBUTTONDBLCLK) app->openSettings(); else if(lp==WM_RBUTTONUP || lp==WM_CONTEXTMENU) app->trayMenu(); return 0;
    case WM_COMMAND:
        switch(LOWORD(wp)) {case ID_SHOW: app->toggleVisible(); break; case ID_NEW: app->addGroup(); break; case ID_SETTINGS: app->openSettings(); break;
            case ID_EXPORT: app->exportLayout(); break; case ID_IMPORT: app->importLayout(); break; case ID_QUIT: PostQuitMessage(0); break;} return 0;
    case WM_HOTKEY: if(wp==1) app->toggleVisible(); else if(wp==2) app->addGroup(); return 0;
    case WM_REOPEN: app->openSettings(); return 0;
    case WM_APP+20: app->deleteGroup(static_cast<uint32_t>(wp)); return 0;
    case WM_DESKTOP:
        if(app->visible) {HWND window=reinterpret_cast<HWND>(wp); if(IsWindow(window) && shellWindow(window)) app->showPanels(); else lowerPanels(*app);} return 0;
    case WM_TIMER:
        if(wp==1) {KillTimer(hwnd,1); if(app->dirty) app->save();}
        if(wp==2) for(auto& p:app->panels) {
            if(p->folderWatch!=INVALID_HANDLE_VALUE && WaitForSingleObject(p->folderWatch,0)==WAIT_OBJECT_0) {
                FindNextChangeNotification(p->folderWatch); p->refreshItems(); if(app->settings && app->settings->target==p->group.id) app->settings->fillItems();
            }
        } return 0;
    case WM_SETTINGCHANGE: case WM_DISPLAYCHANGE:
        app->reloadWallpaper(); for(auto& p:app->panels) {clampToMonitor(p->group,p->scale); p->resizeFromModel();} app->changed(); return 0;
    case WM_QUERYENDSESSION: app->save(); return TRUE;
    case WM_ENDSESSION: if(wp) {app->save(); PostQuitMessage(0);} return 0;
    case WM_CLOSE: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\FencesT.SingleInstance.v1");
    if(mutex && GetLastError()==ERROR_ALREADY_EXISTS) {
        if(HWND existing=FindWindowW(L"FencesT.Controller",L"FencesT")) {DWORD pid=0; GetWindowThreadProcessId(existing,&pid); AllowSetForegroundWindow(pid); PostMessageW(existing,ft::WM_REOPEN,0,0);}
        CloseHandle(mutex); return 0;
    }
    if(FAILED(OleInitialize(nullptr))) {if(mutex) CloseHandle(mutex); return 1;}
    ft::App app; int code=0;
    try {
        if(!app.initialize(instance)) throw std::runtime_error("Initialization failed");
        if(!wcsstr(command,L"--background")) app.openSettings();
        MSG msg{}; BOOL result;
        while((result=GetMessageW(&msg,nullptr,0,0))>0) {
            if(app.settings && IsWindowVisible(app.settings->hwnd) && IsDialogMessageW(app.settings->hwnd,&msg)) continue;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        code=result<0 ? 1 : static_cast<int>(msg.wParam);
    } catch(const std::exception& error) {ft::showError(nullptr,L"Не удалось запустить FencesT.\n"+ft::wide(error.what())); code=1;}
    app.shutdown(); OleUninitialize(); if(mutex) CloseHandle(mutex); return code;
}
