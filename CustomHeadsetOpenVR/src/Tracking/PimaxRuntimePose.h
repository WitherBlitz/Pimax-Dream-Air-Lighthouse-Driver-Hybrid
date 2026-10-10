#pragma once
#include <openvr_driver.h>
#include <PVR_Types.h>
#include <cmath>

namespace dreamair {
// PVR floor-space head poses already match the stock driver's raw coordinates.
// Keep the last finite reference on loss, but expose the original validity.
class PimaxRuntimePose {
public:
    vr::DriverPose_t Map(const pvrPoseStatef& head,pvrResult result,bool connected,double submissionTime) {
        const auto& q=head.ThePose.Orientation;
        const double norm=std::hypot(std::hypot(q.w,q.x),std::hypot(q.y,q.z));
        const double offset=head.TimeInSeconds-submissionTime;
        const bool valid=result==pvr_success&&connected&&(head.StatusFlags&3)==3
            &&std::isfinite(offset)&&offset>=-.25&&offset<=.05
            &&std::isfinite(norm)&&norm>1e-6&&Finite(head.ThePose.Position)
            &&Finite(head.LinearVelocity)&&Finite(head.AngularVelocity);
        if(valid) {
            vr::DriverPose_t pose{};
            pose.qWorldFromDriverRotation.w=pose.qDriverFromHeadRotation.w=1;
            pose.qRotation={q.w/norm,q.x/norm,q.y/norm,q.z/norm};
            Copy(head.ThePose.Position,pose.vecPosition);
            Copy(head.LinearVelocity,pose.vecVelocity);
            Copy(head.AngularVelocity,pose.vecAngularVelocity);
            pose.poseTimeOffset=offset;pose.deviceIsConnected=pose.poseIsValid=true;
            pose.result=vr::TrackingResult_Running_OK;last_=pose;return pose;
        }
        auto pose=last_;pose.poseIsValid=false;pose.deviceIsConnected=connected;
        pose.result=vr::TrackingResult_Running_OutOfRange;pose.poseTimeOffset=0;
        for(int i=0;i<3;i++)pose.vecVelocity[i]=pose.vecAngularVelocity[i]=pose.vecAcceleration[i]=pose.vecAngularAcceleration[i]=0;
        return pose;
    }
private:
    vr::DriverPose_t last_=[](){vr::DriverPose_t p{};p.qRotation.w=p.qWorldFromDriverRotation.w=p.qDriverFromHeadRotation.w=1;return p;}();
    static bool Finite(const pvrVector3f& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
    static void Copy(const pvrVector3f& v,double* out){out[0]=v.x;out[1]=v.y;out[2]=v.z;}
};
}
