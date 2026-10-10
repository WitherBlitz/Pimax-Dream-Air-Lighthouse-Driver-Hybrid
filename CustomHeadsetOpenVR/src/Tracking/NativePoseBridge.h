#pragma once
#include <openvr_driver.h>
#include <mutex>
#include <cstdint>
#include <cmath>

namespace dreamair {
inline vr::DriverPose_t RebaseNativePose(vr::DriverPose_t pose,double queryArrival,double submissionArrival) {
    if(!std::isfinite(queryArrival)||!std::isfinite(submissionArrival)||submissionArrival<queryArrival){pose.poseIsValid=false;return pose;}
    pose.poseTimeOffset-=submissionArrival-queryArrival;
    return pose;
}
struct NativePoseStatistics {
    bool enabled=false,aligned=false,pimaxValid=false,lighthouseValid=false;
    uint32_t deviceIndex=vr::k_unTrackedDeviceIndexInvalid;
    uint64_t lighthouseSamples=0,lighthouseInvalid=0,pimaxSamples=0,pimaxInvalid=0;
    double lastLighthouseArrival=0,lastPimaxArrival=0;
};
class NativePoseBridge {
public:
    void Configure(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stats_.enabled!=enabled) { stats_.aligned=false; haveSource_=false; haveCandidate_=false; }
        stats_.enabled=enabled;
    }
    void Register(uint32_t index) {
        std::lock_guard<std::mutex> lock(mutex_); const bool enabled=stats_.enabled;
        stats_={};stats_.enabled=enabled;stats_.deviceIndex=index;haveSource_=false;haveCandidate_=false;lastOutput_={};
    }
    void Unregister(uint32_t index) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stats_.deviceIndex==index){stats_.deviceIndex=vr::k_unTrackedDeviceIndexInvalid;stats_.aligned=false;haveSource_=false;haveCandidate_=false;}
    }
    bool Observe(uint32_t index,const vr::DriverPose_t& input,double now) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(index!=stats_.deviceIndex || index==vr::k_unTrackedDeviceIndexInvalid)return true;
        source_=input;haveSource_=std::isfinite(now);stats_.lastLighthouseArrival=now;stats_.lighthouseSamples++;
        stats_.lighthouseValid=Valid(source_);if(!stats_.lighthouseValid)stats_.lighthouseInvalid++;
        return !stats_.enabled || !stats_.aligned;
    }
    bool Prepare(uint32_t index,const vr::DriverPose_t& input,double now,vr::DriverPose_t& output) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!stats_.enabled || index!=stats_.deviceIndex || index==vr::k_unTrackedDeviceIndexInvalid)return false;
        vr::DriverPose_t native=input;
        stats_.pimaxSamples++;stats_.lastPimaxArrival=now;stats_.pimaxValid=std::isfinite(now)&&Valid(native);
        if(!stats_.pimaxValid)stats_.pimaxInvalid++;
        if(!stats_.aligned) {
            if(!stats_.pimaxValid || !haveSource_ || !stats_.lighthouseValid || now<stats_.lastLighthouseArrival || now-stats_.lastLighthouseArrival>0.05){haveCandidate_=false;return false;}
            // Arrival order does not establish that both poses describe the same instant.
            const double sourceTime=stats_.lastLighthouseArrival+source_.poseTimeOffset;
            const double nativeTime=now+native.poseTimeOffset;
            if(!std::isfinite(sourceTime)||!std::isfinite(nativeTime)||std::abs(sourceTime-nativeTime)>0.02
                || !LowMotion(source_) || !LowMotion(native)){haveCandidate_=false;return false;}
            auto source=source_;Normalize(source.qRotation);Normalize(source.qWorldFromDriverRotation);Normalize(source.qDriverFromHeadRotation);Normalize(native.qRotation);
            const auto targetRotation=Multiply(Multiply(source.qWorldFromDriverRotation,source.qRotation),source.qDriverFromHeadRotation);
            auto candidateRotation=Multiply(targetRotation,Conjugate(native.qRotation));Normalize(candidateRotation);
            double headOffset[3],driverPosition[3],worldPosition[3],nativePosition[3];
            Rotate(source.qRotation,source.vecDriverFromHeadTranslation,headOffset);
            for(int i=0;i<3;i++)driverPosition[i]=source.vecPosition[i]+headOffset[i];
            Rotate(source.qWorldFromDriverRotation,driverPosition,worldPosition);
            Rotate(candidateRotation,native.vecPosition,nativePosition);
            double candidateTranslation[3],delta[3];
            for(int i=0;i<3;i++){candidateTranslation[i]=worldPosition[i]+source.vecWorldFromDriverTranslation[i]-nativePosition[i];delta[i]=candidateTranslation[i]-translation_[i];}
            const double dot=std::abs(candidateRotation.w*rotation_.w+candidateRotation.x*rotation_.x+candidateRotation.y*rotation_.y+candidateRotation.z*rotation_.z);
            if(!haveCandidate_ || now<candidateLast_ || now-candidateLast_>0.08 || Length(delta)>0.02 || dot<std::cos(0.01)){
                rotation_=candidateRotation;for(int i=0;i<3;i++)translation_[i]=candidateTranslation[i];
                candidateStart_=now;candidateSamples_=0;haveCandidate_=true;
            }
            candidateLast_=now;candidateSamples_++;
            if(now-candidateStart_<0.15-1e-9 || candidateSamples_<3)return false;
            stats_.aligned=true;
        }
        if(stats_.pimaxValid) {
            output=native;Normalize(output.qRotation);
            output.qWorldFromDriverRotation=rotation_;
            output.qDriverFromHeadRotation={1,0,0,0};
            for(int i=0;i<3;i++){output.vecWorldFromDriverTranslation[i]=translation_[i];output.vecDriverFromHeadTranslation[i]=0;}
            lastOutput_=output;
        } else {
            // Retain a finite reference on invalid input; validity is still false.
            // The user's separate forceTracking setting is applied downstream.
            output=lastOutput_;output.poseIsValid=false;output.deviceIsConnected=input.deviceIsConnected;
            output.result=vr::TrackingResult_Running_OutOfRange;output.poseTimeOffset=0;
            for(int i=0;i<3;i++)output.vecVelocity[i]=output.vecAcceleration[i]=output.vecAngularVelocity[i]=output.vecAngularAcceleration[i]=0;
        }
        return true;
    }
    NativePoseStatistics Snapshot() { std::lock_guard<std::mutex> lock(mutex_); return stats_; }
