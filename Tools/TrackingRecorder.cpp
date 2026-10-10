// OpenVR poses are runtime output after the installed driver, not raw optical/IMU samples.
#include "openvr.h"
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <cwchar>

static std::string Property(vr::IVRSystem* system,uint32_t index,vr::ETrackedDeviceProperty key) {
    vr::ETrackedPropertyError error=vr::TrackedProp_Success;
    char value[1024]{}; system->GetStringTrackedDeviceProperty(index,key,value,sizeof(value),&error);
    return error==vr::TrackedProp_Success ? value : "";
}
static std::string Csv(std::string value) {
    std::string out="\""; for(char c:value){ out+=c; if(c=='\"') out+=c; } return out+"\"";
}
struct Counts { uint64_t frames=0,invalid=0,nonfinite=0,steps=0; bool previousValid=false; vr::HmdMatrix34_t previous{}; };
int wmain(int argc,wchar_t** argv) {
    int seconds=30; std::filesystem::path output=L"poses.csv";
    for(int i=1;i<argc;i++) {
        if(std::wcscmp(argv[i],L"--seconds")==0 && i+1<argc) {
            wchar_t* end=nullptr; long parsed=std::wcstol(argv[++i],&end,10);
            if(!end || *end || parsed<1 || parsed>1800){ std::cerr<<"Seconds must be 1..1800\n"; return 2; } seconds=static_cast<int>(parsed);
        } else if(std::wcscmp(argv[i],L"--output")==0 && i+1<argc) output=argv[++i];
        else { std::cerr<<"Usage: TrackingRecorder --seconds 30 --output poses.csv\n"; return 2; }
    }
    try {
        if(!output.parent_path().empty()) std::filesystem::create_directories(output.parent_path());
        std::ofstream file(output); if(!file){ std::cerr<<"Cannot create CSV\n"; return 3; }
        vr::EVRInitError error=vr::VRInitError_None;
        auto system=vr::VR_Init(&error,vr::VRApplication_Background);
        if(error!=vr::VRInitError_None || !system){ std::cerr<<"OpenVR: "<<vr::VR_GetVRInitErrorAsEnglishDescription(error)<<"\nStart SteamVR first.\n"; return 4; }
        struct Shutdown { ~Shutdown(){ vr::VR_Shutdown(); } } shutdown;
        std::array<vr::TrackedDevicePose_t,vr::k_unMaxTrackedDeviceCount> poses{};
        std::array<Counts,vr::k_unMaxTrackedDeviceCount> counts{};
        std::array<std::string,vr::k_unMaxTrackedDeviceCount> serials{};
        std::array<bool,vr::k_unMaxTrackedDeviceCount> known{};
        file<<"elapsed_s,index,class,serial,connected,valid,result,finite,x_m,y_m,z_m,vx_m_s,vy_m_s,vz_m_s,wx_rad_s,wy_rad_s,wz_rad_s,step_m\n"<<std::setprecision(9);
        std::cout<<"Runtime output, zero prediction, RawAndUncalibrated origin. Large steps can be real movement; this is evidence, not a diagnosis.\n";
        const auto start=std::chrono::steady_clock::now();
        auto next=start;
        while(true) {
            const auto now=std::chrono::steady_clock::now();
            const double elapsed=std::chrono::duration<double>(now-start).count();
            if(elapsed>=seconds) break;
            system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseRawAndUncalibrated,0,poses.data(),static_cast<uint32_t>(poses.size()));
            for(uint32_t i=0;i<poses.size();i++) {
                const auto& p=poses[i]; auto& c=counts[i];
                if(!p.bDeviceIsConnected && !known[i]) continue;
                if(p.bDeviceIsConnected && !known[i]) {
                    known[i]=true; serials[i]=Property(system,i,vr::Prop_SerialNumber_String);
                    std::cout<<"Device "<<i<<" class "<<system->GetTrackedDeviceClass(i)<<" serial "<<serials[i]
                        <<" model "<<Property(system,i,vr::Prop_ModelNumber_String)<<" mode "<<Property(system,i,vr::Prop_ModeLabel_String)<<'\n';
                }
                bool finite=true;
                for(const auto& row:p.mDeviceToAbsoluteTracking.m) for(float x:row) finite=finite && std::isfinite(x);
                for(float x:p.vVelocity.v) finite=finite && std::isfinite(x);
                for(float x:p.vAngularVelocity.v) finite=finite && std::isfinite(x);
                const bool valid=p.bDeviceIsConnected && p.bPoseIsValid && p.eTrackingResult==vr::TrackingResult_Running_OK && finite;
                double step=0;
                if(valid && c.previousValid) step=std::hypot(static_cast<double>(p.mDeviceToAbsoluteTracking.m[0][3]-c.previous.m[0][3]),
                    static_cast<double>(p.mDeviceToAbsoluteTracking.m[1][3]-c.previous.m[1][3]),static_cast<double>(p.mDeviceToAbsoluteTracking.m[2][3]-c.previous.m[2][3]));
                c.frames++; if(!valid)c.invalid++; if(!finite)c.nonfinite++; if(step>0.1)c.steps++;
                c.previous=p.mDeviceToAbsoluteTracking; c.previousValid=valid;
                file<<elapsed<<','<<i<<','<<system->GetTrackedDeviceClass(i)<<','<<Csv(serials[i])<<','<<p.bDeviceIsConnected<<','<<p.bPoseIsValid<<','<<p.eTrackingResult<<','<<finite;
                for(int axis=0;axis<3;axis++) file<<','<<p.mDeviceToAbsoluteTracking.m[axis][3];
                for(float v:p.vVelocity.v) file<<','<<v;
                for(float v:p.vAngularVelocity.v) file<<','<<v;
                file<<','<<step<<'\n';
            }
            next+=std::chrono::milliseconds(5);
            if(next<now) next=now+std::chrono::milliseconds(5);
            std::this_thread::sleep_until(next);
        }
        for(uint32_t i=0;i<counts.size();i++) if(counts[i].frames) {
            const auto& c=counts[i]; std::cout<<"Summary device "<<i<<" samples="<<c.frames<<" non-OK="<<c.invalid<<" nonfinite="<<c.nonfinite<<" steps-over-10cm="<<c.steps<<'\n';
        }
        if(!file.good()){ std::cerr<<"CSV write failed\n"; return 5; }
        std::cout<<"CSV saved.\n"; return 0;
    } catch(const std::exception& e){ std::cerr<<e.what()<<'\n'; return 6; }
}
