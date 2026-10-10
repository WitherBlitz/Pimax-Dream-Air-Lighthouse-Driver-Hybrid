#pragma once
#include <openvr_driver.h>
#include <map>
#include <string>
#include <cmath>
#include <mutex>
#include <optional>
namespace dreamair {
struct AlignmentPose {vr::HmdQuaternion_t rotation;double position[3];};
// One rigid world transform for Lighthouse devices. The PVR HMD and all
// sensor-space pose/prediction fields stay untouched. Reference input is
// always cached before applying our transform, so no feedback loop exists.
class NativeLighthouseAlignment {
public:
 struct State {bool ready=false;uint64_t universe=0;std::string serial;AlignmentPose transform{{1,0,0,0},{0,0,0}};};
 void Configure(const std::map<std::string,AlignmentPose>& targets){
  std::lock_guard<std::mutex> lock(mutex_);targets_.clear();state_={};
  for(auto entry:targets)if(Normalize(entry.second.rotation)&&Finite(entry.second.position))targets_.insert(entry);
  Select();
 }
 // Keep genuine raw reference observations across a PVR reconnect, but never
 // keep an active mapping after source loss. SteamVR reload creates fresh state.
 void Reset(){std::lock_guard<std::mutex> lock(mutex_);targets_.clear();state_={};}
 void Forget(const std::string& serial){std::lock_guard<std::mutex> lock(mutex_);references_.erase(serial);Select();}
 State Snapshot(){std::lock_guard<std::mutex> lock(mutex_);return state_;}
 bool Process(const std::string& serial,vr::ETrackedDeviceClass cls,uint64_t universe,vr::DriverPose_t& pose){
  if(cls==vr::TrackedDeviceClass_HMD||cls==vr::TrackedDeviceClass_Invalid||serial.empty()||universe==0)return false;
  std::lock_guard<std::mutex> lock(mutex_);
  bool changed=false;
  if(cls==vr::TrackedDeviceClass_TrackingReference){
   AlignmentPose raw{};
   if(pose.poseIsValid&&pose.deviceIsConnected&&pose.result==vr::TrackingResult_Running_OK&&WorldPose(pose,raw)){
    references_[serial]={universe,raw};
   }else{
    references_.erase(serial);if(state_.serial==serial)state_={};
   }
   const auto before=state_;Select();
   changed=state_.ready&&(!before.ready||before.universe!=state_.universe||before.serial!=state_.serial||Distance(before.transform,state_.transform)>1e-8);
  }
  if(state_.ready&&state_.universe==universe){
   auto world=pose.qWorldFromDriverRotation;
   if(Normalize(world)&&Finite(pose.vecWorldFromDriverTranslation)){
    double translated[3];Rotate(state_.transform.rotation,pose.vecWorldFromDriverTranslation,translated);
    pose.qWorldFromDriverRotation=Multiply(state_.transform.rotation,world);
    for(int i=0;i<3;i++)pose.vecWorldFromDriverTranslation[i]=translated[i]+state_.transform.position[i];
   }
  }
  return changed;
 }
private:
 struct Reference {uint64_t universe;AlignmentPose pose;};
 std::mutex mutex_;std::map<std::string,AlignmentPose> targets_;std::map<std::string,Reference> references_;State state_;
 static bool Finite(const double* p){return std::isfinite(p[0])&&std::isfinite(p[1])&&std::isfinite(p[2]);}
 static bool Normalize(vr::HmdQuaternion_t& q){double n=std::hypot(std::hypot(q.w,q.x),std::hypot(q.y,q.z));if(!std::isfinite(n)||n<1e-6)return false;q={q.w/n,q.x/n,q.y/n,q.z/n};return true;}
 static vr::HmdQuaternion_t Multiply(const vr::HmdQuaternion_t& a,const vr::HmdQuaternion_t& b){return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};}
 static void Rotate(const vr::HmdQuaternion_t& q,const double* p,double* out){double tx=2*(q.y*p[2]-q.z*p[1]),ty=2*(q.z*p[0]-q.x*p[2]),tz=2*(q.x*p[1]-q.y*p[0]);out[0]=p[0]+q.w*tx+q.y*tz-q.z*ty;out[1]=p[1]+q.w*ty+q.z*tx-q.x*tz;out[2]=p[2]+q.w*tz+q.x*ty-q.y*tx;}
 static bool WorldPose(const vr::DriverPose_t& p,AlignmentPose& out){
  auto w=p.qWorldFromDriverRotation,r=p.qRotation,h=p.qDriverFromHeadRotation;
  if(!Normalize(w)||!Normalize(r)||!Normalize(h)||!Finite(p.vecPosition)||!Finite(p.vecWorldFromDriverTranslation)||!Finite(p.vecDriverFromHeadTranslation))return false;
  out.rotation=Multiply(Multiply(w,r),h);double local[3],world[3];Rotate(r,p.vecDriverFromHeadTranslation,local);for(int i=0;i<3;i++)local[i]+=p.vecPosition[i];Rotate(w,local,world);for(int i=0;i<3;i++)out.position[i]=world[i]+p.vecWorldFromDriverTranslation[i];return true;
 }
 static double Distance(const AlignmentPose& a,const AlignmentPose& b){double d=std::abs(a.rotation.w*b.rotation.w+a.rotation.x*b.rotation.x+a.rotation.y*b.rotation.y+a.rotation.z*b.rotation.z);for(int i=0;i<3;i++)d-=std::abs(a.position[i]-b.position[i]);return std::abs(1-d);}
 void Select(){
  std::optional<State> selected;
  for(const auto& entry:references_){
   const auto target=targets_.find(entry.first);if(target==targets_.end())continue;
   const auto& raw=entry.second.pose;
   // Lighthouse 1.0 defines its primary station at the raw origin. Selecting
   // another station mixes separately stored station calibrations (measured
   // ~5 degrees disagreement) and gives the wrong controller transform.
   if(std::hypot(std::hypot(raw.position[0],raw.position[1]),raw.position[2])>.05)continue;
   if(selected){state_={};return;} // Ambiguous primary: don't guess a frame.
   auto inverse=raw.rotation;inverse.x=-inverse.x;inverse.y=-inverse.y;inverse.z=-inverse.z;
   State value;value.ready=true;value.universe=entry.second.universe;value.serial=entry.first;value.transform.rotation=Multiply(target->second.rotation,inverse);
   double shifted[3];Rotate(value.transform.rotation,raw.position,shifted);for(int i=0;i<3;i++)value.transform.position[i]=target->second.position[i]-shifted[i];selected=value;
  }
  state_=selected.value_or(State{});
 }
};
inline NativeLighthouseAlignment nativeLighthouseAlignment;
}
