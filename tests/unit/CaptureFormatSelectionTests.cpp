#include "veyra/source/CaptureFormatSelection.h"
#include <array>
#include <iostream>

int main(){
    using namespace veyra::source;
    int failures=0;
    auto check=[&](bool ok,const char* name){if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}};
    std::array<CaptureFormat,3> formats{{{7,3840,2160,60,L"",L"4k60-p010"},{2,1920,1080,30,L"",L"1080p30-nv12"},{0,3840,2160,30,L"",L"4k30-p010"}}};
    check(selectCaptureFormat(formats,2,L"4k60-p010")==&formats[0],"stable key overrides stale index");
    check(!selectCaptureFormat(formats,2,L"missing"),"missing key never falls back to another format");
    check(selectCaptureFormat(formats,2,L"")==&formats[1],"legacy request retains index semantics");
    check(!selectCaptureFormat(formats,9,L""),"invalid legacy index rejected");
    double fps=0;std::wstring key;
    for(double requested:{0.,29.97,40.,60.}){
        const auto query=capturePathOptions(requested,L"0034004B");
        check(parseCapturePathOptions(std::wstring_view(query).substr(1),fps,key)&&fps==requested&&key==L"0034004B","options roundtrip");
    }
    check(parseCapturePathOptions(L"fps=50",fps,key)&&fps==50&&key.empty(),"old fps-only URI supported");
    check(parseCapturePathOptions(L"format=0041&fps=30",fps,key)&&fps==30,"option order independent");
    for(auto bad:{L"",L"format=",L"format=XYZW",L"format=000",L"fps=30&fps=60",L"format=0041&format=0042",L"fps=60&",L"unknown=1",L"fps=nan"})
        check(!parseCapturePathOptions(bad,fps,key),"malformed options rejected");
    check(captureFrameRateMatches(60,166833),"nominal 59.94 allowed");
    check(!captureFrameRateMatches(60,200000),"60 to 50 renegotiation rejected");
    check(!captureFrameRateMatches(60,0),"unknown negotiated rate rejected");
    std::cout<<"capture format selection failures="<<failures<<'\n';return failures?1:0;
}
