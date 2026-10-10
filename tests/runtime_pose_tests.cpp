#include "../CustomHeadsetOpenVR/src/Tracking/PimaxRuntimePose.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <cmath>
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
        std::cout<<"PASS native coordinates, timing, full tracking validity, invalid hold, finite pose, API loss, disconnect, time bounds and quaternion normalization\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
