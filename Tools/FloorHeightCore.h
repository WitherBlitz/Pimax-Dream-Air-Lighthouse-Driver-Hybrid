#pragma once
// Headset-relative standing-floor calibration; no headset-only pose offsets.
#include "openvr.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
namespace floorheight {
using Matrix=vr::HmdMatrix34_t;
struct Sample { Matrix pose{};double velocity[3]{};bool connected=false,valid=false;vr::ETrackingResult result=vr::TrackingResult_Uninitialized;std::string serial;uint64_t universe=0; };
struct Backup { Matrix original{},applied{};std::string serial,id;uint64_t universe=0;double target=0; };
struct Runtime {virtual ~Runtime()=default;virtual bool Calibrated()=0;virtual Matrix ReadOrigin()=0;virtual Sample ReadHeadset()=0;virtual bool Stage(const Matrix&,const Matrix&)=0;virtual bool Commit()=0;virtual void Discard()=0;};
struct Store {virtual ~Store()=default;virtual void SavePending(Backup&)=0;virtual void MarkApplied(const Backup&)=0;virtual void MarkRestored(const Backup&)=0;};
inline double Height(double meters) {
    if(!std::isfinite(meters) || meters<0 || meters>10)throw std::invalid_argument("Height must be between 0 and 10 metres.");
    return meters;
}
inline double FeetInches(double feet,double inches) {
    if(!std::isfinite(feet) || !std::isfinite(inches) || feet<0 || feet!=std::floor(feet) || inches<0 || inches>=12)
        throw std::invalid_argument("Use whole nonnegative feet and inches from 0 to less than 12.");
    return Height((feet*12+inches)*0.0254);
}
inline double Centimeters(double cm) { return Height(cm/100); }
inline bool Same(const Matrix& a,const Matrix& b) {
    for(int i=0;i<3;i++)for(int j=0;j<4;j++)if(!std::isfinite(a.m[i][j]) || !std::isfinite(b.m[i][j]) || std::abs(static_cast<double>(a.m[i][j])-b.m[i][j])>1e-5)return false;
    return true;
}
inline void Rigid(const Matrix& m) {
    for(const auto& row:m.m)for(float value:row)if(!std::isfinite(value))throw std::invalid_argument("Tracking transform is not finite.");
    for(int a=0;a<3;a++)for(int b=0;b<3;b++) {
        double dot=0;for(int i=0;i<3;i++)dot+=static_cast<double>(m.m[i][a])*m.m[i][b];
        if(std::abs(dot-(a==b?1.0:0.0))>0.001)throw std::invalid_argument("Tracking transform is not a rigid rotation.");
    }
    const double det=static_cast<double>(m.m[0][0])*(m.m[1][1]*m.m[2][2]-m.m[1][2]*m.m[2][1])
        -static_cast<double>(m.m[0][1])*(m.m[1][0]*m.m[2][2]-m.m[1][2]*m.m[2][0])
        +static_cast<double>(m.m[0][2])*(m.m[1][0]*m.m[2][1]-m.m[1][1]*m.m[2][0]);
    if(std::abs(det-1)>0.001)throw std::invalid_argument("Tracking transform contains a reflection.");
}
inline Matrix Inverse(const Matrix& m) {
    Rigid(m);Matrix out{};
    for(int i=0;i<3;i++) {
        double offset=0;
        for(int j=0;j<3;j++){out.m[i][j]=m.m[j][i];offset-=static_cast<double>(out.m[i][j])*m.m[j][3];}
        out.m[i][3]=static_cast<float>(offset);
    }
    return out;
}
inline double HeadHeight(const Matrix& standingToRaw,const Matrix& headToRaw) {
    Rigid(standingToRaw);Rigid(headToRaw);double height=0;
    for(int i=0;i<3;i++)height+=static_cast<double>(standingToRaw.m[i][1])*(static_cast<double>(headToRaw.m[i][3])-standingToRaw.m[i][3]);
    if(!std::isfinite(height) || std::abs(height)>100)throw std::invalid_argument("Headset height is outside a plausible tracking space.");
    return height;
}
inline Matrix MoveFloor(const Matrix& standingToRaw,double current,double target) {
    Rigid(standingToRaw);Height(target);
    if(!std::isfinite(current) || std::abs(current)>100)throw std::invalid_argument("Current height is invalid.");
    Matrix out=standingToRaw;
    for(int i=0;i<3;i++)out.m[i][3]=static_cast<float>(static_cast<double>(out.m[i][3])+out.m[i][1]*(current-target));
    Rigid(out);return out;
}
inline void Stable(const std::vector<Sample>& samples) {
    if(samples.size()<20)throw std::invalid_argument("Not enough headset samples.");
    const auto& reference=samples.front();
    if(reference.serial.empty() || reference.universe==0)throw std::invalid_argument("Headset identity/tracking universe is unavailable.");
    for(const auto& s:samples) {
        if(!s.connected || !s.valid || s.result!=vr::TrackingResult_Running_OK)throw std::invalid_argument("Headset tracking is invalid. Wait for reliable tracking and try again.");
        if(s.serial!=reference.serial || s.universe!=reference.universe)throw std::invalid_argument("Headset or tracking universe changed during capture.");
        Rigid(s.pose);double speed2=0,displacement2=0;
        for(int i=0;i<3;i++) {
            if(!std::isfinite(s.velocity[i]))throw std::invalid_argument("Headset velocity is not finite.");
            speed2+=s.velocity[i]*s.velocity[i];
            const double difference=static_cast<double>(s.pose.m[i][3])-reference.pose.m[i][3];displacement2+=difference*difference;
        }
        if(speed2>0.08*0.08 || displacement2>0.01*0.01)throw std::invalid_argument("Keep the headset still while setting floor height.");
    }
}
inline void CommitOrigin(Runtime& runtime,const Matrix& expected,const Matrix& replacement) {
    if(!runtime.Calibrated())throw std::runtime_error("SteamVR room calibration is not ready.");
    if(!runtime.Stage(expected,replacement)){runtime.Discard();throw std::runtime_error("Standing origin changed or could not be staged. No floor was committed.");}
    if(!runtime.Commit()){runtime.Discard();throw std::runtime_error("SteamVR could not commit the standing floor. Backup retained.");}
    if(!Same(runtime.ReadOrigin(),replacement))throw std::runtime_error("SteamVR did not confirm the new standing origin. Backup retained.");
}
inline void Apply(Runtime& runtime,Store& store,const Matrix& captureOrigin,const std::vector<Sample>& captured,double target) {
    Height(target);Rigid(captureOrigin);
    if(!runtime.Calibrated())throw std::runtime_error("SteamVR room calibration is not ready.");
    auto samples=captured;samples.push_back(runtime.ReadHeadset());Stable(samples);
    const auto current=runtime.ReadOrigin();
    if(!Same(current,captureOrigin))throw std::runtime_error("Standing origin changed during capture. Try again.");
    Backup backup{};backup.original=current;backup.applied=MoveFloor(current,HeadHeight(current,samples.back().pose),target);
    backup.serial=samples.back().serial;backup.universe=samples.back().universe;backup.target=target;
    store.SavePending(backup); // Must succeed before any staging or commit.
    const auto fresh=runtime.ReadHeadset();samples.push_back(fresh);Stable(samples);
    if(std::abs(HeadHeight(current,fresh.pose)-HeadHeight(current,samples[samples.size()-2].pose))>0.001)
        throw std::runtime_error("Headset height changed while saving the backup. Hold still and try again.");
    try { CommitOrigin(runtime,backup.original,backup.applied);store.MarkApplied(backup); }
    catch(...) {
        const auto failure=std::current_exception();runtime.Discard();
        try {
            const auto live=runtime.ReadOrigin();
            if(!Same(live,backup.original)) {
                if(!Same(live,backup.applied))throw std::runtime_error("Another application changed the standing origin.");
                const auto identity=runtime.ReadHeadset();
                if(!identity.connected || identity.serial!=backup.serial || identity.universe!=backup.universe)
                    throw std::runtime_error("Headset or tracking universe changed; automatic rollback was refused.");
                CommitOrigin(runtime,live,backup.original);
            }
        } catch(...) { throw std::runtime_error("Floor operation failed and rollback could not be confirmed. Preserve backup "+backup.id+" and use Floor-Restore with that backup."); }
        std::rethrow_exception(failure);
    }
}
inline void Restore(Runtime& runtime,Store& store,const Backup& backup) {
    Rigid(backup.original);Rigid(backup.applied);
    if(backup.serial.empty() || backup.universe==0)throw std::invalid_argument("Invalid floor backup identity.");
    if(!runtime.Calibrated())throw std::runtime_error("SteamVR room calibration is not ready.");
    const auto headset=runtime.ReadHeadset();
    if(!headset.connected || headset.serial!=backup.serial || headset.universe!=backup.universe)throw std::runtime_error("Backup belongs to a different headset or tracking universe.");
    const auto current=runtime.ReadOrigin();
    if(Same(current,backup.original)){store.MarkRestored(backup);return;}
    if(!Same(current,backup.applied))throw std::runtime_error("Room calibration or another tool changed the origin after this backup. Restore was refused.");
    CommitOrigin(runtime,current,backup.original);store.MarkRestored(backup);
}
}
