#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace ft {
enum class Style : uint32_t { Glass, Clear, Matte };
struct Item { std::string path; std::string label; };
struct Group {
    uint32_t id = 1;
    std::string name = "New group";
    int x = 80, y = 80, width = 360, height = 300;
    Style style = Style::Glass;
    int opacity = 65, radius = 24, blur = 12, imageOpacity = 65;
    uint32_t tint = 0x172338;
    bool collapsed = false, locked = false, showLabels = true;
    int iconSize = 40;
    std::string image, folder;
    std::vector<Item> items;
};
struct State { uint32_t nextId = 1; bool snap = true; std::vector<Group> groups; };
constexpr size_t maxGroups = 48, maxItems = 512, maxStateBytes = 8 * 1024 * 1024;

inline void validate(Group& g) {
    if (g.name.empty()) g.name = "New group";
    if (g.name.size() > 512) throw std::runtime_error("Group name is too long");
    g.width = std::clamp(g.width, 260, 1600); g.height = std::clamp(g.height, 160, 1400);
    g.x = std::clamp(g.x, -100000, 100000); g.y = std::clamp(g.y, -100000, 100000);
    if (static_cast<uint32_t>(g.style) > 2) g.style = Style::Glass;
    g.opacity = std::clamp(g.opacity, 0, 100); g.radius = std::clamp(g.radius, 0, 64);
    g.blur = std::clamp(g.blur, 0, 30); g.imageOpacity = std::clamp(g.imageOpacity, 0, 100);
    g.iconSize = std::clamp(g.iconSize, 24, 72);
    g.tint &= 0xffffff;
    if (g.items.size() > maxItems) throw std::runtime_error("Too many shortcuts");
    for (auto& item : g.items) {
        if (item.path.empty() || item.path.size() > 32768 || item.label.size() > 2048)
            throw std::runtime_error("Invalid shortcut");
    }
}
inline bool addItem(Group& g, Item item) {
    if (item.path.empty() || g.items.size() >= maxItems) return false;
    if (std::any_of(g.items.begin(), g.items.end(), [&](const Item& v) { return v.path == item.path; })) return false;
    g.items.push_back(std::move(item)); return true;
}
// Explicit little-endian format. Never serialize compiler struct layout or pointers.
struct Writer {
    std::vector<uint8_t> data;
    void number(uint32_t v) { for(int i=0; i<4; ++i) data.push_back(static_cast<uint8_t>(v >> (8*i))); }
    void text(const std::string& v) {
        if (v.size()>32768) throw std::runtime_error("String too long");
        number(static_cast<uint32_t>(v.size())); data.insert(data.end(),v.begin(),v.end());
    }
};
struct Reader {
    const std::vector<uint8_t>& data; size_t pos=0;
    uint32_t number() {
        if (pos+4>data.size()) throw std::runtime_error("Truncated state");
        uint32_t v=0; for(int i=0;i<4;++i) v |= static_cast<uint32_t>(data[pos++])<<(8*i); return v;
    }
    std::string text() {
        size_t n=number(); if(n>32768 || n>data.size()-pos) throw std::runtime_error("Invalid string");
        std::string v(data.begin()+static_cast<ptrdiff_t>(pos),data.begin()+static_cast<ptrdiff_t>(pos+n)); pos+=n;
        if(v.find('\0')!=std::string::npos) throw std::runtime_error("Embedded null");
        return v;
    }
};
inline uint32_t checksum(const std::vector<uint8_t>& bytes, size_t end) {
    uint32_t v=2166136261u; for(size_t i=0;i<end;++i) { v^=bytes[i]; v*=16777619u; } return v;
}
inline std::vector<uint8_t> encode(const State& state) {
    if (state.groups.size()>maxGroups) throw std::runtime_error("Too many groups");
    Writer w; w.number(0x31535446); w.number(1); w.number(state.nextId); w.number(state.snap ? 1 : 0); w.number(static_cast<uint32_t>(state.groups.size()));
    for(auto g:state.groups) {
        validate(g); w.number(g.id); w.text(g.name); w.number(g.x); w.number(g.y); w.number(g.width); w.number(g.height);
        w.number(static_cast<uint32_t>(g.style)); w.number(g.opacity); w.number(g.radius); w.number(g.blur); w.number(g.tint);
        w.number(g.imageOpacity); w.number(g.collapsed ? 1 : 0); w.number(g.locked ? 1 : 0); w.number(g.showLabels ? 1 : 0);
        w.number(g.iconSize); w.text(g.image); w.text(g.folder);
        w.number(static_cast<uint32_t>(g.items.size())); for(const auto& i:g.items) {w.text(i.path); w.text(i.label);}
    }
    if(w.data.size()+4>maxStateBytes) throw std::runtime_error("State too large");
    w.number(checksum(w.data,w.data.size())); return w.data;
}
inline State decode(const std::vector<uint8_t>& data) {
    if(data.size()<20 || data.size()>maxStateBytes) throw std::runtime_error("Invalid state size");
    Reader r{data}; if(r.number()!=0x31535446 || r.number()!=1) throw std::runtime_error("Unsupported state format");
    State s; s.nextId=r.number(); s.snap=r.number()!=0; const auto count=r.number(); if(count>maxGroups) throw std::runtime_error("Too many groups");
    uint32_t highest=0;
    for(uint32_t n=0;n<count;++n) {
        Group g; g.id=r.number(); g.name=r.text(); g.x=static_cast<int32_t>(r.number()); g.y=static_cast<int32_t>(r.number());
        g.width=static_cast<int32_t>(r.number()); g.height=static_cast<int32_t>(r.number()); g.style=static_cast<Style>(r.number());
        g.opacity=static_cast<int32_t>(r.number()); g.radius=static_cast<int32_t>(r.number()); g.blur=static_cast<int32_t>(r.number());
        g.tint=r.number(); g.imageOpacity=static_cast<int32_t>(r.number()); g.collapsed=r.number()!=0; g.locked=r.number()!=0;
        g.showLabels=r.number()!=0; g.iconSize=static_cast<int32_t>(r.number()); g.image=r.text(); g.folder=r.text();
        auto items=r.number(); if(items>maxItems) throw std::runtime_error("Too many shortcuts");
        for(uint32_t k=0;k<items;++k) { Item i; i.path=r.text(); i.label=r.text(); g.items.push_back(std::move(i)); }
        if(g.id==0 || g.id==std::numeric_limits<uint32_t>::max() || std::any_of(s.groups.begin(),s.groups.end(),[&](const Group& v){return v.id==g.id;}))
            throw std::runtime_error("Invalid group ID");
        validate(g); highest=std::max(highest,g.id); s.groups.push_back(std::move(g));
    }
    if(r.pos+4!=data.size() || r.number()!=checksum(data,data.size()-4)) throw std::runtime_error("Damaged state");
    s.nextId=std::max(highest+1,s.nextId); if(s.nextId==0 || s.nextId==std::numeric_limits<uint32_t>::max()) s.nextId=highest+1;
    return s;
}
}
