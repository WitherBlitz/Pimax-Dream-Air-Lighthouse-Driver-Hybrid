#pragma once
#include "openvr_driver.h"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace dreamair {
struct Settings {
    bool enabled = true;
    bool smoothPosition = false;
    bool smoothOrientation = false;
    double maxLinearSpeed = 8;
    double maxAngularSpeed = 35;
    double positionSlack = 0.035;
    double angularSlack = 0.13962634015954636;
    double maxReportedLinearSpeed = 12;
    double maxReportedAngularSpeed = 50;
    double maxPoseTimeOffset = 0.25;
    double resetGapSeconds = 0.1;
    int reacquireSamples = 3;
    double positionMinCutoff = 20;
    double positionBeta = 80;
    double orientationMinCutoff = 30;
    double orientationBeta = 6;
    static double Bound(double v, double fallback, double lo, double hi) {
        return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
    }
    void Sanitize() {
        maxLinearSpeed=Bound(maxLinearSpeed,8,1,30);
        maxAngularSpeed=Bound(maxAngularSpeed,35,5,100);
        positionSlack=Bound(positionSlack,0.035,0.001,0.25);
        angularSlack=Bound(angularSlack,0.13962634015954636,0.001,0.5);
        maxReportedLinearSpeed=Bound(maxReportedLinearSpeed,12,1,50);
        maxReportedAngularSpeed=Bound(maxReportedAngularSpeed,50,5,150);
        maxPoseTimeOffset=Bound(maxPoseTimeOffset,0.25,0.01,1);
        resetGapSeconds=Bound(resetGapSeconds,0.1,0.02,0.5);
        reacquireSamples=std::clamp(reacquireSamples,2,20);
        positionMinCutoff=Bound(positionMinCutoff,20,1,240);
        positionBeta=Bound(positionBeta,80,0,500);
        orientationMinCutoff=Bound(orientationMinCutoff,30,1,240);
        orientationBeta=Bound(orientationBeta,6,0,100);
    }
    bool operator==(const Settings& s) const {
        return std::tie(enabled,smoothPosition,smoothOrientation,maxLinearSpeed,maxAngularSpeed,
            positionSlack,angularSlack,maxReportedLinearSpeed,maxReportedAngularSpeed,maxPoseTimeOffset,
            resetGapSeconds,reacquireSamples,positionMinCutoff,positionBeta,orientationMinCutoff,orientationBeta)
            ==std::tie(s.enabled,s.smoothPosition,s.smoothOrientation,s.maxLinearSpeed,s.maxAngularSpeed,
            s.positionSlack,s.angularSlack,s.maxReportedLinearSpeed,s.maxReportedAngularSpeed,s.maxPoseTimeOffset,
            s.resetGapSeconds,s.reacquireSamples,s.positionMinCutoff,s.positionBeta,s.orientationMinCutoff,s.orientationBeta);
    }
};
enum class Decision { Disabled, Accepted, SourceInvalid, Malformed, JumpRejected, Reacquiring, FrameReset, GapReset, Stale };
inline const char* DecisionName(Decision d) {
    switch(d) {
    case Decision::Disabled: return "disabled";
    case Decision::Accepted: return "accepted";
    case Decision::SourceInvalid: return "source-invalid";
    case Decision::Malformed: return "malformed";
    case Decision::JumpRejected: return "jump-rejected";
    case Decision::Reacquiring: return "reacquiring";
    case Decision::FrameReset: return "frame-reset";
    case Decision::GapReset: return "gap-reset";
    case Decision::Stale: return "stale-timestamp";
    default: return "unknown";
    }
}
class PoseStability {
public:
    void Reset() { *this=PoseStability{}; }
    Decision Process(vr::DriverPose_t& pose, double arrival, const Settings& config) {
        Settings s=config; s.Sanitize();
        if(!s.enabled) { Reset(); return Decision::Disabled; }
        if(!std::isfinite(arrival) || !AllFinite(pose) || std::abs(pose.poseTimeOffset)>s.maxPoseTimeOffset) {
            BreakHistory(); return Reject(pose,Decision::Malformed);
        }
        const double time=arrival+pose.poseTimeOffset;
        if(!std::isfinite(time)) { BreakHistory(); return Reject(pose,Decision::Malformed); }
        // Preserve the source's loss/disconnection/rotation-only contract exactly.
        if(!pose.poseIsValid || !pose.deviceIsConnected || pose.result!=vr::TrackingResult_Running_OK) {
            BreakHistory(); lastSeen_=time; seen_=true; return Decision::SourceInvalid;
        }
        if(!Normalize(pose.qRotation) || !Normalize(pose.qWorldFromDriverRotation) || !Normalize(pose.qDriverFromHeadRotation)
            || Norm(pose.vecVelocity)>s.maxReportedLinearSpeed || Norm(pose.vecAngularVelocity)>s.maxReportedAngularSpeed) {
            BreakHistory(); return Reject(pose,Decision::Malformed);
        }
        if(seen_ && time<=lastSeen_+1e-9) {
            if(std::abs(time-lastSeen_)<=1e-9 && good_ && !recovering_ && SamePose(pose,lastRaw_)) {
                const bool connected=pose.deviceIsConnected; pose=lastOutput_; pose.deviceIsConnected=connected;
                return Decision::Accepted;
            }
            if(time<lastSeen_-1e-9) { BreakHistory(); seen_=false; }
            return Reject(pose,Decision::Stale);
        }
        const bool gap=seen_ && time-lastSeen_>s.resetGapSeconds;
        lastSeen_=time; seen_=true;
        // Driver/world or head transforms indicate a new coordinate frame, not physical speed.
        const bool frameChanged=good_ && FrameChanged(pose,lastRaw_);
        if(frameChanged && !recovering_ && !gap) { AcceptFresh(pose,time); return Decision::FrameReset; }
        if(gap) {
            BreakHistory(); Candidate(pose,time,s); return Reject(pose,Decision::GapReset);
        }
        if(!good_ && !recovering_) { AcceptFresh(pose,time); return Decision::Accepted; }
        if(recovering_) {
            if(returnAllowed_ && !frameChanged && Plausible(pose,lastRaw_,time-lastAccepted_,s)) {
                AcceptFresh(pose,time); return Decision::Accepted;
            }
            if(Candidate(pose,time,s)) { AcceptFresh(pose,time); return Decision::Accepted; }
            return Reject(pose,Decision::Reacquiring);
        }
        if(!Plausible(pose,lastRaw_,time-lastAccepted_,s)) {
            recovering_=true; returnAllowed_=true; candidateCount_=0;
            Candidate(pose,time,s); return Reject(pose,Decision::JumpRejected);
        }
        const vr::DriverPose_t raw=pose;
        const double dt=time-lastAccepted_;
        if(s.smoothPosition) {
            const double speed=Distance(raw.vecPosition,lastRaw_.vecPosition)/dt;
            const double a=Alpha(s.positionMinCutoff+s.positionBeta*speed,dt);
            for(int i=0;i<3;i++) {
                pose.vecPosition[i]=Blend(lastOutput_.vecPosition[i],raw.vecPosition[i],a);
                pose.vecVelocity[i]=Blend(lastOutput_.vecVelocity[i],raw.vecVelocity[i],a);
                pose.vecAcceleration[i]=Blend(lastOutput_.vecAcceleration[i],raw.vecAcceleration[i],a);
            }
        }
        if(s.smoothOrientation) {
            const double speed=Angle(raw.qRotation,lastRaw_.qRotation)/dt;
            const double a=Alpha(s.orientationMinCutoff+s.orientationBeta*speed,dt);
            pose.qRotation=Slerp(lastOutput_.qRotation,raw.qRotation,a);
            for(int i=0;i<3;i++) {
                pose.vecAngularVelocity[i]=Blend(lastOutput_.vecAngularVelocity[i],raw.vecAngularVelocity[i],a);
                pose.vecAngularAcceleration[i]=Blend(lastOutput_.vecAngularAcceleration[i],raw.vecAngularAcceleration[i],a);
            }
        }
        lastRaw_=raw; lastOutput_=pose; lastAccepted_=time;
        return Decision::Accepted;
    }
private:
    vr::DriverPose_t lastRaw_{},lastOutput_{},candidate_{};
    double lastAccepted_=0,lastSeen_=0,candidateTime_=0;
    bool good_=false,seen_=false,recovering_=false,returnAllowed_=false;
    int candidateCount_=0;
    static double Norm(const double* v) { return std::hypot(v[0],v[1],v[2]); }
    static double Distance(const double* a,const double* b) { return std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]); }
    static bool FiniteVector(const double* v) {
        return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
    }
    static bool FiniteQuaternion(vr::HmdQuaternion_t q) {
        return std::isfinite(q.w) && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z);
    }
    static bool AllFinite(const vr::DriverPose_t& p) {
        return std::isfinite(p.poseTimeOffset) && FiniteQuaternion(p.qRotation)
            && FiniteQuaternion(p.qWorldFromDriverRotation) && FiniteQuaternion(p.qDriverFromHeadRotation)
            && FiniteVector(p.vecWorldFromDriverTranslation) && FiniteVector(p.vecDriverFromHeadTranslation)
            && FiniteVector(p.vecPosition) && FiniteVector(p.vecVelocity) && FiniteVector(p.vecAcceleration)
            && FiniteVector(p.vecAngularVelocity) && FiniteVector(p.vecAngularAcceleration);
    }
    static bool Normalize(vr::HmdQuaternion_t& q) {
        const double n=std::hypot(std::hypot(q.w,q.x),std::hypot(q.y,q.z));
        if(!std::isfinite(n) || n<1e-6) return false;
        q.w/=n; q.x/=n; q.y/=n; q.z/=n; return true;
    }
    static double Dot(vr::HmdQuaternion_t a,vr::HmdQuaternion_t b) { return a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z; }
    static double Angle(vr::HmdQuaternion_t a,vr::HmdQuaternion_t b) { return 2*std::acos(std::clamp(std::abs(Dot(a,b)),0.0,1.0)); }
    static bool SamePose(const vr::DriverPose_t& a,const vr::DriverPose_t& b) {
        return Distance(a.vecPosition,b.vecPosition)<1e-9 && Angle(a.qRotation,b.qRotation)<1e-7 && !FrameChanged(a,b);
    }
    static bool FrameChanged(const vr::DriverPose_t& a,const vr::DriverPose_t& b) {
        return Distance(a.vecWorldFromDriverTranslation,b.vecWorldFromDriverTranslation)>1e-4
            || Distance(a.vecDriverFromHeadTranslation,b.vecDriverFromHeadTranslation)>1e-4
            || Angle(a.qWorldFromDriverRotation,b.qWorldFromDriverRotation)>1e-4
            || Angle(a.qDriverFromHeadRotation,b.qDriverFromHeadRotation)>1e-4;
    }
    static bool Plausible(const vr::DriverPose_t& a,const vr::DriverPose_t& b,double dt,const Settings& s) {
        return dt>1e-9 && dt<=s.resetGapSeconds
            && Distance(a.vecPosition,b.vecPosition)<=s.positionSlack+s.maxLinearSpeed*dt
            && Angle(a.qRotation,b.qRotation)<=s.angularSlack+s.maxAngularSpeed*dt;
    }
    static double Alpha(double cutoff,double dt) {
        cutoff=std::clamp(cutoff,1.0,240.0);
        return 1/(1+1/(6.28318530717958647692*cutoff*dt));
    }
    static double Blend(double a,double b,double alpha) { return a+(b-a)*alpha; }
    static vr::HmdQuaternion_t Slerp(vr::HmdQuaternion_t a,vr::HmdQuaternion_t b,double t) {
        double dot=Dot(a,b);
        if(dot<0) { b={-b.w,-b.x,-b.y,-b.z}; dot=-dot; }
        dot=std::clamp(dot,0.0,1.0);
        double wa=1-t,wb=t;
        if(dot<0.9995) { const double angle=std::acos(dot),denom=std::sin(angle); wa=std::sin((1-t)*angle)/denom; wb=std::sin(t*angle)/denom; }
        vr::HmdQuaternion_t q={a.w*wa+b.w*wb,a.x*wa+b.x*wb,a.y*wa+b.y*wb,a.z*wa+b.z*wb};
        Normalize(q); return q;
    }
    void BreakHistory() { recovering_=true; returnAllowed_=false; candidateCount_=0; }
    void AcceptFresh(const vr::DriverPose_t& p,double t) {
        lastRaw_=lastOutput_=p; lastAccepted_=t; good_=true; recovering_=returnAllowed_=false; candidateCount_=0;
    }
    bool Candidate(const vr::DriverPose_t& p,double t,const Settings& s) {
        if(candidateCount_>0 && !FrameChanged(p,candidate_) && Plausible(p,candidate_,t-candidateTime_,s)) candidateCount_++;
        else candidateCount_=1;
        candidate_=p; candidateTime_=t;
        return candidateCount_>=s.reacquireSamples;
    }
    Decision Reject(vr::DriverPose_t& pose,Decision reason) const {
        const bool connected=pose.deviceIsConnected;
        if(good_) pose=lastOutput_;
        else { pose={}; pose.qRotation.w=pose.qWorldFromDriverRotation.w=pose.qDriverFromHeadRotation.w=1; }
        pose.deviceIsConnected=connected; pose.poseIsValid=false; pose.result=vr::TrackingResult_Running_OutOfRange; pose.poseTimeOffset=0;
        for(int i=0;i<3;i++) pose.vecVelocity[i]=pose.vecAcceleration[i]=pose.vecAngularVelocity[i]=pose.vecAngularAcceleration[i]=0;
        return reason;
    }
};
}
