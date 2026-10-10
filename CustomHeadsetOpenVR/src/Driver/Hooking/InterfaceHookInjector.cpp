#include "../DriverLog.h"
#include "Hooking.h"
#include "InterfaceHookInjector.h"
#include "../DeviceProvider.h"
#include "../../Tracking/NativePoseBridge.h"
#include "../../Tracking/TrackingManager.h"
#include "../../Tracking/NativeLighthouseAlignment.h"
#include "../../Config/Config.h"
#include <chrono>
#include <mutex>

static CustomHeadsetDeviceProvider *Driver = nullptr;
// Serialize the original HMD source with native submissions. The owning host
// is captured from the Lighthouse callback, never from our own driver host.
static std::recursive_mutex poseSubmissionMutex;
using PoseCallback=void(*)(vr::IVRServerDriverHost*,uint32_t,const vr::DriverPose_t&,uint32_t);
static PoseCallback owningPoseCallback=nullptr;
static vr::IVRServerDriverHost* owningPoseHost=nullptr;
static uint32_t owningPoseIndex=vr::k_unTrackedDeviceIndexInvalid;
struct ReferenceCallback {vr::IVRServerDriverHost* host;PoseCallback callback;std::string serial;uint64_t universe;vr::DriverPose_t pose;};
static std::map<uint32_t,ReferenceCallback> referenceCallbacks;
static double PoseClock(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static bool ObserveLighthousePose(vr::IVRServerDriverHost* host,uint32_t index,const vr::DriverPose_t& pose,PoseCallback callback,double arrival){
	if(host==vr::VRServerDriverHost())return true;
	if(driverConfig.dreamAir.useNativePimaxTracking){
		const auto container=vr::VRProperties()->TrackedDeviceToPropertyContainer(index);
		const auto previous=referenceCallbacks.find(index);
		if(previous!=referenceCallbacks.end() &&
			(vr::VRProperties()->GetStringProperty(container,vr::Prop_SerialNumber_String)!=previous->second.serial
			||vr::VRProperties()->GetUint64Property(container,vr::Prop_CurrentUniverseId_Uint64)!=previous->second.universe
			||vr::VRProperties()->GetInt32Property(container,vr::Prop_DeviceClass_Int32)!=vr::TrackedDeviceClass_TrackingReference
			||vr::VRProperties()->GetStringProperty(container,vr::Prop_TrackingSystemName_String)!="lighthouse")){
			dreamair::nativeLighthouseAlignment.Forget(previous->second.serial);referenceCallbacks.erase(previous);
		}
		if(vr::VRProperties()->GetInt32Property(container,vr::Prop_DeviceClass_Int32)==vr::TrackedDeviceClass_TrackingReference
			&&vr::VRProperties()->GetStringProperty(container,vr::Prop_TrackingSystemName_String)=="lighthouse"){
			if(pose.deviceIsConnected&&pose.poseIsValid&&pose.result==vr::TrackingResult_Running_OK){
				referenceCallbacks[index]={host,callback,vr::VRProperties()->GetStringProperty(container,vr::Prop_SerialNumber_String),vr::VRProperties()->GetUint64Property(container,vr::Prop_CurrentUniverseId_Uint64),pose};
			}else{
				dreamair::nativeLighthouseAlignment.Forget(vr::VRProperties()->GetStringProperty(container,vr::Prop_SerialNumber_String));
				referenceCallbacks.erase(index);
			}
		}
	}
	const auto target=dreamair::nativePoseBridge.Snapshot().deviceIndex;
	if(index==target && index!=vr::k_unTrackedDeviceIndexInvalid){owningPoseHost=host;owningPoseCallback=callback;owningPoseIndex=index;}
	return dreamair::nativePoseBridge.Observe(index,pose,arrival);
}

void RefreshNativeLighthouseReferences(bool replay){
	std::lock_guard<std::recursive_mutex> submissionLock(poseSubmissionMutex);
	if(!driverConfig.dreamAir.useNativePimaxTracking)return;
	// Remove stale lifecycle identities even while alignment is ready, or an
	// obsolete zero-origin reference can prevent choosing the next primary.
	for(auto it=referenceCallbacks.begin();it!=referenceCallbacks.end();){
		const auto container=vr::VRProperties()->TrackedDeviceToPropertyContainer(it->first);const auto& cached=it->second;
		if(vr::VRProperties()->GetStringProperty(container,vr::Prop_SerialNumber_String)!=cached.serial
			||vr::VRProperties()->GetStringProperty(container,vr::Prop_TrackingSystemName_String)!="lighthouse"
			||vr::VRProperties()->GetInt32Property(container,vr::Prop_DeviceClass_Int32)!=vr::TrackedDeviceClass_TrackingReference
			||vr::VRProperties()->GetUint64Property(container,vr::Prop_CurrentUniverseId_Uint64)!=cached.universe){
			dreamair::nativeLighthouseAlignment.Forget(cached.serial);it=referenceCallbacks.erase(it);
		}else ++it;
	}
	if(!replay||!dreamair::nativeLighthouseAlignment.Snapshot().ready)return;
	// Original callbacks preserve owning-host identity. Static references may
	// never emit again after startup; republish their genuine cached source.
	for(const auto& entry:referenceCallbacks){
		const auto& cached=entry.second;const auto container=vr::VRProperties()->TrackedDeviceToPropertyContainer(entry.first);
		if(vr::VRProperties()->GetStringProperty(container,vr::Prop_SerialNumber_String)!=cached.serial
			||vr::VRProperties()->GetStringProperty(container,vr::Prop_TrackingSystemName_String)!="lighthouse"
			||vr::VRProperties()->GetInt32Property(container,vr::Prop_DeviceClass_Int32)!=vr::TrackedDeviceClass_TrackingReference
			||vr::VRProperties()->GetUint64Property(container,vr::Prop_CurrentUniverseId_Uint64)!=cached.universe)continue;
		auto pose=cached.pose;
		dreamair::nativeLighthouseAlignment.Process(cached.serial,vr::TrackedDeviceClass_TrackingReference,cached.universe,pose);
		// Stations are fixed references, so the original geometry remains valid
		// at replay time. Do not predict a reference from an old timestamp.
		pose.poseTimeOffset=0;
		cached.callback(cached.host,entry.first,pose,sizeof(pose));
	}
}

bool PublishPimaxPose(uint32_t index,const vr::DriverPose_t& nativePose,double queryArrival){
	std::lock_guard<std::recursive_mutex> submissionLock(poseSubmissionMutex);
	const double arrival=PoseClock();
	vr::DriverPose_t pose{};
	if(!dreamair::nativePoseBridge.Prepare(index,dreamair::RebaseNativePose(nativePose,queryArrival,arrival),arrival,pose))return false;
	if(index!=owningPoseIndex || !owningPoseHost || !owningPoseCallback)return false;
	dreamair::trackingManager.Process(index,pose,arrival);
	pose.poseTimeOffset-=PoseClock()-arrival;
	owningPoseCallback(owningPoseHost,index,pose,sizeof(pose));
	return true;
}

static Hook<void*(*)(vr::IVRDriverContext *, const char *, vr::EVRInitError *)> 
	GetGenericInterfaceHook("IVRDriverContext::GetGenericInterface");

static Hook<void(*)(vr::IVRServerDriverHost *, uint32_t, const vr::DriverPose_t &, uint32_t)>
	TrackedDevicePoseUpdatedHook005("IVRServerDriverHost005::TrackedDevicePoseUpdated");

static Hook<void(*)(vr::IVRServerDriverHost *, uint32_t, const vr::DriverPose_t &, uint32_t)>
	TrackedDevicePoseUpdatedHook006("IVRServerDriverHost006::TrackedDevicePoseUpdated");

static Hook<void(*)(vr::IVRServerDriverHost *_this, const char *pchDeviceSerialNumber, vr::ETrackedDeviceClass eDeviceClass, vr::ITrackedDeviceServerDriver *pDriver)>
	TrackedDeviceAddedHook006("IVRServerDriverHost006::TrackedDeviceAdded");

static void DetourTrackedDevicePoseUpdated005(vr::IVRServerDriverHost *_this, uint32_t unWhichDevice, const vr::DriverPose_t &newPose, uint32_t unPoseStructSize)
{
	const double arrival=PoseClock();
	std::lock_guard<std::recursive_mutex> submissionLock(poseSubmissionMutex);
	if(!ObserveLighthousePose(_this,unWhichDevice,newPose,TrackedDevicePoseUpdatedHook005.originalFunc,arrival))return;
	//TRACE("ServerTrackedDeviceProvider::DetourTrackedDevicePoseUpdated(%d)", unWhichDevice);
	const double submission=PoseClock();
	auto pose = dreamair::RebaseNativePose(newPose,arrival,submission);
	if (Driver->HandleDevicePoseUpdated(unWhichDevice, pose))
	{
		pose.poseTimeOffset-=PoseClock()-submission;
		TrackedDevicePoseUpdatedHook005.originalFunc(_this, unWhichDevice, pose, unPoseStructSize);
	}
}

static void DetourTrackedDevicePoseUpdated006(vr::IVRServerDriverHost *_this, uint32_t unWhichDevice, const vr::DriverPose_t &newPose, uint32_t unPoseStructSize)
{
	const double arrival=PoseClock();
	std::lock_guard<std::recursive_mutex> submissionLock(poseSubmissionMutex);
	if(!ObserveLighthousePose(_this,unWhichDevice,newPose,TrackedDevicePoseUpdatedHook006.originalFunc,arrival))return;
	//TRACE("ServerTrackedDeviceProvider::DetourTrackedDevicePoseUpdated(%d)", unWhichDevice);
	const double submission=PoseClock();
	auto pose = dreamair::RebaseNativePose(newPose,arrival,submission);
	if (Driver->HandleDevicePoseUpdated(unWhichDevice, pose))
	{
		pose.poseTimeOffset-=PoseClock()-submission;
		TrackedDevicePoseUpdatedHook006.originalFunc(_this, unWhichDevice, pose, unPoseStructSize);
	}
}

static void DetourTrackedDeviceAdded006(vr::IVRServerDriverHost *_this, const char *pchDeviceSerialNumber, vr::ETrackedDeviceClass eDeviceClass, vr::ITrackedDeviceServerDriver *pDriver)
{
	if (Driver->HandleDeviceAdded(pchDeviceSerialNumber, eDeviceClass, pDriver))  
	{
		TrackedDeviceAddedHook006.originalFunc(_this, pchDeviceSerialNumber, eDeviceClass, pDriver);
		// // Add 19 more copies with random serial numbers
		// for (int i = 0; i < 19; i++)
		// {
		// 	char serial[32]{};
		// 	snprintf(serial, sizeof(serial), "FIM-%d-%d", i, rand());
		// 	TrackedDeviceAddedHook006.originalFunc(_this, serial, eDeviceClass, pDriver);
		// }
	}
}

static void *DetourGetGenericInterface(vr::IVRDriverContext *_this, const char *pchInterfaceVersion, vr::EVRInitError *peError)
{
	Driver->driverContexts.insert(_this);  // Store the driver context for later use
	
	// TRACE("ServerTrackedDeviceProvider::DetourGetGenericInterface(%s)", pchInterfaceVersion);
	auto originalInterface = GetGenericInterfaceHook.originalFunc(_this, pchInterfaceVersion, peError);

	std::string iface(pchInterfaceVersion);
	if (iface == "IVRServerDriverHost_005")
	{
		if (!IHook::Exists(TrackedDevicePoseUpdatedHook005.name))
		{
			TrackedDevicePoseUpdatedHook005.CreateHookInObjectVTable(originalInterface, 1, &DetourTrackedDevicePoseUpdated005);
			IHook::Register(&TrackedDevicePoseUpdatedHook005);
		}
	}
	else if (iface == "IVRServerDriverHost_006")
	{
		if (!IHook::Exists(TrackedDevicePoseUpdatedHook006.name))
		{
			TrackedDevicePoseUpdatedHook006.CreateHookInObjectVTable(originalInterface, 1, &DetourTrackedDevicePoseUpdated006);
			IHook::Register(&TrackedDevicePoseUpdatedHook006);
		}
		if (!IHook::Exists(TrackedDeviceAddedHook006.name))
		{
			TrackedDeviceAddedHook006.CreateHookInObjectVTable(originalInterface, 0, &DetourTrackedDeviceAdded006);
			IHook::Register(&TrackedDeviceAddedHook006);
		}
	}

	return originalInterface;
}

void InjectHooks(CustomHeadsetDeviceProvider *driver, vr::IVRDriverContext *pDriverContext)
{
	Driver = driver;

	// auto err = MH_Initialize();
	// if (err == MH_OK)
	// {
	// 	GetGenericInterfaceHook.CreateHookInObjectVTable(pDriverContext, 0, &DetourGetGenericInterface);
	// 	IHook::Register(&GetGenericInterfaceHook);
	// }
	// else
	// {
	// 	DriverLog("MH_Initialize error: %s", MH_StatusToString(err));
	// }
	GetGenericInterfaceHook.CreateHookInObjectVTable(pDriverContext, 0, &DetourGetGenericInterface);
	IHook::Register(&GetGenericInterfaceHook);
}

void DisableHooks()
{
	IHook::DestroyAll();
	// MH_Uninitialize();
}
