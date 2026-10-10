#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>
#include "FloorStateStore.h"
#include <array>
#include <iomanip>
#include <iostream>
#include <thread>
#include <cwchar>
#include <memory>

using namespace floorheight;
static bool SteamVrRunning() {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return false;
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool found=false;
    if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"vrserver.exe")==0){found=true;break;}}while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);return found;
}
static std::string Serial(vr::IVRSystem* system) {
    char serial[1024]{};vr::ETrackedPropertyError error=vr::TrackedProp_Success;
    system->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,vr::Prop_SerialNumber_String,serial,sizeof(serial),&error);
    if(error!=vr::TrackedProp_Success)throw std::runtime_error("Headset serial is unavailable.");return serial;
}
class OpenVrRuntime:public Runtime {
    vr::IVRSystem* system;vr::IVRChaperone* chaperone;vr::IVRChaperoneSetup* setup;
    Matrix staged{};bool hasStaged=false;
public:
    explicit OpenVrRuntime(vr::IVRSystem* s):system(s),chaperone(vr::VRChaperone()),setup(vr::VRChaperoneSetup()) {
        if(!chaperone || !setup)throw std::runtime_error("SteamVR chaperone interfaces are unavailable.");
    }
    bool Calibrated()override {return chaperone->GetCalibrationState()==vr::ChaperoneCalibrationState_OK;}
    Matrix ReadOrigin()override {return Inverse(system->GetRawZeroPoseToStandingAbsoluteTrackingPose());}
    Sample ReadHeadset()override {
        std::array<vr::TrackedDevicePose_t,vr::k_unMaxTrackedDeviceCount> poses{};
        system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseRawAndUncalibrated,0,poses.data(),static_cast<uint32_t>(poses.size()));
        const auto& pose=poses[vr::k_unTrackedDeviceIndex_Hmd];Sample s{};
        s.pose=pose.mDeviceToAbsoluteTracking;s.connected=pose.bDeviceIsConnected;s.valid=pose.bPoseIsValid;s.result=pose.eTrackingResult;
        for(int i=0;i<3;i++)s.velocity[i]=pose.vVelocity.v[i];s.serial=Serial(system);
        vr::ETrackedPropertyError error=vr::TrackedProp_Success;
        s.universe=system->GetUint64TrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,vr::Prop_CurrentUniverseId_Uint64,&error);
        if(error!=vr::TrackedProp_Success)throw std::runtime_error("Headset tracking universe is unavailable.");return s;
    }
    bool Stage(const Matrix& expected,const Matrix& replacement)override {
        if(!Calibrated() || !Same(ReadOrigin(),expected))return false;
        setup->RevertWorkingCopy();Matrix working{};
        if(!setup->GetWorkingStandingZeroPoseToRawTrackingPose(&working) || !Same(working,expected))return false;
        // No seated-origin, collision-bounds or play-area setters are called.
        setup->SetWorkingStandingZeroPoseToRawTrackingPose(&replacement);staged=replacement;hasStaged=true;return true;
    }
    bool Commit()override {
        if(!hasStaged || !Calibrated())return false;
        hasStaged=false;const bool committed=setup->CommitWorkingCopy(vr::EChaperoneConfigFile_Live);
        // The client transform can lag the server commit by a frame. Confirm propagation.
        for(int i=0;i<80;i++) {
            if(Same(ReadOrigin(),staged))return committed;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
    void Discard()override {hasStaged=false;if(Calibrated())setup->RevertWorkingCopy();}
    std::string ExportCalibration() {
        uint32_t length=0;setup->ExportLiveToBuffer(nullptr,&length);
        if(length==0 || length>16*1024*1024)throw std::runtime_error("Cannot export the existing SteamVR calibration.");
        std::vector<char> buffer(length+1,0);uint32_t capacity=length;
        if(!setup->ExportLiveToBuffer(buffer.data(),&capacity))throw std::runtime_error("Cannot export the existing SteamVR calibration.");
        return std::string(buffer.data());
    }
};
static void RequireForceTrackingOff() {
    wchar_t appdata[32768]{};DWORD count=GetEnvironmentVariableW(L"APPDATA",appdata,32768);
    if(!count || count>=32768)throw std::runtime_error("APPDATA is unavailable.");
    auto path=std::filesystem::path(appdata)/L"CustomHeadset"/L"settings.json";
    if(!std::filesystem::exists(path))return;
    std::ifstream input(path);if(!input)throw std::runtime_error("Cannot read driver settings.");const auto settings=Json::parse(input);
    for(const char* name:{"forceTracking","forceTrackingHeadsetOnly"})
        if(settings.contains(name) && settings.at(name).get<bool>())throw std::runtime_error("Disable both force-tracking options first (Profile-Guard.cmd). They can hide invalid headset tracking.");
}
static std::filesystem::path ExecutableDirectory() {
    std::vector<wchar_t> path(32768,0);DWORD length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    if(!length || length>=path.size())throw std::runtime_error("Cannot find tool directory.");return std::filesystem::path(path.data()).parent_path();
}
static void Usage() {
    std::cout<<"FloorHeight --show\nFloorHeight --feet 6 --inches 3 [--delay 5]\n"
        "FloorHeight --centimeters 180 | --meters 1.8\nFloorHeight --restore [--backup-id ID]\n"
        "Optional: --state-dir DIRECTORY. Heights: 0..10 metres. SteamVR must already be running.\n"
        "Changes the fixed standing floor; seated tracking is unchanged. Keep the headset still while applying.\n";
}
int wmain(int argc,wchar_t** argv) {
    Options options;
    try {std::vector<std::wstring> arguments;for(int i=1;i<argc;i++)arguments.push_back(argv[i]);options=Parse(arguments);}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';Usage();return 2;}
    if(options.action==Action::Help){Usage();return 0;}
    try {
        if(!SteamVrRunning())throw std::runtime_error("Start SteamVR first. This tool does not start or stop it.");
        if(options.action!=Action::Show)RequireForceTrackingOff();
        std::unique_ptr<MutationLock> mutationLock;
        if(options.action!=Action::Show)mutationLock=std::make_unique<MutationLock>();
        if(options.stateDirectory.empty())options.stateDirectory=ExecutableDirectory().parent_path()/L"FloorBackups";
        vr::EVRInitError error=vr::VRInitError_None;auto system=vr::VR_Init(&error,vr::VRApplication_Background);
        if(error!=vr::VRInitError_None || !system){std::cerr<<vr::VR_GetVRInitErrorAsEnglishDescription(error)<<'\n';return 4;}
        struct Shutdown {~Shutdown(){vr::VR_Shutdown();}} shutdown;
        OpenVrRuntime runtime(system);FileStore store(options.stateDirectory,[&]{return runtime.ExportCalibration();});
        if(options.action==Action::Show) {
            const auto head=runtime.ReadHeadset();
            if(!head.connected || !head.valid || head.result!=vr::TrackingResult_Running_OK)throw std::runtime_error("Headset has no valid position to measure.");
            double height=HeadHeight(runtime.ReadOrigin(),head.pose);double inches=height/.0254;
            std::cout<<std::fixed<<std::setprecision(4)<<"Current tracked headset height: "<<height<<" m ("<<height*100<<" cm, "<<inches<<" inches).\n"
                <<"HMD "<<head.serial<<"; universe "<<head.universe<<"; calibration ready: "<<runtime.Calibrated()<<". Read only; no floor changed.\n";
            return 0;
        }
        if(options.action==Action::Restore) {
            const auto backup=store.Load(options.backupId);Restore(runtime,store,backup);
            std::cout<<"Restored standing origin from backup "<<backup.id<<". Seated origin and room bounds were not edited.\n";return 0;
        }
        std::cout<<"Setting fixed standing floor "<<std::fixed<<std::setprecision(4)<<options.target<<" m below the headset.\n"
            <<"Hold the headset at the desired reference position and keep it still. Capture starts in "<<options.delay<<" seconds.\n"<<std::flush;
        for(int second=0;second<options.delay;second++)std::this_thread::sleep_for(std::chrono::seconds(1));
        if(!runtime.Calibrated())throw std::runtime_error("SteamVR room calibration is not ready. Finish room setup and restore reliable tracking first.");
        const auto origin=runtime.ReadOrigin();std::vector<Sample> captured;
        for(int i=0;i<20;i++){captured.push_back(runtime.ReadHeadset());std::this_thread::sleep_for(std::chrono::milliseconds(20));}
        RequireForceTrackingOff();Apply(runtime,store,origin,captured,options.target);
        const auto backup=store.Load();
        std::cout<<"Floor applied. Nominal sampled headset height: "<<options.target<<" m. The floor now stays fixed as you move.\nBackup "<<backup.id<<" saved in ";std::wcout<<options.stateDirectory.wstring()<<L"\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 3;}
}
