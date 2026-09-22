// Professional-inspector layout and scroll-commit checks (1.4.4 test packages).
//
// The inspector moves its child windows instead of using a native scroll bar,
// so two failure modes are easy to ship without noticing:
//
//   1. two controls placed on the same band (a row that "sticks" to the control
//      above it, or a control drawn on top of another one), and
//   2. a scroll that only repositions the child windows and leaves the previous
//      position's pixels on the screen until the next WM_PAINT arrives.
//
// The panel keeps single-instance file-scope state, so every case runs in its
// own process. Running this binary without an argument re-executes itself once
// per case and aggregates the result.
#include "../../apps/veyra/SettingsWindow.h"
#include "../../apps/veyra/ui/Theme.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures=0;
void check(bool ok,const std::string& what){std::cout<<(ok?"PASS ":"FAIL ")<<what<<'\n';if(!ok)++failures;}
void note(const std::string& what){std::cout<<"INFO "<<what<<'\n';}

void pump(){
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
        if(msg.message==WM_QUIT)continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
void dpiAware(){
    using SetContext=BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    if(auto set=reinterpret_cast<SetContext>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext"))){
        if(set(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))return;
    }
    SetProcessDPIAware();
}

struct Band{int id=0;int left=0;int top=0;int right=0;int bottom=0;};
std::string text(const Band& b){
    return "id="+std::to_string(b.id)+" band=["+std::to_string(b.left)+","+std::to_string(b.top)+" → "+
        std::to_string(b.right)+","+std::to_string(b.bottom)+"]";
}
bool overlaps(const Band& a,const Band& b){
    return a.left<b.right&&b.left<a.right&&a.top<b.bottom&&b.top<a.bottom;
}
int verticalGap(const Band& above,const Band& below){return below.top-above.bottom;}

// Page 1 ("frame generation / motion estimation") controls, in reading order.
// The capture/stream audio sync controls (1115/216/1116/217) belong to the audio
// page, and 1120 is the collapsible Smooth Motion help text.
const int kFramePageIds[]={1103,1111,208,1112,202,210,1113,209,215,204,1114,205,1110,
                           240,242,243,244,1147,1150,221,1120};
constexpr int kMinimumGap=6;      // dip between stacked controls
constexpr int kComfortableGap=8;  // dip required around the strict-cadence switch

Band bandOf(HWND body,HWND control){
    RECT r{};
    GetWindowRect(control,&r);
    POINT top{r.left,r.top};
    ScreenToClient(body,&top);
    const int dpi=int(veyra::ui::layoutDpi(body));
    Band band{};
    band.id=GetDlgCtrlID(control);
    band.left=MulDiv(top.x,96,dpi);
    band.top=MulDiv(top.y,96,dpi);
    band.right=MulDiv(top.x+(r.right-r.left),96,dpi);
    band.bottom=MulDiv(top.y+(r.bottom-r.top),96,dpi);
    return band;
}

int runLayoutCase(int dpi){
    veyra::ui::smokeLayoutDpi=UINT(dpi);
    veyra::engine::EngineController engine;
    veyra::engine::EnhancementSettings received{};
    bool delivered=false;
    HWND parent=CreateWindowExW(0,L"STATIC",L"Veyra settings layout probe",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_CLIPCHILDREN,-4000,-4000,
        MulDiv(420,dpi,96),MulDiv(920,dpi,96),nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    auto panel=veyra::ui::createSettingsPanel(parent,engine,[&](auto settings){
        // The engine bumps the revision when it applies settings; modelling that
        // here is what lets the panel accept the applied value back.
        received=settings;++received.revision;delivered=true;return true;
    });
    SetWindowPos(panel,nullptr,0,0,MulDiv(400,dpi,96),MulDiv(880,dpi,96),SWP_NOZORDER|SWP_SHOWWINDOW);
    veyra::ui::settingsPage(1);
    pump();
    HWND body=GetParent(veyra::ui::settingsControlForTest(202));
    check(body==GetParent(veyra::ui::settingsControlForTest(210)),"page-1 controls share the scrolling body");

    auto collect=[&]{
        std::vector<Band> out;
        for(int id:kFramePageIds){
            auto control=veyra::ui::settingsControlForTest(id);
            // Rows below the fold are hidden by the virtualization step, but
            // they are still positioned, so their geometry can be measured.
            if(!control){note("layout skip id="+std::to_string(id)+" (missing)");continue;}
            out.push_back(bandOf(body,control));
        }
        return out;
    };
    auto spacing=[&](const std::vector<Band>& measured,const std::string& stage){
        for(size_t i=0;i<measured.size();++i){
            for(size_t j=i+1;j<measured.size();++j){
                if(overlaps(measured[i],measured[j])){
                    check(false,stage+": controls must not overlap: "+text(measured[i])+" vs "+text(measured[j]));
                    continue;
                }
                const bool stacked=measured[i].left<measured[j].right&&measured[j].left<measured[i].right;
                if(!stacked)continue;
                const Band& above=measured[i].top<=measured[j].top?measured[i]:measured[j];
                const Band& below=above.id==measured[i].id?measured[j]:measured[i];
                const int gap=verticalGap(above,below);
                if(gap<kMinimumGap)
                    check(false,stage+": stacked controls sit "+std::to_string(gap)+" dip apart (minimum "+
                        std::to_string(kMinimumGap)+"): "+text(above)+" vs "+text(below));
            }
        }
    };

    const auto bands=collect();
    for(const auto& band:bands)note("layout dpi="+std::to_string(dpi)+" "+text(band));
    check(bands.size()==std::size(kFramePageIds),"every frame-page control was created");
    spacing(bands,"collapsed");

    // The Smooth Motion explainer unfolds between the frame-generation and
    // pacing rows. Expanding it used to be able to bury a control under another
    // one, so the same spacing rules apply to the unfolded page.
    SendMessageW(veyra::ui::settingsControlForTest(221),BM_CLICK,0,0);
    pump();
    const auto expanded=collect();
    const auto* help=[&]()->const Band*{
        for(const auto& band:expanded)if(band.id==1120)return &band;
        return nullptr;
    }();
    check(help&&help->bottom-help->top>0,"expanding the Smooth Motion explainer gives it a readable height");
    spacing(expanded,"help expanded");

    // The 1.4.4 report was about the switch glued to its neighbours, so hold
    // that row to the comfortable spacing against whatever rows it has.
    auto find=[&](int id)->const Band*{
        for(const auto& band:bands)if(band.id==id)return &band;
        return nullptr;
    };
    const auto* strict=find(210);
    check(strict!=nullptr,"the strict-cadence switch is present on the frame page");
    if(strict){
        int above=INT_MIN,below=INT_MAX;
        for(const auto& band:bands){
            if(band.id==strict->id)continue;
            if(band.left>=strict->right||strict->left>=band.right)continue;
            if(band.bottom<=strict->top)above=std::max(above,band.bottom);
            if(band.top>=strict->bottom)below=std::min(below,band.top);
        }
        check(strict->top-above>=kComfortableGap,
            "strict-cadence switch keeps "+std::to_string(kComfortableGap)+" dip above (measured "+
            std::to_string(strict->top-above)+")");
        check(below-strict->bottom>=kComfortableGap,
            "strict-cadence switch keeps "+std::to_string(kComfortableGap)+" dip below (measured "+
            std::to_string(below-strict->bottom)+")");
    }

    // The switch must still be the real control: clicking it submits the
    // strictness flag, and feeding the applied settings back (which is what the
    // player does on its next timer tick) must let it be turned off again.
    auto toggle=veyra::ui::settingsControlForTest(210);
    SendMessageW(toggle,BM_CLICK,0,0);
    check(delivered&&received.fgStrictAdmission,"clicking the switch still submits fgStrictAdmission=true");
    veyra::ui::settingsEnabled(false,received);
    delivered=false;
    SendMessageW(toggle,BM_CLICK,0,0);
    check(delivered&&!received.fgStrictAdmission,"clicking again submits fgStrictAdmission=false");

    DestroyWindow(parent);
    pump();
    veyra::ui::smokeLayoutDpi=0;
    std::cout<<"CASE dpi="<<dpi<<" failures="<<failures<<'\n';
    return failures;
}

struct Capture{
    int width=0,height=0;
    std::vector<uint32_t> pixels;
    bool valid=false;
};
bool grab(HWND window,Capture& out){
    RECT r{};
    if(!GetWindowRect(window,&r))return false;
    const int width=r.right-r.left,height=r.bottom-r.top;
    if(width<=0||height<=0)return false;
    HDC screen=GetDC(nullptr);
    if(!screen)return false;
    HDC memory=CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    HGDIOBJ previous=SelectObject(memory,bitmap);
    const BOOL copied=BitBlt(memory,0,0,width,height,screen,r.left,r.top,SRCCOPY);
    GdiFlush();
    out.width=width;
    out.height=height;
    out.pixels.assign(static_cast<uint32_t*>(bits),static_cast<uint32_t*>(bits)+size_t(width)*height);
    out.valid=copied!=FALSE;
    SelectObject(memory,previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr,screen);
    return out.valid;
}
size_t pixelDiff(const Capture& a,const Capture& b){
    if(a.width!=b.width||a.height!=b.height)return SIZE_MAX;
    size_t differing=0;
    for(size_t i=0;i<a.pixels.size();++i)
        if((a.pixels[i]&0xffffffu)!=(b.pixels[i]&0xffffffu))++differing;
    return differing;
}
double darkFraction(const Capture& capture){
    size_t dark=0;
    for(auto pixel:capture.pixels){
        const auto red=pixel&255u,green=(pixel>>8)&255u,blue=(pixel>>16)&255u;
        if(red<70&&green<70&&blue<70)++dark;
    }
    return capture.pixels.empty()?0.0:double(dark)/double(capture.pixels.size());
}

int runScrollCase(){
    veyra::ui::smokeLayoutDpi=0;
    dpiAware();
    const UINT dpi=GetDpiForSystem();
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY),&monitor);
    const int workWidth=monitor.rcWork.right-monitor.rcWork.left;
    const int workHeight=monitor.rcWork.bottom-monitor.rcWork.top;
    const int width=std::min(MulDiv(440,int(dpi),96),workWidth-40);
    const int height=std::min(MulDiv(920,int(dpi),96),workHeight-80);
    veyra::engine::EngineController engine;
    HWND parent=CreateWindowExW(WS_EX_TOPMOST,L"STATIC",L"Veyra settings scroll probe",
        WS_POPUP|WS_VISIBLE,monitor.rcWork.left+20,monitor.rcWork.top+20,width,height,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    auto panel=veyra::ui::createSettingsPanel(parent,engine,[](auto){return true;});
    SetWindowPos(panel,nullptr,0,0,width,height,SWP_NOZORDER|SWP_SHOWWINDOW);
    veyra::ui::settingsPage(1);
    SetWindowPos(parent,HWND_TOPMOST,monitor.rcWork.left+20,monitor.rcWork.top+20,width,height,SWP_SHOWWINDOW);
    SetForegroundWindow(parent);
    pump();
    Sleep(200);
    pump();
    HWND body=GetParent(veyra::ui::settingsControlForTest(202));
    Capture baseline{};
    grab(body,baseline);
    const double visible=darkFraction(baseline);
    note("scroll probe dpi="+std::to_string(dpi)+" body="+std::to_string(baseline.width)+"x"+
        std::to_string(baseline.height)+" dark="+std::to_string(visible));
    if(!baseline.valid||visible<0.5){
        note("scroll case skipped: the probe window is occluded or off screen "
             "(dark fraction "+std::to_string(visible)+"); rerun with the desktop visible");
        DestroyWindow(parent);
        return 0;
    }

    // One wheel notch worth of scrolling. The handler must leave the window in
    // its final state before it returns; a deferred repaint is exactly what the
    // user sees as ghosting/duplicated controls while scrolling.
    SendMessageW(panel,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA*3),0);
    Sleep(80);                       // let the compositor present, but do not
    Capture immediate{};             // pump: a queued WM_PAINT must not rescue it
    grab(body,immediate);
    pump();
    RedrawWindow(body,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    pump();
    Sleep(60);
    Capture settled{};
    grab(body,settled);
    const size_t stale=pixelDiff(immediate,settled);
    check(stale==0,"a wheel scroll shows the settled frame before returning ("+std::to_string(stale)+" stale pixels)");
    check(pixelDiff(baseline,settled)>0,"the wheel scroll moved the view");

    DestroyWindow(parent);
    pump();
    std::cout<<"CASE scroll failures="<<failures<<'\n';
    return failures;
}

int runSelf(const std::wstring& argument){
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr,path,MAX_PATH);
    std::wstring command=L"\""+std::wstring(path)+L"\" "+argument;
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)){
        std::wcout<<L"FAIL could not re-execute the layout probe for case "<<argument<<L"\n";
        return 1;
    }
    WaitForSingleObject(process.hProcess,INFINITE);
    DWORD code=0;
    GetExitCodeProcess(process.hProcess,&code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return int(code);
}
}

int wmain(int argc,wchar_t** argv){
    Gdiplus::GdiplusStartupInput graphicsInput;
    ULONG_PTR graphicsToken=0;
    Gdiplus::GdiplusStartup(&graphicsToken,&graphicsInput,nullptr);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};
    InitCommonControlsEx(&controls);
    int result=0;
    if(argc>1){
        const std::wstring mode=argv[1];
        if(mode==L"96")result=runLayoutCase(96);
        else if(mode==L"192")result=runLayoutCase(192);
        else if(mode==L"scroll")result=runScrollCase();
        else{std::cout<<"FAIL unknown case\n";result=1;}
    }else{
        for(const wchar_t* mode:{L"96",L"192",L"scroll"})result+=runSelf(mode);
        std::cout<<(result?"FAILED":"PASS")<<": settings page layout and scroll commit ("<<result<<" failures)\n";
    }
    Gdiplus::GdiplusShutdown(graphicsToken);
    return result;
}
