#pragma once
#include "PoseStability.h"
#include <mutex>
#include <cstdint>

namespace dreamair {
struct Statistics {
    uint32_t deviceIndex=vr::k_unTrackedDeviceIndexInvalid;
    uint64_t samples=0,sourceInvalid=0,malformed=0,jumps=0,reacquiring=0,frameResets=0,gapResets=0,stale=0;
    bool enabled=true,sourceValid=false,outputValid=false,smoothPosition=false,smoothOrientation=false,forceTrackingRequested=false;
    int sourceResult=0;
    Decision lastDecision=Decision::Disabled;
    double lastArrival=0;
};
class TrackingManager {
public:
    void Configure(Settings settings,bool forceTracking=false,bool forceHeadsetOnly=false) {
        settings.Sanitize(); std::lock_guard<std::mutex> lock(mutex_);
        if(!(settings==settings_)) filter_.Reset();
        settings_=settings; forceTracking_=forceTracking; forceHeadsetOnly_=forceHeadsetOnly;
        stats_.enabled=settings.enabled; stats_.smoothPosition=settings.smoothPosition; stats_.smoothOrientation=settings.smoothOrientation;
        stats_.forceTrackingRequested=forceTracking || forceHeadsetOnly;
    }
    void Register(uint32_t index) {
        std::lock_guard<std::mutex> lock(mutex_); filter_.Reset();
        const bool enabled=stats_.enabled,position=stats_.smoothPosition,orientation=stats_.smoothOrientation,forceRequested=stats_.forceTrackingRequested;
        stats_={}; stats_.enabled=enabled; stats_.smoothPosition=position; stats_.smoothOrientation=orientation; stats_.deviceIndex=index;
        stats_.forceTrackingRequested=forceRequested;
    }
    void Unregister(uint32_t index) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stats_.deviceIndex==index) { stats_.deviceIndex=vr::k_unTrackedDeviceIndexInvalid; filter_.Reset(); }
    }
    bool RegisterActivated(uint32_t index,bool activationSucceeded,bool shimActive,bool isActive,bool desired,bool dreamAir) {
        if(activationSucceeded && shimActive && isActive && desired && dreamAir) { Register(index); return true; }
        return false;
    }
    void Process(uint32_t index,vr::DriverPose_t& pose,double arrival) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(index==stats_.deviceIndex && settings_.enabled) {
            stats_.samples++; stats_.sourceValid=pose.poseIsValid && pose.deviceIsConnected && pose.result==vr::TrackingResult_Running_OK;
            stats_.sourceResult=static_cast<int>(pose.result);
            stats_.lastArrival=arrival; stats_.lastDecision=filter_.Process(pose,arrival,settings_);
            stats_.outputValid=pose.poseIsValid && pose.deviceIsConnected && pose.result==vr::TrackingResult_Running_OK;
            switch(stats_.lastDecision) {
            case Decision::SourceInvalid: stats_.sourceInvalid++; break;
            case Decision::Malformed: stats_.malformed++; break;
            case Decision::JumpRejected: stats_.jumps++; break;
            case Decision::Reacquiring: stats_.reacquiring++; break;
            case Decision::FrameReset: stats_.frameResets++; break;
            case Decision::GapReset: stats_.gapResets++; break;
            case Decision::Stale: stats_.stale++; break;
            default: break;
            }
            return; // Supersedes this module's force flags; another module may also hook poses.
        }
        // Preserve upstream behavior for devices outside this guard, and when explicitly disabled.
        if(forceTracking_ || (forceHeadsetOnly_ && index==0)) {
            pose.poseIsValid=true;
            if(pose.result!=vr::TrackingResult_Fallback_RotationOnly) pose.result=vr::TrackingResult_Running_OK;
        }
    }
    Statistics Snapshot() { std::lock_guard<std::mutex> lock(mutex_); return stats_; }
private:
    std::mutex mutex_;
    PoseStability filter_;
    Settings settings_;
    Statistics stats_;
    bool forceTracking_=false,forceHeadsetOnly_=false;
};
inline TrackingManager trackingManager;
}
