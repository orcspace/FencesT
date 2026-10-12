#pragma once
#include <array>
#include <cstdint>

namespace ft {
struct Hotkey {
    uint32_t modifiers=0, key=0;
    bool operator==(const Hotkey&) const = default;
};
using Hotkeys=std::array<Hotkey,2>;
inline constexpr Hotkeys defaultHotkeys{{{9,'F'},{9,'N'}}};
inline bool validHotkey(Hotkey h) {
    if(h.modifiers & ~15u) return false;
    if(h.key==0) return h.modifiers==0;
    bool function=h.key>=0x70 && h.key<=0x87 && h.key!=0x7b; // F12 is reserved by Windows.
    bool regular=(h.key>='A' && h.key<='Z') || (h.key>='0' && h.key<='9') || h.key==0x20 || h.key==0x09 ||
        (h.key>=0x21 && h.key<=0x28) || h.key==0x2d || h.key==0x2e;
    return function || (regular && h.modifiers!=0);
}
inline bool validHotkeys(const Hotkeys& h) {
    return validHotkey(h[0]) && validHotkey(h[1]) && (!h[0].key || h[0]!=h[1]);
}
struct HotkeyResult { bool success=false, restored=true; int failedAction=-1; };
// Release both old combinations first, so the two actions can swap bindings.
// On any conflict restore the complete previous configuration.
template<class Remove,class Register>
HotkeyResult replaceHotkeys(const Hotkeys& previous,const Hotkeys& next,Remove remove,Register reg) {
    if(!validHotkeys(next)) return {};
    for(int i=0;i<2;++i) remove(i);
    for(int i=0;i<2;++i) if(next[i].key && !reg(i,next[i])) {
        for(int n=0;n<2;++n) remove(n);
        bool restored=true;
        for(int n=0;n<2;++n) if(previous[n].key && !reg(n,previous[n])) restored=false;
        return {false,restored,i};
    }
    return {true,true,-1};
}
}
