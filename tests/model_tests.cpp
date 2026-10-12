#include "model.hpp"
#include <iostream>
#include <cstdlib>
using namespace ft;
void require(bool value) { if(!value) {std::cerr << "Test failure\n"; std::exit(1);} }
bool rejects(const std::vector<uint8_t>& bytes) {try {decode(bytes); return false;} catch(const std::exception&) {return true;} }
int main() {
    State s; s.nextId=9; s.snap=false; Group g; g.id=8; g.name="Проекты • 日本語"; g.x=-1920; g.radius=48; g.opacity=0;
    g.showLabels=false; g.iconSize=64; g.folder="C:\\Папки\\Исходники";
    g.image="C:\\Фото\\обои.png"; g.collapsed=true; g.locked=true; g.style=Style::Clear;
    require(addItem(g,{"C:\\Проекты\\readme.txt","Документ"})); require(!addItem(g,{"C:\\Проекты\\readme.txt","Копия"}));
    s.groups.push_back(g); auto b=encode(s); auto restored=decode(b);
    require(encode(restored)==b); require(restored.groups[0].x==-1920); require(restored.groups[0].radius==48);
    require(restored.groups[0].items[0].label=="Документ");
    require(!restored.snap && !restored.groups[0].showLabels && restored.groups[0].iconSize==64 && restored.groups[0].folder==g.folder);
    for(size_t len=0;len<b.size();++len) require(rejects({b.begin(),b.begin()+static_cast<ptrdiff_t>(len)}));
    for(size_t i=0;i<b.size();++i) {auto corrupt=b; corrupt[i]^=0x80; require(rejects(corrupt));}
    auto tail=b; tail.push_back(0); require(rejects(tail));
    Group clamped; clamped.width=1; clamped.height=9000; clamped.radius=200; clamped.opacity=-40; validate(clamped);
    require(clamped.width==260 && clamped.height==1400 && clamped.radius==64 && clamped.opacity==0);
    s.groups.push_back(g); require(rejects(encode(s)));
    State empty; require(decode(encode(empty)).groups.empty());
    State custom; custom.groups.push_back(g); custom.hotkeys={{{3,'H'},{6,'J'}}};
    auto hotkeyBytes=encode(custom); require(decode(hotkeyBytes).hotkeys==custom.hotkeys);
    auto legacy=hotkeyBytes;
    legacy.erase(legacy.end()-20,legacy.end()); // Version 1 ended immediately after group records.
    legacy[4]=1; Writer trailer; trailer.number(checksum(legacy,legacy.size()));
    legacy.insert(legacy.end(),trailer.data.begin(),trailer.data.end());
    auto migrated=decode(legacy); require(migrated.hotkeys==defaultHotkeys && migrated.groups[0].name==g.name);
    require(migrated.groups[0].folder==g.folder && !migrated.groups[0].showLabels);
    require(!validHotkeys({{{3,'A'},{3,'A'}}})); require(!validHotkey({0,'A'}));
    require(!validHotkey({2,0x7b})); require(!validHotkey({16,'A'})); require(validHotkey({0,0x70}));
    require(validHotkeys({{{0,0},{0,0}}}));
    Hotkeys registered=defaultHotkeys;
    Hotkey external{3,'K'};
    auto remove=[&](int id){registered[id]={};};
    auto reg=[&](int id,Hotkey key){
        if(key==external || (key.key && key==registered[1-id])) return false;
        registered[id]=key; return true;
    };
    auto result=replaceHotkeys(defaultHotkeys,Hotkeys{{defaultHotkeys[1],defaultHotkeys[0]}},remove,reg);
    require(result.success && registered[0]==defaultHotkeys[1] && registered[1]==defaultHotkeys[0]);
    auto before=registered;
    result=replaceHotkeys(before,Hotkeys{{{3,'H'},external}},remove,reg);
    require(!result.success && result.restored && result.failedAction==1 && registered==before);
    result=replaceHotkeys(before,Hotkeys{{{3,'H'},{3,'H'}}},remove,reg);
    require(!result.success && registered==before);
    result=replaceHotkeys(before,Hotkeys{},remove,reg);
    require(result.success && registered==Hotkeys{});
    std::cout << "State round-trip, Unicode, corruption, bounds, duplicates and truncation checks passed.\n";
}
