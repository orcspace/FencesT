#include "app.hpp"
#include <cwctype>
using namespace Gdiplus;
namespace ft {
enum Control { Groups=1000, New, Duplicate, Delete, TabLook=1100, TabItems, TabApp,
    Name=1200, Glass, Clear, Matte, Opacity=1210, Radius, Blur, IconSize, ImageOpacity,
    Labels=1220, Lock, Tint, Background, ClearBackground,
    Search=1300, Items, AddFiles, AddFolder, Remove, Portal, ClearPortal, Launch,
    Startup=1400, Snap, Export, Import, Project };
static bool appearance(int id) {return id>=1200 && id<1300;}
static bool shortcuts(int id) {return id>=1300 && id<1400;}
static bool global(int id) {return id>=1400;}
static std::wstring lowered(std::wstring value) {for(auto& c:value) c=static_cast<wchar_t>(towlower(c)); return value;}
Settings::~Settings() {if(hwnd) DestroyWindow(hwnd); if(font) DeleteObject(font); if(titleFont) DeleteObject(titleFont); if(brush) DeleteObject(brush); if(fieldBrush) DeleteObject(fieldBrush);}
Panel* Settings::panel() {return app.find(target);}
void Settings::open(uint32_t id) {
    if(id) target=id;
    if(!hwnd) {
        MONITORINFO info{}; info.cbSize=sizeof(info); POINT cursor; GetCursorPos(&cursor); GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&info);
        scale=std::min({GetDpiForSystem()/96.f,(info.rcWork.right-info.rcWork.left)/1040.f,(info.rcWork.bottom-info.rcWork.top)/760.f});
        RECT r{0,0,static_cast<LONG>(1000*scale),static_cast<LONG>(700*scale)};
        AdjustWindowRectEx(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0);
        hwnd=CreateWindowExW(0,L"FencesT.Settings",L"FencesT — Рабочий стол по-твоему",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
            info.rcWork.left+((info.rcWork.right-info.rcWork.left)-(r.right-r.left))/2,info.rcWork.top+((info.rcWork.bottom-info.rcWork.top)-(r.bottom-r.top))/2,
            r.right-r.left,r.bottom-r.top,nullptr,nullptr,app.instance,this);
    }
    refresh(); ShowWindow(hwnd,SW_RESTORE); SetForegroundWindow(hwnd);
}
void Settings::build() {
    font=CreateFontW(-static_cast<int>(13*scale),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    titleFont=CreateFontW(-static_cast<int>(15*scale),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    brush=CreateSolidBrush(RGB(18,20,27)); fieldBrush=CreateSolidBrush(RGB(36,39,50));
    auto add=[&](int id,const wchar_t* cls,const wchar_t* text,DWORD style) {
        HWND child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,0,0,1,1,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),app.instance,nullptr);
        controls[id]=child; SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return child;
    };
    add(Groups,L"LISTBOX",L"Группы",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_VSCROLL);
    SendMessageW(controls[Groups],LB_SETITEMHEIGHT,0,static_cast<LPARAM>(52*scale));
    for(auto [id,text]:std::vector<std::pair<int,const wchar_t*>>{{New,L"+ Новая группа"},{Duplicate,L"Дублировать"},{Delete,L"Удалить"},
        {TabLook,L"Оформление"},{TabItems,L"Ярлыки"},{TabApp,L"Настройки"},{Glass,L"Liquid Glass"},{Clear,L"Прозрачный"},{Matte,L"Тёмный матовый"},
        {Tint,L"Цвет фона"},{Background,L"Выбрать картинку…"},{ClearBackground,L"Убрать картинку"},
        {AddFiles,L"+ Файлы и приложения"},{AddFolder,L"+ Ярлык папки"},{Remove,L"Убрать ярлык"},{Portal,L"Связать с папкой…"},
        {ClearPortal,L"Отключить связь"},{Launch,L"Открыть"},{Export,L"Экспорт раскладки…"},{Import,L"Восстановить…"},{Project,L"Открыть GitHub"}})
        add(id,L"BUTTON",text,BS_OWNERDRAW);
    add(Name,L"EDIT",L"",ES_AUTOHSCROLL); SendMessageW(controls[Name],EM_SETLIMITTEXT,80,0);
    add(Search,L"EDIT",L"",ES_AUTOHSCROLL); SendMessageW(controls[Search],EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Поиск по названию или пути…"));
    for(auto [id,text]:std::vector<std::pair<int,const wchar_t*>>{{Labels,L"Показывать названия приложений"},{Lock,L"Закрепить положение и размер"},
        {Startup,L"Запускать вместе с Windows"},{Snap,L"Выравнивать группы по сетке"}}) add(id,L"BUTTON",text,BS_AUTOCHECKBOX);
    for(int id=Opacity;id<=ImageOpacity;++id) {
        auto slider=add(id,TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS);
        int lo=id==IconSize ? 24 : 0, hi=id==Radius ? 64 : id==Blur ? 30 : id==IconSize ? 72 : 100;
        SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELONG(lo,hi)); SendMessageW(slider,TBM_SETPAGESIZE,0,4);
    }
    auto items=add(Items,WC_LISTVIEWW,L"Ярлыки группы",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER);
    ListView_SetExtendedListViewStyle(items,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
    ListView_SetBkColor(items,RGB(27,30,40)); ListView_SetTextBkColor(items,RGB(27,30,40)); ListView_SetTextColor(items,RGB(235,235,245));
    LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.pszText=const_cast<LPWSTR>(L"Название"); col.cx=static_cast<int>(235*scale); ListView_InsertColumn(items,0,&col);
    col.pszText=const_cast<LPWSTR>(L"Путь"); col.cx=static_cast<int>(420*scale); ListView_InsertColumn(items,1,&col);
    BOOL dark=TRUE; DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));
    SendMessageW(hwnd,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(app.icon)); SendMessageW(hwnd,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(app.icon));
    layout();
}
void Settings::layout() {
    auto place=[&](int id,int x,int y,int w,int h){SetWindowPos(controls[id],nullptr,static_cast<int>(x*scale),static_cast<int>(y*scale),static_cast<int>(w*scale),static_cast<int>(h*scale),SWP_NOZORDER|SWP_NOACTIVATE);};
    place(Groups,18,142,208,356); place(New,18,516,208,40); place(Duplicate,18,566,100,36); place(Delete,126,566,100,36);
    place(TabLook,274,105,190,40); place(TabItems,474,105,190,40); place(TabApp,674,105,290,40);
    place(Name,278,195,680,30);
    place(Glass,278,259,214,50); place(Clear,508,259,214,50); place(Matte,738,259,220,50);
    place(Opacity,275,353,402,26); place(Radius,275,420,402,26); place(Blur,275,487,402,26); place(IconSize,275,554,402,26);
    place(Labels,278,604,405,28); place(Lock,278,637,405,28);
    place(Tint,726,348,232,36); place(Background,726,402,232,36); place(ClearBackground,726,446,232,32);
    place(ImageOpacity,722,522,240,28);
    place(Search,278,188,680,32); place(Items,278,234,680,283);
    place(AddFiles,278,531,250,38); place(AddFolder,540,531,200,38); place(Remove,752,531,206,38);
    place(Portal,278,587,250,38); place(ClearPortal,540,587,240,38); place(Launch,792,587,166,38);
    place(Startup,278,199,650,32); place(Snap,278,253,650,32);
    place(Export,278,382,330,42); place(Import,628,382,330,42); place(Project,278,569,220,38);
    for(auto [id,child]:controls) {
        bool visible=(!appearance(id) && !shortcuts(id) && !global(id)) || (appearance(id)&&tab==0) || (shortcuts(id)&&tab==1) || (global(id)&&tab==2);
        ShowWindow(child,visible ? SW_SHOW : SW_HIDE);
    }
    InvalidateRect(hwnd,nullptr,TRUE);
}
void Settings::refreshGroups() {
    SendMessageW(controls[Groups],LB_RESETCONTENT,0,0); int selected=0;
    if(!app.find(target) && !app.panels.empty()) target=app.panels.front()->group.id;
    for(size_t i=0;i<app.panels.size();++i) {
        auto& g=app.panels[i]->group; auto name=wide(g.name); auto row=SendMessageW(controls[Groups],LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
        SendMessageW(controls[Groups],LB_SETITEMDATA,row,g.id); if(g.id==target) selected=static_cast<int>(i);
    }
    SendMessageW(controls[Groups],LB_SETCURSEL,selected,0);
}
void Settings::selectGroup(uint32_t id) {target=id; refresh();}
void Settings::refresh(bool list) {
    if(!hwnd) return;
    loading=true;
    if(list) refreshGroups();
    auto p=panel(); bool exists=p!=nullptr;
    for(auto [id,child]:controls) if(appearance(id)||shortcuts(id)||id==Duplicate||id==Delete) EnableWindow(child,exists);
    if(p) {
        const auto& g=p->group; SetWindowTextW(controls[Name],wide(g.name).c_str());
        for(auto [id,value]:std::vector<std::pair<int,int>>{{Opacity,100-g.opacity},{Radius,g.radius},{Blur,g.blur},{IconSize,g.iconSize},{ImageOpacity,g.imageOpacity}})
            SendMessageW(controls[id],TBM_SETPOS,TRUE,value);
        SendMessageW(controls[Labels],BM_SETCHECK,g.showLabels ? BST_CHECKED : BST_UNCHECKED,0);
        SendMessageW(controls[Lock],BM_SETCHECK,g.locked ? BST_CHECKED : BST_UNCHECKED,0);
        EnableWindow(controls[Blur],g.style!=Style::Clear); EnableWindow(controls[ClearBackground],!g.image.empty());
        EnableWindow(controls[ImageOpacity],!g.image.empty()); EnableWindow(controls[AddFiles],g.folder.empty()); EnableWindow(controls[AddFolder],g.folder.empty());
        EnableWindow(controls[Remove],g.folder.empty()); EnableWindow(controls[ClearPortal],!g.folder.empty());
    }
    SendMessageW(controls[Startup],BM_SETCHECK,startupEnabled() ? BST_CHECKED : BST_UNCHECKED,0);
    SendMessageW(controls[Snap],BM_SETCHECK,app.snap ? BST_CHECKED : BST_UNCHECKED,0);
    fillItems(); loading=false; InvalidateRect(hwnd,nullptr,TRUE); for(auto [id,child]:controls) if(id>=Glass&&id<=Matte) InvalidateRect(child,nullptr,TRUE);
}
void Settings::fillItems() {
    if(!hwnd) return;
    ListView_DeleteAllItems(controls[Items]); filtered.clear(); auto p=panel(); if(!p) return;
    auto query=lowered(windowText(controls[Search]));
    for(size_t i=0;i<p->entries.size();++i) {
        auto name=wide(p->entries[i].item.label),path=wide(p->entries[i].item.path);
        if(!query.empty() && lowered(name+L" "+path).find(query)==std::wstring::npos) continue;
        LVITEMW row{}; row.mask=LVIF_TEXT; row.iItem=static_cast<int>(filtered.size()); row.pszText=name.data();
        int at=ListView_InsertItem(controls[Items],&row); ListView_SetItemText(controls[Items],at,1,path.data()); filtered.push_back(i);
    }
}
void Settings::update(int id) {
    if(loading) return;
    auto p=panel(); if(!p) return;
    int value=static_cast<int>(SendMessageW(controls[id],TBM_GETPOS,0,0));
    if(id==Opacity) p->group.opacity=100-value;
    if(id==Radius) p->group.radius=value;
    if(id==Blur) p->group.blur=value;
    if(id==IconSize) p->group.iconSize=value;
    if(id==ImageOpacity) p->group.imageOpacity=value;
    p->apply(); InvalidateRect(hwnd,nullptr,FALSE);
}
void Settings::command(int id) {
    if(loading) return;
    if(id>=TabLook&&id<=TabApp) {tab=id-TabLook; layout(); return;}
    if(id==New) {app.addGroup(); return;}
    if(id==Export) {app.exportLayout(); return;} if(id==Import) {app.importLayout(); return;}
    if(id==Startup) {if(!setStartup(SendMessageW(controls[Startup],BM_GETCHECK,0,0)==BST_CHECKED)) showError(hwnd,L"Не удалось изменить автозапуск."); refresh(false); return;}
    if(id==Snap) {app.snap=SendMessageW(controls[Snap],BM_GETCHECK,0,0)==BST_CHECKED; app.changed(); return;}
    if(id==Project) {ShellExecuteW(hwnd,L"open",L"https://github.com/orcspace/FencesT",nullptr,nullptr,SW_SHOWNORMAL); return;}
    auto p=panel(); if(!p) return;
    switch(id) {
    case Duplicate: app.addGroup(&p->group); return;
    case Delete: app.deleteGroup(target); return;
    case Name: {auto value=windowText(controls[Name]); if(!value.empty()) {p->group.name=utf8(value); p->apply(); refreshGroups();} return;}
    case Glass: case Clear: case Matte:
        p->group.style=static_cast<Style>(id-Glass); break;
    case Labels: p->group.showLabels=SendMessageW(controls[Labels],BM_GETCHECK,0,0)==BST_CHECKED; break;
    case Lock: p->group.locked=SendMessageW(controls[Lock],BM_GETCHECK,0,0)==BST_CHECKED; break;
    case Tint: {
        static COLORREF custom[16]{}; CHOOSECOLORW c{}; c.lStructSize=sizeof(c); c.hwndOwner=hwnd; c.lpCustColors=custom; auto tint=p->group.tint;
        c.rgbResult=RGB((tint>>16)&255,(tint>>8)&255,tint&255); c.Flags=CC_FULLOPEN|CC_RGBINIT;
        if(ChooseColorW(&c)) p->group.tint=(GetRValue(c.rgbResult)<<16)|(GetGValue(c.rgbResult)<<8)|GetBValue(c.rgbResult);
        break;
    }
    case Background: {
        auto files=pickFiles(hwnd,false,true); if(!files.empty()) {
            auto image=loadImage(files[0]); if(!image) {showError(hwnd,L"Не удалось прочитать изображение. Выберите PNG, JPEG или BMP."); return;}
            p->group.image=utf8(files[0]); p->background=std::move(image);
        } break;
    }
    case ClearBackground: p->group.image.clear(); p->background.reset(); break;
    case AddFiles: p->addPaths(pickFiles(hwnd)); return;
    case AddFolder: p->addPaths(pickFiles(hwnd,true)); return;
    case Portal: {
        auto folder=pickFiles(hwnd,true); if(!folder.empty()) {p->group.folder=utf8(folder[0]); p->refreshItems(); p->updateWatch();} break;
    }
    case ClearPortal: p->group.folder.clear(); p->refreshItems(); p->updateWatch(); break;
    case Remove: case Launch: {
        int row=ListView_GetNextItem(controls[Items],-1,LVNI_SELECTED);
        if(row>=0 && row<static_cast<int>(filtered.size())) {
            auto index=filtered[row];
            if(id==Launch) p->activate(static_cast<int>(index));
            else if(p->group.folder.empty() && index<p->group.items.size()) {p->group.items.erase(p->group.items.begin()+static_cast<ptrdiff_t>(index)); p->refreshItems();}
        } break;
    }
    }
    p->apply(); refresh(false);
}
void Settings::paint() {
    PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT rect; GetClientRect(hwnd,&rect);
    Bitmap image(rect.right,rect.bottom,PixelFormat32bppPARGB);
    {
        Graphics g(&image); g.ScaleTransform(scale,scale); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.Clear(Color(255,18,20,27)); SolidBrush side(Color(255,23,25,34)); g.FillRectangle(&side,0,0,244,700);
        fillRound(g,RectF(24,28,40,40),12,Color(255,166,149,255));
        for(int row=0;row<3;++row) {fillRound(g,RectF(34,38.f+row*8,6,4),1,Color(255,23,20,40)); fillRound(g,RectF(44,38.f+row*8,row==2?7.f:11.f,4),1,Color(255,23,20,40));}
        drawText(g,L"FencesT",RectF(76,27,154,28),23,Color(255,246,246,255),true);
        drawText(g,L"ТВОЙ РАБОЧИЙ СТОЛ",RectF(26,102,200,22),10,Color(255,129,133,153),true);
        drawText(g,L"Win + Alt + F",RectF(26,636,190,22),12,Color(255,195,182,255));
        drawText(g,L"Показать / скрыть группы",RectF(26,658,200,18),10,Color(255,132,138,156));
        auto p=panel(); std::wstring title=tab==2 ? L"Всё на своём месте" : p ? wide(p->group.name) : L"Создайте первую группу";
        drawText(g,title,RectF(278,24,674,38),27,Color(255,246,246,252),true);
        drawText(g,tab==2 ? L"Раскладка, запуск и быстрый доступ." : L"Настройки применяются сразу на рабочем столе.",RectF(278,65,680,24),12,Color(255,141,147,169));
        auto label=[&](const wchar_t* t,int x,int y,int w=400){drawText(g,t,RectF(static_cast<float>(x),static_cast<float>(y),static_cast<float>(w),24),12,Color(255,185,190,209));};
        if(tab==0) {
            label(L"Название группы",278,165); label(L"Материал",278,231);
            label(L"Прозрачность фона",278,324); label(L"Скругление углов",278,391); label(L"Размытие",278,458); label(L"Размер значков",278,525);
            label(L"Картинка — непрозрачность",726,492,250);
            if(p) {
                auto& m=p->group;
                for(auto [value,y]:std::vector<std::pair<std::wstring,int>>{{std::to_wstring(100-m.opacity)+L" %",324},{std::to_wstring(m.radius)+L" px",391},{std::to_wstring(m.blur)+L" px",458},{std::to_wstring(m.iconSize)+L" px",525}})
                    drawText(g,value,RectF(595,static_cast<float>(y),75,24),12,Color(255,195,182,255),true);
                label(m.image.empty() ? L"Свой фон: PNG / JPG / BMP" : L"Картинка установлена",726,559,250);
                label(L"Значки остаются чёткими",726,604,250); label(L"при любой прозрачности.",726,626,250);
            }
        } else if(tab==1) {
            label(L"Поиск ярлыков в выбранной группе",278,159,670);
            auto caption=p && !p->group.folder.empty() ? L"Папка: "+wide(p->group.folder) : L"Ярлыки ссылаются на оригиналы. Файлы остаются на своих местах.";
            drawText(g,caption,RectF(278,642,680,36),11,Color(255,142,149,170));
        } else {
            label(L"РАБОТА ПРИЛОЖЕНИЯ",278,157,600);
            label(L"Удерживайте Shift при перемещении, чтобы временно отключить сетку.",278,298,690);
            label(L"РЕЗЕРВНАЯ КОПИЯ",278,341,600);
            label(L"В файл сохраняются расположение, оформление и ссылки на ваши файлы.",278,440,690);
            label(L"Сами приложения, документы и фоновые картинки в копию не включаются.",278,465,690);
            label(L"Win + Alt + F — скрыть / показать.  Win + Alt + N — новая группа.",278,516,690);
            label(app.hotkeyOk ? L"F2 — оформление группы.  F5 — обновить.  Двойной щелчок — открыть." : L"Горячая клавиша занята. Используйте меню значка в трее.",278,541,690);
            label(L"FencesT 0.1 • Windows • C++",278,643,690);
        }
    }
    Graphics screen(dc); screen.DrawImage(&image,0,0); EndPaint(hwnd,&ps);
}
LRESULT CALLBACK Settings::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto s=reinterpret_cast<Settings*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {s=static_cast<Settings*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); s->hwnd=hwnd; SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}
    if(!s) return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg) {
    case WM_CREATE: s->build(); return 0;
    case WM_PAINT: s->paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: s->app.save(); ShowWindow(hwnd,SW_HIDE); return 0;
    case WM_COMMAND: {
        int id=LOWORD(wp),code=HIWORD(wp);
        if(s->loading) return 0;
        if(id==Groups && code==LBN_SELCHANGE) {auto row=SendMessageW(s->controls[Groups],LB_GETCURSEL,0,0); s->target=static_cast<uint32_t>(SendMessageW(s->controls[Groups],LB_GETITEMDATA,row,0)); s->refresh(false);}
        else if(id==Name && code==EN_KILLFOCUS) s->command(id);
        else if(id==Search && code==EN_CHANGE) s->fillItems();
        else if(code==BN_CLICKED) s->command(id);
        return 0;
    }
    case WM_HSCROLL: s->update(GetDlgCtrlID(reinterpret_cast<HWND>(lp))); return 0;
    case WM_NOTIFY: {
        auto h=reinterpret_cast<NMHDR*>(lp); if(h->idFrom==Items && h->code==NM_DBLCLK) s->command(Launch); return 0;
    }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: {
        auto dc=reinterpret_cast<HDC>(wp); SetTextColor(dc,RGB(227,230,240)); SetBkColor(dc,RGB(36,39,50));
        if(msg==WM_CTLCOLORSTATIC || msg==WM_CTLCOLORBTN) {SetBkColor(dc,RGB(18,20,27)); return reinterpret_cast<LRESULT>(s->brush);}
        return reinterpret_cast<LRESULT>(s->fieldBrush);
    }
    case WM_DRAWITEM: {
        auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp); if(item->itemID==static_cast<UINT>(-1)) return TRUE;
        Graphics g(item->hDC); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        auto r=item->rcItem; RectF box(static_cast<float>(r.left),static_cast<float>(r.top),static_cast<float>(r.right-r.left),static_cast<float>(r.bottom-r.top));
        bool chosen=false; int id=static_cast<int>(item->CtlID); auto p=s->panel();
        if(id==Groups) chosen=(item->itemState&ODS_SELECTED)!=0;
        else if(id>=TabLook&&id<=TabApp) chosen=id==TabLook+s->tab;
        else if(id>=Glass&&id<=Matte && p) chosen=id==Glass+static_cast<int>(p->group.style);
        else if(id==New) chosen=true;
        bool disabled=(item->itemState&ODS_DISABLED)!=0;
        SolidBrush clear(Color(255,id==Groups ? 23 : 18,id==Groups ? 25 : 20,id==Groups ? 34 : 27)); g.FillRectangle(&clear,box);
        fillRound(g,RectF(box.X+2,box.Y+2,box.Width-4,box.Height-4),8*s->scale,
            chosen ? Color(255,63,54,90) : (item->itemState&ODS_SELECTED) ? Color(255,55,57,74) : Color(255,34,37,49));
        if(chosen) {GraphicsPath outline; roundPath(outline,RectF(box.X+2,box.Y+2,box.Width-4,box.Height-4),8*s->scale); Pen pen(Color(255,129,109,190),s->scale); g.DrawPath(&pen,&outline);}
        std::wstring text;
        if(id==Groups) {auto group=s->app.find(static_cast<uint32_t>(item->itemData)); if(group) text=wide(group->group.name);}
        else text=windowText(item->hwndItem);
        drawText(g,text,RectF(box.X+12*s->scale,box.Y,box.Width-24*s->scale,box.Height),13*s->scale,
            disabled ? Color(255,99,103,120) : chosen ? Color(255,222,210,255) : Color(255,218,223,237),chosen,id!=Groups);
        if(item->itemState&ODS_FOCUS) {RECT focus=r; InflateRect(&focus,-5,-5); DrawFocusRect(item->hDC,&focus);} return TRUE;
    }
    case WM_DPICHANGED: {
        // This compact settings window is scaled to fit the destination work area.
        auto suggested=reinterpret_cast<RECT*>(lp); MONITORINFO info{}; info.cbSize=sizeof(info); GetMonitorInfoW(MonitorFromRect(suggested,MONITOR_DEFAULTTONEAREST),&info);
        s->scale=std::min({HIWORD(wp)/96.f,(info.rcWork.right-info.rcWork.left)/1040.f,(info.rcWork.bottom-info.rcWork.top)/760.f});
        for(auto [id,child]:s->controls) DestroyWindow(child);
        s->controls.clear();
        DeleteObject(s->font); DeleteObject(s->titleFont); DeleteObject(s->brush); DeleteObject(s->fieldBrush); s->build();
        RECT r{0,0,static_cast<LONG>(1000*s->scale),static_cast<LONG>(700*s->scale)}; AdjustWindowRectEx(&r,static_cast<DWORD>(GetWindowLongW(hwnd,GWL_STYLE)),FALSE,0);
        SetWindowPos(hwnd,nullptr,suggested->left,suggested->top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE); s->refresh(); return 0;
    }
    case WM_NCDESTROY: s->hwnd=nullptr; SetWindowLongPtrW(hwnd,GWLP_USERDATA,0); break;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}
