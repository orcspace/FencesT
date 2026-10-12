#include "app.hpp"
#include <shlwapi.h>
#include <fstream>
#include <sstream>

namespace ft {
std::wstring hotkeyText(Hotkey h) {
    if(!h.key) return L"Отключено";
    std::wstring text;
    if(h.modifiers&2) text+=L"Ctrl + ";
    if(h.modifiers&1) text+=L"Alt + ";
    if(h.modifiers&4) text+=L"Shift + ";
    if(h.modifiers&8) text+=L"Win + ";
    if(h.key>=0x70 && h.key<=0x87) text+=L"F"+std::to_wstring(h.key-0x70+1);
    else if(h.key==VK_SPACE) text+=L"Space";
    else if(h.key==VK_TAB) text+=L"Tab";
    else if(h.key>=0x21 && h.key<=0x28) {const wchar_t* names[]={L"Page Up",L"Page Down",L"End",L"Home",L"Left",L"Up",L"Right",L"Down"}; text+=names[h.key-0x21];}
    else if(h.key==VK_INSERT) text+=L"Insert";
    else if(h.key==VK_DELETE) text+=L"Delete";
    else text+=static_cast<wchar_t>(h.key);
    return text;
}
std::wstring wide(const std::string& s) {
    if(s.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(n<=0) return L"?";
    std::wstring out(n,L'\0'); MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n); return out;
}
std::string utf8(const std::wstring& s) {
    if(s.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    std::string out(n,'\0'); WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr); return out;
}
std::wstring windowText(HWND hwnd) {int n=GetWindowTextLengthW(hwnd); std::wstring s(n+1,0); GetWindowTextW(hwnd,s.data(),n+1); s.resize(n); return s;}
std::wstring labelFor(const std::wstring& path) {
    auto p=std::filesystem::path(path); auto name=p.filename().wstring();
    if(name.empty()) name=path;
    if(_wcsicmp(p.extension().c_str(),L".lnk")==0 || _wcsicmp(p.extension().c_str(),L".url")==0) name=p.stem().wstring();
    return name;
}
void showError(HWND owner,const std::wstring& text) { MessageBoxW(owner,text.c_str(),L"FencesT",MB_OK|MB_ICONWARNING); }
void openItem(HWND owner,const Item& item) {
    auto path=wide(item.path);
    auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(owner,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL));
    if(result<=32) showError(owner,L"Не удалось открыть ярлык. Возможно, файл был перемещён или удалён.\n\n"+path);
}
std::vector<std::wstring> pickFiles(HWND owner,bool folder,bool image) {
    std::vector<std::wstring> paths; IFileOpenDialog* dialog=nullptr;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) return paths;
    DWORD options=0; dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_FILEMUSTEXIST|FOS_NOCHANGEDIR;
    if(folder) options |= FOS_PICKFOLDERS;
    else if(!image) options |= FOS_ALLOWMULTISELECT;
    dialog->SetOptions(options); dialog->SetTitle(folder ? L"Выберите папку" : image ? L"Фон группы" : L"Добавить ярлыки");
    COMDLG_FILTERSPEC images[]={{L"Изображения",L"*.png;*.jpg;*.jpeg;*.bmp"}};
    if(image) dialog->SetFileTypes(1,images);
    if(SUCCEEDED(dialog->Show(owner))) {
        IShellItemArray* results=nullptr;
        if(SUCCEEDED(dialog->GetResults(&results))) {
            DWORD count=0; results->GetCount(&count);
            for(DWORD i=0;i<std::min<DWORD>(count,512);++i) {
                IShellItem* item=nullptr; if(SUCCEEDED(results->GetItemAt(i,&item))) {
                    PWSTR value=nullptr; if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&value))) {paths.emplace_back(value); CoTaskMemFree(value);} item->Release();
                }
            }
            results->Release();
        }
    }
    dialog->Release(); return paths;
}
std::wstring pickLayout(HWND owner,bool save) {
    IFileDialog* dialog=nullptr;
    if(FAILED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) return {};
    DWORD options=0; dialog->GetOptions(&options); dialog->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR);
    COMDLG_FILTERSPEC types[]={{L"Раскладка FencesT",L"*.fencest"}}; dialog->SetFileTypes(1,types); dialog->SetDefaultExtension(L"fencest");
    dialog->SetFileName(L"My desktop.fencest"); dialog->SetTitle(save ? L"Экспорт раскладки" : L"Восстановить раскладку");
    std::wstring path;
    if(SUCCEEDED(dialog->Show(owner))) {IShellItem* item=nullptr; if(SUCCEEDED(dialog->GetResult(&item))) {
        PWSTR value=nullptr; if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&value))) {path=value; CoTaskMemFree(value);} item->Release();
    }}
    dialog->Release(); return path;
}
std::filesystem::path dataPath() {
    PWSTR dir=nullptr; std::filesystem::path path;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&dir))) {path=dir; CoTaskMemFree(dir);}
    if(path.empty()) throw std::runtime_error("LocalAppData unavailable");
    path/=L"FencesT"; std::filesystem::create_directories(path); return path/L"layout.fencest";
}
bool loadState(const std::filesystem::path& path,State& state) {
    try {
        std::ifstream file(path,std::ios::binary|std::ios::ate); if(!file) return false;
        const auto length=file.tellg(); if(length<0 || length>static_cast<std::streamoff>(maxStateBytes)) return false;
        file.seekg(0); std::vector<uint8_t> bytes(static_cast<size_t>(length));
        if(!file.read(reinterpret_cast<char*>(bytes.data()),length)) return false;
        State decoded=decode(bytes); state=std::move(decoded); return true;
    } catch(...) {return false;}
}
bool saveState(const std::filesystem::path& path,const State& state,bool backup) {
    try {
        const auto bytes=encode(state); auto temp=path; temp+=L".tmp";
        HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) return false;
        DWORD written=0; bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size();
        if(ok) ok=FlushFileBuffers(file)!=FALSE;
        CloseHandle(file);
        if(!ok) {DeleteFileW(temp.c_str()); return false;}
        if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES) {
            auto bak=path; bak+=L".bak";
            ok=ReplaceFileW(path.c_str(),temp.c_str(),backup ? bak.c_str() : nullptr,0,nullptr,nullptr)!=FALSE;
        } else ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE;
        if(!ok) DeleteFileW(temp.c_str());
        return ok;
    } catch(...) {return false;}
}
bool startupEnabled() {
    DWORD bytes=0;
    return RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",L"FencesT",RRF_RT_REG_SZ,nullptr,nullptr,&bytes)==ERROR_SUCCESS;
}
bool setStartup(bool enabled) {
    HKEY key=nullptr; if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS) return false;
    LSTATUS result;
    if(enabled) {std::wstring path(32768,0); DWORD n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size())); path.resize(n);
        path=L"\""+path+L"\" --background";
        result=RegSetValueExW(key,L"FencesT",0,REG_SZ,reinterpret_cast<const BYTE*>(path.c_str()),static_cast<DWORD>((path.size()+1)*sizeof(wchar_t)));
    } else {result=RegDeleteValueW(key,L"FencesT"); if(result==ERROR_FILE_NOT_FOUND) result=ERROR_SUCCESS;}
    RegCloseKey(key); return result==ERROR_SUCCESS;
}
void clampToMonitor(Group& group,float scale) {
    RECT r{group.x,group.y,group.x+static_cast<LONG>((group.width+inset*2)*scale),group.y+static_cast<LONG>((group.height+inset*2)*scale)};
    auto monitor=MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST); MONITORINFO info{}; info.cbSize=sizeof(info);
    if(GetMonitorInfoW(monitor,&info)) {
        group.x=std::clamp(group.x,static_cast<int>(info.rcWork.left),static_cast<int>(std::max(info.rcWork.left,info.rcWork.right-(r.right-r.left))));
        group.y=std::clamp(group.y,static_cast<int>(info.rcWork.top),static_cast<int>(std::max(info.rcWork.top,info.rcWork.bottom-80)));
    }
}
}
