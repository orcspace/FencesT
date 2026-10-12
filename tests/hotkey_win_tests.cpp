#include <windows.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include "hotkeys.hpp"
using namespace ft;
static void require(bool ok,const char* message) {
    if(!ok) {std::cerr << message << '\n'; std::exit(1);}
}
int main() {
    const Hotkeys original{{{7,VK_F21},{7,VK_F22}}};
    const Hotkey occupied{7,VK_F23};
    HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr),stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    require(ready && stop,"Could not create test synchronization events");
    std::atomic<bool> reserved=false;
    std::thread other([&]{
        reserved=RegisterHotKey(nullptr,99,occupied.modifiers|MOD_NOREPEAT,occupied.key)!=FALSE;
        SetEvent(ready); WaitForSingleObject(stop,10000);
        if(reserved) UnregisterHotKey(nullptr,99);
    });
    require(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0 && reserved,"Could not reserve conflict test hotkey");
    auto remove=[](int i){UnregisterHotKey(nullptr,100+i);};
    auto reg=[](int i,Hotkey h){return RegisterHotKey(nullptr,100+i,h.modifiers|MOD_NOREPEAT,h.key)!=FALSE;};
    auto result=replaceHotkeys(Hotkeys{},original,remove,reg);
    require(result.success,"Initial Windows registration failed");
    const Hotkeys swapped{{original[1],original[0]}};
    result=replaceHotkeys(original,swapped,remove,reg);
    require(result.success,"Could not swap registered hotkeys");
    result=replaceHotkeys(swapped,Hotkeys{{{7,VK_F20},occupied}},remove,reg);
    require(!result.success && result.failedAction==1 && result.restored,"Conflict did not restore previous Windows registrations");
    // The temporary first binding must have been released after the second conflicted.
    require(RegisterHotKey(nullptr,110,7|MOD_NOREPEAT,VK_F20)!=FALSE,"Failed update leaked the first hotkey");
    UnregisterHotKey(nullptr,110);
    result=replaceHotkeys(swapped,Hotkeys{},remove,reg);
    require(result.success,"Could not disable global shortcuts");
    require(RegisterHotKey(nullptr,110,7|MOD_NOREPEAT,VK_F21)!=FALSE,"Disabling did not release hotkey");
    UnregisterHotKey(nullptr,110);
    SetEvent(stop); other.join(); CloseHandle(stop); CloseHandle(ready);
    std::cout << "Windows registration, swap, external conflict, rollback and disabling passed.\n";
}