private:
    std::mutex mutex_;
    NativePoseStatistics stats_;
    vr::DriverPose_t source_{},lastOutput_{};
    bool haveSource_=false,haveCandidate_=false;
    double candidateStart_=0,candidateLast_=0;
    uint32_t candidateSamples_=0;
    vr::HmdQuaternion_t rotation_{1,0,0,0};
    double translation_[3]={};
    static bool Finite(const double* v){return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);}
    static double Length(const double* v){return std::hypot(std::hypot(v[0],v[1]),v[2]);}
    static bool LowMotion(const vr::DriverPose_t& p){return Length(p.vecVelocity)<=0.15&&Length(p.vecAngularVelocity)<=0.35;}
    static bool Normalize(vr::HmdQuaternion_t& q){double n=std::hypot(std::hypot(q.w,q.x),std::hypot(q.y,q.z));if(!std::isfinite(n)||n<1e-6)return false;q.w/=n;q.x/=n;q.y/=n;q.z/=n;return true;}
    static bool Valid(vr::DriverPose_t p){return p.poseIsValid&&p.deviceIsConnected&&p.result==vr::TrackingResult_Running_OK
        &&std::isfinite(p.poseTimeOffset)&&p.poseTimeOffset>=-0.25&&p.poseTimeOffset<=0.05
        &&Normalize(p.qRotation)&&Normalize(p.qWorldFromDriverRotation)&&Normalize(p.qDriverFromHeadRotation)
        &&Finite(p.vecPosition)&&Finite(p.vecWorldFromDriverTranslation)&&Finite(p.vecDriverFromHeadTranslation)
        &&Finite(p.vecVelocity)&&Finite(p.vecAcceleration)&&Finite(p.vecAngularVelocity)&&Finite(p.vecAngularAcceleration);}
    static vr::HmdQuaternion_t Conjugate(vr::HmdQuaternion_t q){return {q.w,-q.x,-q.y,-q.z};}
    static vr::HmdQuaternion_t Multiply(vr::HmdQuaternion_t a,vr::HmdQuaternion_t b){return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};}
    static void Rotate(vr::HmdQuaternion_t q,const double* v,double* out){auto r=Multiply(Multiply(q,{0,v[0],v[1],v[2]}),Conjugate(q));out[0]=r.x;out[1]=r.y;out[2]=r.z;}
};
inline NativePoseBridge nativePoseBridge;
}
