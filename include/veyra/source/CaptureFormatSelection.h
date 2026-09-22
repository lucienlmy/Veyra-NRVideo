#pragma once
#include "veyra/source/CaptureFrameRate.h"
#include <format>
#include <span>

namespace veyra::source {
struct CaptureFormat {int index=0;unsigned width=0,height=0;double fps=0;std::wstring label;std::wstring key;int rank=0;int tier=0;};

inline const CaptureFormat* selectCaptureFormat(std::span<const CaptureFormat> formats,int legacyIndex,std::wstring_view key){
    for(const auto& format:formats)
        if(key.empty()?format.index==legacyIndex:format.key==key)return &format;
    return nullptr;
}

inline std::wstring capturePathOptions(double fps,std::wstring_view encodedKey){
    std::wstring result;
    if(fps>0)result=std::format(L"?fps={:.6f}",fps);
    if(!encodedKey.empty())result+=std::format(L"{}format={}",result.empty()?L"?":L"&",encodedKey);
    return result;
}

inline bool parseCapturePathOptions(std::wstring_view query,double& fps,std::wstring& encodedKey){
    fps=0;encodedKey.clear();bool haveFps=false,haveKey=false;
    if(query.empty())return false;
    while(!query.empty()){
        const auto end=query.find(L'&');const auto option=query.substr(0,end);
        if(option.starts_with(L"fps=")){
            if(haveFps||!parseCaptureFrameRate(option.substr(4),fps))return false;
            haveFps=true;
        }else if(option.starts_with(L"format=")){
            const auto key=option.substr(7);
            if(haveKey||key.empty()||key.size()>4096||key.size()%4||key.find_first_not_of(L"0123456789abcdefABCDEF")!=std::wstring_view::npos)return false;
            haveKey=true;encodedKey=key;
        }else return false;
        if(end==std::wstring_view::npos)break;
        query.remove_prefix(end+1);
        if(query.empty())return false;
    }
    return true;
}
}
