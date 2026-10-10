#include "../CustomHeadsetOpenVR/src/Tracking/NativeLighthouseAlignment.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static vr::DriverPose_t Pose(double x,double y,double z){vr::DriverPose_t p{};p.qRotation.w=p.qWorldFromDriverRotation.w=p.qDriverFromHeadRotation.w=1;p.vecPosition[0]=x;p.vecPosition[1]=y;p.vecPosition[2]=z;p.deviceIsConnected=p.poseIsValid=true;p.result=vr::TrackingResult_Running_OK;return p;}
int main(){try{
 dreamair::NativeLighthouseAlignment a;
 // Primary-station raw origin maps into PVR FloorLevel. Secondary-station
 // orientation can differ between runtimes; it must never choose the frame.
 a.Configure({{"primary",{{.7071067811865476,0,.7071067811865476,0},{.5,1.2,1.5}}},{"secondary",{{1,0,0,0},{2,2,3}}}});
 auto secondary=Pose(2,1,3);a.Process("secondary",vr::TrackedDeviceClass_TrackingReference,99,secondary);
 auto controller=Pose(1,2,3);a.Process("controller",vr::TrackedDeviceClass_Controller,99,controller);
 Check(controller.qWorldFromDriverRotation.w==1,"secondary station chose alignment");
 auto primary=Pose(0,0,0);Check(a.Process("primary",vr::TrackedDeviceClass_TrackingReference,99,primary),"primary frame was not selected");
 controller=Pose(1,2,3);controller.vecVelocity[0]=.4;controller.vecAngularVelocity[1]=.7;controller.poseTimeOffset=-.003;
 a.Process("controller",vr::TrackedDeviceClass_Controller,99,controller);
 Check(std::abs(controller.qWorldFromDriverRotation.y-.7071067811865476)<1e-9&&controller.vecWorldFromDriverTranslation[2]==1.5,"controller frame not mapped");
 Check(controller.vecPosition[0]==1&&controller.vecVelocity[0]==.4&&controller.vecAngularVelocity[1]==.7&&controller.poseTimeOffset==-.003,"controller prediction/source fields changed");
 auto hmd=Pose(1,2,3);a.Process("hmd",vr::TrackedDeviceClass_HMD,99,hmd);Check(hmd.qWorldFromDriverRotation.w==1,"HMD mapping changed");
 auto other=Pose(1,2,3);a.Process("other",vr::TrackedDeviceClass_Controller,100,other);Check(other.qWorldFromDriverRotation.w==1,"another universe changed");
 // Compose with an existing driver-to-world transform instead of replacing it.
 auto transformed=Pose(1,2,3);transformed.vecWorldFromDriverTranslation[0]=2;
 a.Process("controller",vr::TrackedDeviceClass_Controller,99,transformed);
 Check(std::abs(transformed.vecWorldFromDriverTranslation[0]-.5)<1e-9&&std::abs(transformed.vecWorldFromDriverTranslation[2]+.5)<1e-9,"world transform not composed");
 auto lost=Pose(1,2,3);lost.poseIsValid=false;lost.result=vr::TrackingResult_Running_OutOfRange;a.Process("controller",vr::TrackedDeviceClass_Controller,99,lost);Check(!lost.poseIsValid&&lost.result==vr::TrackingResult_Running_OutOfRange,"loss masked");
 a.Reset();auto untouched=Pose(1,2,3);a.Process("controller",vr::TrackedDeviceClass_Controller,99,untouched);Check(untouched.qWorldFromDriverRotation.w==1,"stale mapping used after reset");
 a.Configure({{"primary",{{1,0,0,0},{0,0,std::numeric_limits<double>::quiet_NaN()}}}});primary=Pose(0,0,0);Check(!a.Process("primary",vr::TrackedDeviceClass_TrackingReference,99,primary),"malformed reference accepted");
 // Static reference callbacks can arrive before the native worker starts.
 dreamair::NativeLighthouseAlignment early;primary=Pose(0,0,0);early.Process("primary",vr::TrackedDeviceClass_TrackingReference,99,primary);
 early.Configure({{"primary",{{1,0,0,0},{.5,1.2,1.5}}}});Check(early.Snapshot().ready,"early reference not paired at configure");
 primary=Pose(0,0,0);primary.qRotation.w=-1;Check(!early.Process("primary",vr::TrackedDeviceClass_TrackingReference,99,primary),"quaternion sign caused a frame change");
 // Raw universe changes require a new mapping; old-universe devices no longer
 // receive the transform. Losing the genuine primary clears the active frame.
 primary=Pose(0,0,0);primary.qRotation={0,0,1,0};early.Process("primary",vr::TrackedDeviceClass_TrackingReference,100,primary);
 controller=Pose(1,2,3);early.Process("controller",vr::TrackedDeviceClass_Controller,99,controller);Check(controller.qWorldFromDriverRotation.w==1,"old universe retained after frame change");
 primary.deviceIsConnected=false;primary.poseIsValid=false;early.Process("primary",vr::TrackedDeviceClass_TrackingReference,100,primary);Check(!early.Snapshot().ready,"invalid primary kept a mapping");
 // Nonidentity world/body/model rotations and offsets must all enter the
 // anchor pose; omitting any one changes the controller alignment.
 dreamair::NativeLighthouseAlignment full;full.Configure({{"primary",{{1,0,0,0},{.5,1.2,1.5}}}});
 primary=Pose(4,-3,-2.2);primary.qWorldFromDriverRotation={.7071067811865476,0,.7071067811865476,0};primary.vecWorldFromDriverTranslation[0]=2;primary.vecWorldFromDriverTranslation[1]=3;primary.vecWorldFromDriverTranslation[2]=4;
 primary.qRotation={.7071067811865476,.7071067811865476,0,0};primary.qDriverFromHeadRotation={.7071067811865476,0,0,.7071067811865476};primary.vecDriverFromHeadTranslation[1]=.2;
 Check(full.Process("primary",vr::TrackedDeviceClass_TrackingReference,99,primary),"full reference pose did not select primary");
 controller=Pose(1,2,3);full.Process("controller",vr::TrackedDeviceClass_Controller,99,controller);
 Check(std::abs(controller.qWorldFromDriverRotation.x+.7071067811865476)<1e-9&&std::abs(controller.qWorldFromDriverRotation.y)<1e-9&&std::abs(controller.vecWorldFromDriverTranslation[0]-.5)<1e-9,"full anchor geometry omitted an offset/rotation");
 // A sign-equivalent quaternion is the same frame, and multiple zero-origin
 // stations are ambiguous rather than a reason to pick the first entry.
 dreamair::NativeLighthouseAlignment ambiguous;ambiguous.Configure({{"a",{{1,0,0,0},{0,0,0}}},{"b",{{1,0,0,0},{1,0,0}}}});
 primary=Pose(0,0,0);ambiguous.Process("a",vr::TrackedDeviceClass_TrackingReference,99,primary);primary=Pose(0,0,0);ambiguous.Process("b",vr::TrackedDeviceClass_TrackingReference,99,primary);Check(!ambiguous.Snapshot().ready,"ambiguous primary guessed");
 ambiguous.Forget("a");Check(ambiguous.Snapshot().ready&&ambiguous.Snapshot().serial=="b","stale lifecycle reference blocked new primary");
 ambiguous.Forget("b");Check(!ambiguous.Snapshot().ready,"removed primary kept a mapping");
 std::cout<<"PASS Lighthouse primary alignment, transform composition, source preservation, HMD/universe isolation, loss and reset\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
