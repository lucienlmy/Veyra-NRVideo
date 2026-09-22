#pragma once
#include "veyra/source/ElgatoHdrControl.h"
#include <cstring>

namespace {
struct ElgatoProperties final:IKsPropertySet {
    std::array<unsigned char,32> packet{};
    DWORD mode=1;unsigned gets=0,sets=0;
    bool shortPacket=false,readableMode=true,rejectSet=false,ignoreSet=false,queryFails=false;
    void hdr(unsigned eotf=2){
        packet={};packet[0]=0x87;packet[1]=1;packet[2]=26;packet[4]=static_cast<unsigned char>(eotf);
        packet[22]=0xe8;packet[23]=3;packet[26]=0x90;packet[27]=1;packet[28]=100;
        checksum();
    }
    void checksum(){packet[3]=0;unsigned sum=0;for(unsigned i=0;i<30;++i)sum+=packet[i];packet[3]=static_cast<unsigned char>(0-sum);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{
        if(!out)return E_POINTER;*out=nullptr;
        if(id!=IID_IUnknown&&id!=__uuidof(IKsPropertySet))return E_NOINTERFACE;
        *out=static_cast<IKsPropertySet*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return 2;}
    ULONG STDMETHODCALLTYPE Release()override{return 1;}
    HRESULT STDMETHODCALLTYPE Set(REFGUID id,DWORD prop,LPVOID instance,DWORD instanceSize,LPVOID data,DWORD size)override{
        ++sets;if(id.Data1!=0xD1E5209F||prop!=722||instance||instanceSize||size!=4)return E_INVALIDARG;
        if(rejectSet)return E_FAIL;if(!ignoreSet)std::memcpy(&mode,data,4);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Get(REFGUID id,DWORD prop,LPVOID instance,DWORD instanceSize,LPVOID data,DWORD size,DWORD* returned)override{
        ++gets;*returned=0;if(id.Data1!=0xD1E5209F||instance||instanceSize)return E_INVALIDARG;
        if((prop==720||prop==721)&&size==16){std::memcpy(data,packet.data()+(prop==720?0:16),16);*returned=shortPacket?15:16;return S_OK;}
        if(prop==722&&size==4&&readableMode){std::memcpy(data,&mode,4);*returned=4;return S_OK;}
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID,DWORD,DWORD* flags)override{
        *flags=KSPROPERTY_SUPPORT_GET|KSPROPERTY_SUPPORT_SET;return queryFails?E_NOTIMPL:S_OK;
    }
};
template<class Check> void elgatoHdrCases(Check check) {
    using namespace veyra;
    constexpr auto name=L"Game Capture 4K60 Pro MK.2";
    const auto defaults=[] {pipeline::ColorDescription c;c.matrixAssumed=c.primariesAssumed=c.transferAssumed=true;c.transfer=pipeline::TransferFunction::SRGB;c.range=pipeline::ColorRange::Limited;return c;};
    check(source::isElgatoMk2(name)&&source::isElgatoMk2(L"elgato game capture 4k60 pro mk.2")&&!source::isElgatoMk2(L"Elgato 4K Pro"),"MK.2 exact device aliases only");
    ElgatoProperties ps;ps.hdr();
    auto parsed=source::parseElgatoHdrPacket(ps.packet);
    check(parsed.valid&&parsed.eotf==2&&parsed.peak==1000&&parsed.maxCll==400&&parsed.maxFall==100,"HDR InfoFrame PQ and little endian mastering metadata");
    auto broken=ps.packet;broken[20]^=1;
    check(!source::parseElgatoHdrPacket(broken).valid,"reject corrupt InfoFrame checksum");
    broken=ps.packet;broken[2]=255;check(!source::parseElgatoHdrPacket(broken).valid,"reject unbounded packet length");
    ps.packet[0]=0x82;ps.checksum();check(!source::parseElgatoHdrPacket(ps.packet).valid,"reject other InfoFrame type");
    ps.hdr(1);check(!source::parseElgatoHdrPacket(ps.packet).valid,"unknown HDR gamma is not PQ");
    ps.hdr();auto color=defaults();
    {
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,true,0,color)&&ps.mode==0&&color.transfer==pipeline::TransferFunction::PQ&&color.range==pipeline::ColorRange::Limited&&color.hdrMaxCllNits==400,"auto HDR disables card tonemap and propagates metadata without changing range");
    }
    check(ps.mode==1&&ps.sets==2,"close restores readable initial hardware state");
    ps={};ps.hdr();ps.mode=0;color=defaults();
    color.range=pipeline::ColorRange::Full;color.matrix=pipeline::YuvMatrix::BT709;color.matrixAssumed=false;
    color.primaries=pipeline::ColorPrimaries::BT709;color.primariesAssumed=false;
    {
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,true,0,color)&&ps.sets==0&&color.range==pipeline::ColorRange::Full&&color.matrix==pipeline::YuvMatrix::BT709&&color.primaries==pipeline::ColorPrimaries::BT709&&!color.matrixAssumed&&!color.primariesAssumed,"vendor EOTF never overwrites explicit driver gamut/range or re-writes native mode");
    }
    ps={};color=defaults();color.transfer=pipeline::TransferFunction::PQ;color.transferAssumed=false;
    {
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,true,0,color)&&ps.mode==0,"driver-declared HDR also disables hardware tonemap without vendor HDR packet");
    }
    ps={};ps.hdr(3);color=defaults();{
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,true,8,color)&&color.transfer==pipeline::TransferFunction::HLG,"range-only override still allows auto HLG detection");
    }
    ps={};ps.hdr();color=defaults();{
        source::ElgatoHdrControl control;
        check(control.configure(&ps,L"Other Capture",true,1,color)&&ps.gets==0&&ps.sets==0,"other devices receive zero vendor calls");
    }
    {
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,false,0,color)&&ps.gets==0,"non-P010 receives zero vendor calls");
    }
    ps={};color=defaults();{
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,true,0,color)&&ps.sets==0&&color.transfer==pipeline::TransferFunction::SRGB,"zero SDR packet never enables HDR");
    }
    ps={};ps.hdr();ps.shortPacket=true;color=defaults();{
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,true,0,color)&&ps.sets==0&&color.transferAssumed,"short vendor reads never establish HDR");
    }
    ps={};ps.hdr();ps.rejectSet=true;color=defaults();{
        source::ElgatoHdrControl control;
        check(!control.configure(&ps,name,true,0,color)&&color.transferAssumed,"failed hardware change cannot relabel SDR output PQ");
    }
    ps={};ps.hdr();ps.ignoreSet=true;color=defaults();{
        source::ElgatoHdrControl control;
        check(!control.configure(&ps,name,true,1,color),"reject successful Set with contradictory readback");
    }
    ps={};ps.hdr();ps.mode=0;color=defaults();{
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,true,3,color)&&ps.mode==1&&!color.isHdrPath(),"manual Rec709 requests hardware SDR instead of relabeling raw PQ");
    }
    check(ps.mode==0,"manual SDR close restores original native mode");
    ps={};ps.queryFails=true;ps.readableMode=false;color=defaults();{
        source::ElgatoHdrControl control;
        check(control.configure(&ps,name,true,1,color)&&ps.mode==0,"explicit PQ works with write-only property and broken QuerySupported");
    }
    check(ps.sets==1,"unknown original state is never fabricated on restore");
}
}
