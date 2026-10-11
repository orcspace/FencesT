#include "app.hpp"
#include <cmath>

using namespace Gdiplus;
namespace ft {
Color rgb(uint32_t color,BYTE alpha) {return Color(alpha,(color>>16)&255,(color>>8)&255,color&255);}
void roundPath(GraphicsPath& p,RectF r,float radius) {
    float d=std::min({radius*2,r.Width,r.Height});
    if(d<1) {p.AddRectangle(r); return;}
    p.AddArc(r.X,r.Y,d,d,180,90); p.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
    p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90); p.AddArc(r.X,r.GetBottom()-d,d,d,90,90); p.CloseFigure();
}
void fillRound(Graphics& g,RectF r,float radius,Color color) {GraphicsPath p; roundPath(p,r,radius); SolidBrush b(color); g.FillPath(&b,&p);}
void drawText(Graphics& g,const std::wstring& text,RectF rect,float size,Color color,bool bold,bool center) {
    const int style=bold ? FontStyleBold : FontStyleRegular;
    auto font=std::make_unique<Font>(L"Segoe UI",size,style,UnitPixel);
    if(font->GetLastStatus()!=Ok) font=std::make_unique<Font>(FontFamily::GenericSansSerif(),size,style,UnitPixel);
    SolidBrush brush(color);
    StringFormat format; format.SetTrimming(StringTrimmingEllipsisCharacter); format.SetFormatFlags(StringFormatFlagsNoWrap);
    format.SetLineAlignment(StringAlignmentCenter); if(center) format.SetAlignment(StringAlignmentCenter);
    g.DrawString(text.c_str(),static_cast<INT>(text.size()),font.get(),rect,&format,&brush);
}
std::unique_ptr<Bitmap> loadImage(const std::wstring& path) {
    std::unique_ptr<Bitmap> source(Bitmap::FromFile(path.c_str()));
    if(!source || source->GetLastStatus()!=Ok || source->GetWidth()==0 || source->GetHeight()==0 || source->GetWidth()>30000 || source->GetHeight()>30000) return {};
    float factor=std::min(1.0f,4096.0f/std::max(source->GetWidth(),source->GetHeight()));
    int w=std::max(1,static_cast<int>(source->GetWidth()*factor)), h=std::max(1,static_cast<int>(source->GetHeight()*factor));
    auto out=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB); Graphics g(out.get()); g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(source.get(),Rect(0,0,w,h)); return out;
}
void blurBitmap(Bitmap& b,int radius) {
    if(radius<1) return;
    int w=static_cast<int>(b.GetWidth()),h=static_cast<int>(b.GetHeight());
    Rect rect(0,0,w,h); BitmapData data{};
    if(b.LockBits(&rect,ImageLockModeRead|ImageLockModeWrite,PixelFormat32bppPARGB,&data)!=Ok) return;
    std::vector<BYTE> a(static_cast<size_t>(w)*h*4),temp(a.size());
    for(int y=0;y<h;++y) memcpy(a.data()+static_cast<size_t>(y)*w*4,static_cast<BYTE*>(data.Scan0)+y*data.Stride,w*4);
    int count=radius*2+1;
    for(int pass=0;pass<3;++pass) {
        for(int y=0;y<h;++y) for(int c=0;c<4;++c) {
            int sum=0; for(int dx=-radius;dx<=radius;++dx) sum+=a[(y*w+std::clamp(dx,0,w-1))*4+c];
            for(int x=0;x<w;++x) {temp[(y*w+x)*4+c]=static_cast<BYTE>(sum/count);
                sum-=a[(y*w+std::clamp(x-radius,0,w-1))*4+c]; sum+=a[(y*w+std::clamp(x+radius+1,0,w-1))*4+c];}
        }
        for(int x=0;x<w;++x) for(int c=0;c<4;++c) {
            int sum=0; for(int dy=-radius;dy<=radius;++dy) sum+=temp[(std::clamp(dy,0,h-1)*w+x)*4+c];
            for(int y=0;y<h;++y) {a[(y*w+x)*4+c]=static_cast<BYTE>(sum/count);
                sum-=temp[(std::clamp(y-radius,0,h-1)*w+x)*4+c]; sum+=temp[(std::clamp(y+radius+1,0,h-1)*w+x)*4+c];}
        }
    }
    for(int y=0;y<h;++y) memcpy(static_cast<BYTE*>(data.Scan0)+y*data.Stride,a.data()+static_cast<size_t>(y)*w*4,w*4);
    b.UnlockBits(&data);
}
static void imageCover(Graphics& g,Bitmap* image,RectF dest,float alpha=1) {
    if(!image) return;
    float iw=static_cast<float>(image->GetWidth()),ih=static_cast<float>(image->GetHeight());
    float factor=std::max(dest.Width/iw,dest.Height/ih), sw=dest.Width/factor,sh=dest.Height/factor;
    ColorMatrix matrix={{{1,0,0,0,0},{0,1,0,0,0},{0,0,1,0,0},{0,0,0,alpha,0},{0,0,0,0,1}}};
    ImageAttributes attributes; attributes.SetColorMatrix(&matrix);
    g.DrawImage(image,dest,(iw-sw)/2,(ih-sh)/2,sw,sh,UnitPixel,&attributes);
}
void App::reloadWallpaper() {
    wallpapers.clear(); IDesktopWallpaper* desktop=nullptr;
    if(SUCCEEDED(CoCreateInstance(CLSID_DesktopWallpaper,nullptr,CLSCTX_ALL,IID_PPV_ARGS(&desktop)))) {
        UINT count=0; desktop->GetMonitorDevicePathCount(&count); desktop->GetBackgroundColor(&wallpaperColor);
        DESKTOP_WALLPAPER_POSITION pos=DWPOS_FILL; desktop->GetPosition(&pos);
        for(UINT i=0;i<count;++i) {
            PWSTR monitor=nullptr,path=nullptr; Wallpaper wall;
            if(SUCCEEDED(desktop->GetMonitorDevicePathAt(i,&monitor))) {
                desktop->GetMonitorRECT(monitor,&wall.rect); desktop->GetWallpaper(monitor,&path);
                auto source=path ? loadImage(path) : nullptr;
                int w=std::max(1,static_cast<int>((wall.rect.right-wall.rect.left)/4)),h=std::max(1,static_cast<int>((wall.rect.bottom-wall.rect.top)/4));
                wall.image=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB); Graphics g(wall.image.get());
                g.Clear(Color(255,GetRValue(wallpaperColor),GetGValue(wallpaperColor),GetBValue(wallpaperColor)));
                g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                if(source) {
                    if(pos==DWPOS_FILL) imageCover(g,source.get(),RectF(0,0,static_cast<float>(w),static_cast<float>(h)));
                    else if(pos==DWPOS_FIT) {
                        float f=std::min(static_cast<float>(w)/source->GetWidth(),static_cast<float>(h)/source->GetHeight());
                        float dw=source->GetWidth()*f,dh=source->GetHeight()*f;
                        g.DrawImage(source.get(),RectF((w-dw)/2,(h-dh)/2,dw,dh));
                    } else if(pos==DWPOS_CENTER || pos==DWPOS_TILE) {
                        // Tiled/centered wallpapers use native image dimensions in desktop pixels.
                        float sw=source->GetWidth()/4.f,sh=source->GetHeight()/4.f;
                        if(pos==DWPOS_CENTER) g.DrawImage(source.get(),RectF((w-sw)/2,(h-sh)/2,sw,sh));
                        else for(float y=0;y<h;y+=sh) for(float x=0;x<w;x+=sw) g.DrawImage(source.get(),RectF(x,y,sw,sh));
                    } else if(pos==DWPOS_SPAN) {
                        int vx=GetSystemMetrics(SM_XVIRTUALSCREEN),vy=GetSystemMetrics(SM_YVIRTUALSCREEN);
                        int vw=GetSystemMetrics(SM_CXVIRTUALSCREEN),vh=GetSystemMetrics(SM_CYVIRTUALSCREEN);
                        imageCover(g,source.get(),RectF((vx-wall.rect.left)/4.f,(vy-wall.rect.top)/4.f,vw/4.f,vh/4.f));
                    } else g.DrawImage(source.get(),Rect(0,0,w,h));
                }
                wallpapers.push_back(std::move(wall));
            }
            CoTaskMemFree(path); CoTaskMemFree(monitor);
        }
        desktop->Release();
    }
    if(wallpapers.empty()) {
        wchar_t path[32768]{}; SystemParametersInfoW(SPI_GETDESKWALLPAPER,32768,path,0);
        Wallpaper wall; wall.rect={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),0,0};
        wall.rect.right=wall.rect.left+GetSystemMetrics(SM_CXVIRTUALSCREEN); wall.rect.bottom=wall.rect.top+GetSystemMetrics(SM_CYVIRTUALSCREEN);
        wall.image=loadImage(path); wallpapers.push_back(std::move(wall));
    }
    for(auto& panel:panels) {panel->invalidateBackdrop(); panel->render();}
}
void Panel::invalidateBackdrop() {backdrop.reset();}
void Panel::ensureBackdrop(int w,int h) {
    if(backdrop || group.style==Style::Clear) return;
    int bw=std::max(1,w/4),bh=std::max(1,h/4);
    backdrop=std::make_unique<Bitmap>(bw,bh,PixelFormat32bppPARGB);
    {
        Graphics g(backdrop.get()); g.Clear(Color(255,GetRValue(app.wallpaperColor),GetGValue(app.wallpaperColor),GetBValue(app.wallpaperColor)));
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        for(auto& wall:app.wallpapers) if(wall.image) {
            RectF dest((wall.rect.left-group.x)/4.f,(wall.rect.top-group.y)/4.f,(wall.rect.right-wall.rect.left)/4.f,(wall.rect.bottom-wall.rect.top)/4.f);
            g.DrawImage(wall.image.get(),dest);
        }
    }
    blurBitmap(*backdrop,group.blur/4);
}
void Panel::render() {
    if(!hwnd) return;
    const int logicalHeight=group.collapsed ? header : group.height;
    const int w=static_cast<int>((group.width+inset*2)*scale),h=static_cast<int>((logicalHeight+inset*2)*scale);
    if(w<1 || h<1) return;
    ensureBackdrop(w,h);
    Bitmap bitmap(w,h,PixelFormat32bppPARGB);
    {
        Graphics g(&bitmap); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic); g.Clear(Color(0,0,0,0)); g.ScaleTransform(scale,scale);
        RectF body(inset,inset,static_cast<float>(group.width),static_cast<float>(logicalHeight));
        float radius=std::min(static_cast<float>(group.radius),logicalHeight/2.f);
        for(int i=8;i>0;--i) fillRound(g,RectF(body.X-i/2.f,body.Y-i/3.f+2,body.Width+i,body.Height+i/1.5f),radius+i/2.f,Color(3,0,0,0));
        GraphicsPath clip; roundPath(clip,body,radius); auto state=g.Save(); g.SetClip(&clip);
        float opacity=group.opacity/100.f;
        // A nonzero alpha floor keeps empty transparent areas draggable/drop-capable.
        SolidBrush base(rgb(group.tint,static_cast<BYTE>(std::max(5.f,opacity*255)))); g.FillPath(&base,&clip);
        if(backdrop && group.style!=Style::Clear) {
            ColorMatrix matrix={{{1,0,0,0,0},{0,1,0,0,0},{0,0,1,0,0},{0,0,0,opacity*(group.style==Style::Glass ? .78f : .16f),0},{0,0,0,0,1}}};
            ImageAttributes attr; attr.SetColorMatrix(&matrix);
            g.DrawImage(backdrop.get(),RectF(0,0,w/scale,h/scale),0,0,static_cast<float>(backdrop->GetWidth()),static_cast<float>(backdrop->GetHeight()),UnitPixel,&attr);
        }
        if(background) imageCover(g,background.get(),body,group.imageOpacity/100.f*opacity);
        if(group.style==Style::Glass) {
            LinearGradientBrush shine(PointF(body.X,body.Y),PointF(body.GetRight(),body.GetBottom()),Color(static_cast<BYTE>(opacity*60),255,255,255),Color(static_cast<BYTE>(opacity*8),255,255,255));
            g.FillRectangle(&shine,body);
            LinearGradientBrush rim(PointF(0,body.Y),PointF(0,body.GetBottom()),Color(165,255,255,255),Color(24,255,255,255));
            Pen edge(&rim,1.3f); g.DrawPath(&edge,&clip);
        } else {Pen edge(Color(60,255,255,255),1.f); g.DrawPath(&edge,&clip);}
        if(dropHover) {SolidBrush hover(Color(60,169,154,255)); g.FillRectangle(&hover,body);}
        drawText(g,wide(group.name),RectF(inset+20,inset+9,group.width-133.f,38),14,Color(255,244,244,251),true);
        auto count=std::to_wstring(entries.size());
        drawText(g,count,RectF(inset+group.width-114.f,inset+12,28,30),11,Color(170,236,236,251),false,true);
        const float right=inset+group.width;
        Pen line(Color(210,244,244,255),1.5f);
        if(group.collapsed) {g.DrawLine(&line,right-70,27.f,right-65,32.f); g.DrawLine(&line,right-65,32.f,right-70,37.f);}
        else {g.DrawLine(&line,right-71,30.f,right-66,35.f); g.DrawLine(&line,right-66,35.f,right-61,30.f);}
        SolidBrush dots(Color(225,244,244,255)); for(int i=0;i<3;++i) g.FillEllipse(&dots,right-32+i*5,31.f,2.f,2.f);
        if(!group.collapsed) {
            Pen divider(Color(28,255,255,255),1.f); g.DrawLine(&divider,body.X+18,body.Y+header-3,body.GetRight()-18,body.Y+header-3);
            auto itemState=g.Save(); g.SetClip(RectF(body.X+8,body.Y+header,body.Width-16,body.Height-header-8),CombineModeIntersect);
            if(entries.empty()) {
                drawText(g,group.folder.empty() ? L"Перетащите сюда приложения" : L"Папка пуста или недоступна",RectF(body.X+20,body.Y+header+23,body.Width-40,26),13,Color(220,237,237,249),false,true);
                drawText(g,group.folder.empty() ? L"файлы или папки" : L"Обновить: F5",RectF(body.X+20,body.Y+header+50,body.Width-40,23),11,Color(150,230,230,244),false,true);
            }
            for(int i=0;i<static_cast<int>(entries.size());++i) {
                RectF r=itemRect(i); if(r.GetBottom()<body.Y+header || r.Y>body.GetBottom()-8) continue;
                if(i==hovered || i==selected) fillRound(g,RectF(r.X+3,r.Y+2,r.Width-6,r.Height-4),10,Color(i==selected ? 45 : 25,255,255,255));
                if(entries[i].icon) g.DrawImage(entries[i].icon.get(),RectF(r.X+(r.Width-group.iconSize)/2,r.Y+7,static_cast<float>(group.iconSize),static_cast<float>(group.iconSize)));
                else {fillRound(g,RectF(r.X+(r.Width-group.iconSize)/2,r.Y+7,static_cast<float>(group.iconSize),static_cast<float>(group.iconSize)),8,Color(220,163,151,229));
                    drawText(g,L"↗",RectF(r.X,r.Y+7,r.Width,static_cast<float>(group.iconSize)),22,Color::White,false,true);}
                if(group.showLabels) {
                    auto name=wide(entries[i].item.label); RectF textRect(r.X+2,r.Y+group.iconSize+12,r.Width-4,23);
                    drawText(g,name,RectF(textRect.X,textRect.Y+1,textRect.Width,textRect.Height),11,Color(130,0,0,0),false,true);
                    drawText(g,name,textRect,11,Color(255,246,246,255),false,true);
                }
            }
            g.Restore(itemState);
            int totalRows=(static_cast<int>(entries.size())+columns()-1)/columns();
            if(totalRows>visibleRows()) {
                float track=body.Height-header-20,thumb=std::max(20.f,track*visibleRows()/totalRows);
                float top=body.Y+header+10+(track-thumb)*scroll/std::max(1,totalRows-visibleRows());
                fillRound(g,RectF(body.GetRight()-7,top,3,thumb),1.5f,Color(95,255,255,255));
            }
            if(!group.locked) {Pen grip(Color(100,255,255,255),1.f); g.DrawLine(&grip,body.GetRight()-17,body.GetBottom()-8,body.GetRight()-8,body.GetBottom()-17);}
        }
        g.Restore(state);
    }
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=w; info.bmiHeader.biHeight=-h;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    HDC screen=GetDC(nullptr), dc=CreateCompatibleDC(screen); void* bits=nullptr;
    HBITMAP dib=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(dib && bits) {
        BitmapData data{}; Rect rect(0,0,w,h);
        if(bitmap.LockBits(&rect,ImageLockModeRead,PixelFormat32bppPARGB,&data)==Ok) {
            for(int y=0;y<h;++y) memcpy(static_cast<BYTE*>(bits)+static_cast<size_t>(y)*w*4,static_cast<BYTE*>(data.Scan0)+y*data.Stride,w*4);
            bitmap.UnlockBits(&data); auto old=SelectObject(dc,dib); POINT dst{group.x,group.y},src{0,0}; SIZE size{w,h}; BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            UpdateLayeredWindow(hwnd,screen,&dst,&size,dc,&src,0,&blend,ULW_ALPHA); SelectObject(dc,old);
        }
        DeleteObject(dib);
    }
    DeleteDC(dc); ReleaseDC(nullptr,screen);
}
}
