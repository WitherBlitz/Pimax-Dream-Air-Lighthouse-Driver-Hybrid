#include "../CustomHeadsetOpenVR/src/Tracking/PimaxRuntimePose.h"
#include "../CustomHeadsetOpenVR/src/Tracking/PimaxRuntimeRoom.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <sstream>
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static pvrPoseStatef Head(){pvrPoseStatef p{};p.StatusFlags=3;p.ThePose.Orientation.w=1;p.ThePose.Position={1,2,3};p.TimeInSeconds=10;p.LinearVelocity={.1f,.2f,.3f};p.AngularVelocity={.4f,.5f,.6f};return p;}
int main(){
    try{
        dreamair::PimaxRuntimePose mapper;
        auto h=Head();auto p=mapper.Map(h,pvr_success,true,10.004);
        Check(p.poseIsValid&&p.result==vr::TrackingResult_Running_OK,"6DoF pose rejected");
        Check(p.vecPosition[0]==1&&p.vecPosition[1]==2&&p.vecPosition[2]==3,"native coordinates changed");
        Check(p.qWorldFromDriverRotation.w==1&&p.vecWorldFromDriverTranslation[1]==0,"unexpected room transform");
        Check(std::abs(p.poseTimeOffset+.004)<1e-8,"submission latency omitted");
        Check(std::abs(p.vecAngularVelocity[2]-.6)<1e-6,"angular velocity changed");
        for(unsigned flags:{0u,1u,2u}){h.StatusFlags=flags;p=mapper.Map(h,pvr_success,true,10.005);Check(!p.poseIsValid,"incomplete tracking accepted");Check(p.vecPosition[1]==2,"invalid tracking moved the reference");}
        h=Head();h.ThePose.Position.x=std::numeric_limits<float>::quiet_NaN();p=mapper.Map(h,pvr_success,true,10.006);Check(!p.poseIsValid&&std::isfinite(p.vecPosition[0]),"NaN escaped");
        h=Head();h.ThePose.Orientation={0,0,0,0};Check(!mapper.Map(h,pvr_success,true,10.006).poseIsValid,"zero quaternion accepted");
        h=Head();Check(!mapper.Map(h,pvr_invalid_param,true,10.006).poseIsValid,"API failure accepted");
        Check(!mapper.Map(h,pvr_success,false,10.006).deviceIsConnected,"disconnect hidden");
        Check(!mapper.Map(h,pvr_success,true,10.5).poseIsValid,"stale pose accepted");
        Check(!mapper.Map(h,pvr_success,true,9.9).poseIsValid,"future pose accepted");
        h=Head();h.ThePose.Orientation.w=2;p=mapper.Map(h,pvr_success,true,10.006);Check(p.poseIsValid&&p.qRotation.w==1,"quaternion not normalized");
        Check(!mapper.Map(h,pvr_success,true,std::numeric_limits<double>::quiet_NaN()).poseIsValid,"nonfinite time accepted");
        // FloorLevel rotates PVR poses and linear velocity by the inverse room
        // yaw; SDK 1.33.1 angular velocity keeps the original tracking frame.
        h=Head();h.AngularVelocity={1,0,0};h.LinearVelocity={0,0,2};
        p=mapper.Map(h,pvr_success,true,10.004,3.141592653589793/2);
        Check(std::abs(p.vecAngularVelocity[0])<1e-7&&std::abs(p.vecAngularVelocity[2]+1)<1e-7,"pitch prediction is not in pose space");
        Check(std::abs(p.vecVelocity[0])<1e-7&&std::abs(p.vecVelocity[2]-2)<1e-7,"world linear velocity was rotated twice");
        Check(p.vecPosition[0]==1&&p.qRotation.w==1,"velocity correction altered optics/pose");
        Check(!mapper.Map(h,pvr_success,true,10.004,std::numeric_limits<double>::quiet_NaN()).poseIsValid,"invalid room yaw accepted");
        // SteamVR 2.17.10 applies DriverPose angular velocity in body space:
        // measured +20ms prediction is R(pose)*the submitted angular vector.
        h=Head();h.ThePose.Orientation={0,.70710678f,0,.70710678f};h.AngularVelocity={1,0,0};
        p=mapper.Map(h,pvr_success,true,10.004);
        Check(std::abs(p.vecAngularVelocity[0])<1e-6&&std::abs(p.vecAngularVelocity[2]-1)<1e-6,"world angular velocity was not converted to body space");
        std::istringstream room(R"({"universes":[{"universeID":"30","standing":{"yaw":0}},{"universeID":"31","standing":{"yaw":-2.391004800796509}}]})");
        auto yaw=dreamair::ReadPimaxVelocityYaw(room,31);
        Check(yaw&&std::abs(*yaw-2.391004800796509)<1e-10,"wrong room, yaw sign or units");
        h=Head();p=mapper.Map(h,pvr_success,true,10.004,*yaw);
        Check(std::abs(p.vecAngularVelocity[0]-.116717)<.0001&&std::abs(p.vecAngularVelocity[2]+.711599)<.0001,"recorded room prediction regression");
        for(const char* text:{"broken",R"({"universes":[]})",R"({"universes":[{"universeID":"31","standing":{"yaw":"bad"}}]})",R"({"universes":[{"universeID":"31","standing":{"yaw":0}},{"universeID":"31","standing":{"yaw":1}}]})"}){
            std::istringstream invalid(text);Check(!dreamair::ReadPimaxVelocityYaw(invalid,31),"ambiguous/missing/malformed room accepted");
        }
        std::cout<<"PASS native pose, validity, timing, finite loss, quaternion normalization, room yaw and body-space prediction\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
