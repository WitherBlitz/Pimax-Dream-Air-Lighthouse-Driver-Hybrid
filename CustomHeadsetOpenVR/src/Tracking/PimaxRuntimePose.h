#pragma once
#include <openvr_driver.h>
#include <PVR_Types.h>
#include <cmath>

namespace dreamair {
// PVR floor-space head poses already match the stock driver's raw coordinates.
// Keep the last finite reference on loss, but expose the original validity.
class PimaxRuntimePose {
public:
    vr::DriverPose_t Map(const pvrPoseStatef& head,pvrResult result,bool connected,double submissionTime,double velocityYaw=0) {
        const auto& q=head.ThePose.Orientation;
        const double norm=std::hypot(std::hypot(q.w,q.x),std::hypot(q.y,q.z));
        const double offset=head.TimeInSeconds-submissionTime;
        const bool valid=result==pvr_success&&connected&&(head.StatusFlags&3)==3
            &&std::isfinite(offset)&&offset>=-.25&&offset<=.05
            &&std::isfinite(norm)&&norm>1e-6&&std::isfinite(velocityYaw)&&Finite(head.ThePose.Position)
            &&Finite(head.LinearVelocity)&&Finite(head.AngularVelocity);
        if(valid) {
            vr::DriverPose_t pose{};
            pose.qWorldFromDriverRotation.w=pose.qDriverFromHeadRotation.w=1;
            pose.qRotation={q.w/norm,q.x/norm,q.y/norm,q.z/norm};
            Copy(head.ThePose.Position,pose.vecPosition);
            // PVR linear velocity already follows FloorLevel coordinates.
            Copy(head.LinearVelocity,pose.vecVelocity);
            double worldAngular[3];RotateYaw(head.AngularVelocity,velocityYaw,worldAngular);
            // The measured SteamVR predictor multiplies angular velocity by
            // qRotation. Supply body angular velocity for correct world
            // rotation at display time; linear velocity stays in world.
            RotateInverse(pose.qRotation,worldAngular,pose.vecAngularVelocity);
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
    static void RotateYaw(const pvrVector3f& v,double yaw,double* out){
        const double c=std::cos(yaw),s=std::sin(yaw);
        out[0]=c*v.x+s*v.z;out[1]=v.y;out[2]=-s*v.x+c*v.z;
    }
    static void RotateInverse(const vr::HmdQuaternion_t& q,const double* v,double* out){
        const double x=-q.x,y=-q.y,z=-q.z;
        const double tx=2*(y*v[2]-z*v[1]),ty=2*(z*v[0]-x*v[2]),tz=2*(x*v[1]-y*v[0]);
        out[0]=v[0]+q.w*tx+y*tz-z*ty;
        out[1]=v[1]+q.w*ty+z*tx-x*tz;
        out[2]=v[2]+q.w*tz+x*ty-y*tx;
    }
};
}
