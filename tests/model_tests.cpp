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
    std::cout << "State round-trip, Unicode, corruption, bounds, duplicates and truncation checks passed.\n";
}
