#include "PimaxLighthouse.h"

#include "../Helpers/EyeTrackingOutput.h"
#include "../Helpers/MiscHelper.h"
#include "../Tracking/TrackingManager.h"
#include "../Tracking/NativePoseBridge.h"
#include "../Driver/Hooking/InterfaceHookInjector.h"
#include <chrono>

bool PimaxLighthouseShim::IsDesiredHeadset(std::string model, vr::PropertyContainerHandle_t container){
	std::string trackingSystem = vr::VRProperties()->GetStringProperty(container, vr::Prop_TrackingSystemName_String);
	std::string manufacturer = vr::VRProperties()->GetStringProperty(container, vr::Prop_ManufacturerName_String);
	DriverLog("Model: %s, Manufacturer: %s, Tracking System: %s, IsLighthouseHeadsetConnected: %i, IsPimaxLighthouseDevice: %i\n", model.c_str(), manufacturer.c_str(), trackingSystem.c_str(), IsLighthouseHeadsetConnected(), IsPimaxLighthouseDevice(model, manufacturer));
	if(IsLighthouseHeadsetConnected() && IsPimaxLighthouseDevice(model, manufacturer) && trackingSystem == "lighthouse"){
		return true;
	}
	return false;
}

Config::BaseHeadsetConfig& PimaxLighthouseShim::GetConfig(){
	return GetHeadsetConfig();
}

Config::BaseHeadsetConfig& PimaxLighthouseShim::GetConfigOld(){
	return GetHeadsetConfigOld();
}

void PimaxLighthouseShim::PosTrackedDeviceActivate(uint32_t& unObjectId, vr::EVRInitError& returnValue){
	vr::PropertyContainerHandle_t container = vr::VRProperties()->TrackedDeviceToPropertyContainer(unObjectId);

#ifdef PVR_EXISTS
	// We need to set this config value before UpdateSettings() runs.
	// This is only necessary when using the PimaxDistortionProfile.
	pvr_setIntConfig(GetPvrSession(), "view_rotation_fix", GetConfig().parallelProjection);
#endif

	vr::VRProperties()->SetStringProperty(
		container, vr::Prop_InputProfilePath_String, ("{" + driverConfigLoader.info.driverName + "}/input/pimaxhmd_profile.json").c_str());
	vr::VRDriverInput()->CreateBooleanComponent(
		container, "/input/system/click", &inputComponents[ComponentSystemClick]);
	vr::VRDriverInput()->CreateBooleanComponent(container, "/input/tap/click", &inputComponents[ComponentTap]);

#ifdef PVR_EXISTS
	if (HasEyeTracking()) {
		eyeTrackingOutput.Initialize();
	}

	// We need to set the mesh values before UpdateSettings() runs.
	if (GetConfig().hiddenArea.enable && GetConfig().hiddenArea.autoHiddenArea) {
		SetVisibilityMeshes();
	}
#endif
	
	BaseHeadsetShim::PosTrackedDeviceActivate(unObjectId, returnValue);
	const bool desired=shimActive && isActive && IsDesiredHeadset(vr::VRProperties()->GetStringProperty(container, vr::Prop_ModelNumber_String),container);
	if(dreamair::trackingManager.RegisterActivated(unObjectId, returnValue == vr::VRInitError_None,
		shimActive,isActive,desired,GetConfig().headsetType == Config::HeadsetType::DreamAir)){
		dreamair::nativePoseBridge.Register(unObjectId);
		DriverLog("DreamAir LH1 stability: registered headset index %u", unObjectId);
	}
}

void PimaxLighthouseShim::SubDeactivate(){
	dreamair::trackingManager.Unregister(static_cast<uint32_t>(deviceIndex));
	dreamair::nativePoseBridge.Unregister(static_cast<uint32_t>(deviceIndex));
#ifdef PVR_EXISTS
	StopEyeTracking();
#endif
}

void PimaxLighthouseShim::RunFrame(){
	if(!isActive){
		// don't do anything if not the active device
		return;
	}
#ifdef PVR_EXISTS
	if (driverConfig.hasBeenUpdated) {
		shared_lock_guard<std::shared_mutex> lock(PimaxCommon::pvrLock);
		// We need to set this config value before UpdateSettings() runs.
		// This is only necessary when using the PimaxDistortionProfile.
		pvr_setIntConfig(GetPvrSession(), "view_rotation_fix", GetConfig().parallelProjection);
	}

	if (driverConfig.hasBeenUpdated &&
		(GetConfig().hiddenArea != GetConfigOld().hiddenArea || GetConfigOld().disableEye != GetConfig().disableEye)) {
		// We need to set the mesh values before UpdateSettings() runs.
		if (GetConfig().hiddenArea.enable && GetConfig().hiddenArea.autoHiddenArea) {
			SetVisibilityMeshes();
		}
	}
#endif

	BaseHeadsetShim::RunFrame();
	// Keep native pose updates independent of Lighthouse callback delivery.
	// Pimax's pose is already at the head reference; the bridge supplies the
	// fixed transform into the existing SteamVR room, without recentering it.
#ifdef PVR_EXISTS
	if(GetConfig().headsetType==Config::HeadsetType::DreamAir && dreamair::nativePoseBridge.Snapshot().enabled){
		vr::DriverPose_t pose{};
		pose.qRotation.w=pose.qWorldFromDriverRotation.w=pose.qDriverFromHeadRotation.w=1;
		pvrTrackingState tracking{};pvrHmdStatus status{};
		pvrResult trackingResult=pvr_invalid_param,statusResult=pvr_invalid_param;
		double now=0,queryArrival=0;
		{
			shared_lock_guard<std::shared_mutex> lock(PimaxCommon::pvrLock);
			auto session=GetPvrSession();
			queryArrival=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
			if(session){now=GetPvrTime();trackingResult=pvr_getTrackingState(session,now,&tracking);statusResult=pvr_getHmdStatus(session,&status);}
		}
		const auto& head=tracking.HeadPose;
		pose.deviceIsConnected=statusResult==pvr_success && status.ServiceReady && status.HmdPresent && !status.ShouldQuit;
		pose.poseIsValid=trackingResult==pvr_success && pose.deviceIsConnected && (head.StatusFlags&3)==3;
		pose.result=pose.poseIsValid?vr::TrackingResult_Running_OK:vr::TrackingResult_Running_OutOfRange;
		pose.poseTimeOffset=head.TimeInSeconds-now;
		pose.qRotation={head.ThePose.Orientation.w,head.ThePose.Orientation.x,head.ThePose.Orientation.y,head.ThePose.Orientation.z};
		pose.vecPosition[0]=head.ThePose.Position.x;pose.vecPosition[1]=head.ThePose.Position.y;pose.vecPosition[2]=head.ThePose.Position.z;
		pose.vecVelocity[0]=head.LinearVelocity.x;pose.vecVelocity[1]=head.LinearVelocity.y;pose.vecVelocity[2]=head.LinearVelocity.z;
		pose.vecAngularVelocity[0]=head.AngularVelocity.x;pose.vecAngularVelocity[1]=head.AngularVelocity.y;pose.vecAngularVelocity[2]=head.AngularVelocity.z;
		PublishPimaxPose(static_cast<uint32_t>(deviceIndex),pose,queryArrival);
	}
#endif
	
	if(GetInfo().directConnected){
		// reconnect the USB
		TryDirectConnection();
	}

	// Update the buttons state.
	bool systemClick = false, doubleTap = false;
	GetHmdButtonsState(systemClick, doubleTap);
	if(inputComponents[ComponentSystemClick]){
		vr::VRDriverInput()->UpdateBooleanComponent(inputComponents[ComponentSystemClick], systemClick, 0);
	}
	if(inputComponents[ComponentTap]){
		vr::VRDriverInput()->UpdateBooleanComponent(inputComponents[ComponentTap], doubleTap, 0);
	}

	// Make sure to run BaseHeadsetShim::RunFrame() for housekeeping before checking for lost connection.
#ifdef PVR_EXISTS
	if (CheckPvrDeviceLost()) {
		StopEyeTracking();
		return;
	}

	if(HasEyeTracking() && GetConfig().enableEyeTracking){
		StartEyeTracking();
	} else {
		StopEyeTracking();
	}
#endif
	eyeTrackingOutput.ipd = (float)(GetConfig().ipd + GetConfig().ipdOffset);
	eyeTrackingOutput.RunFrame();
	
}

void PimaxLighthouseShim::RunPvrBackground() {
#ifdef PVR_EXISTS
	pvr_setIntConfig(GetPvrSession(), "view_rotation_fix", GetConfig().parallelProjection);
	
	// Update proximity sensor.
	if(GetConfig().proximitySensorType == Config::ProximitySensorType::ProximitySensorTypeHardware){
		pvrHmdStatus hmdStatus = {};
		pvr_getHmdStatus(GetPvrSession(), &hmdStatus);
		if(inputComponents[ComponentProximity]){
			vr::VRDriverInput()->UpdateBooleanComponent(inputComponents[ComponentProximity], hmdStatus.HmdMounted, 0);
		}
	}

	if(GetInfo().headsetType == Config::HeadsetType::CrystalOG){
		// Update the battery level (Crystal OG).
		const vr::PropertyContainerHandle_t container = vr::VRProperties()->TrackedDeviceToPropertyContainer(deviceIndex);
		const int batteryPercentage = pvr_getTrackedDeviceIntProperty(
			GetPvrSession(), pvrTrackedDevice_HMD, pvrTrackedDeviceProp_BatteryPercent_int, -1);
		if (batteryPercentage > 0) {
			vr::VRProperties()->SetFloatProperty(container, vr::Prop_DeviceBatteryPercentage_Float, batteryPercentage / 100.f);
			vr::VRProperties()->SetBoolProperty(container, vr::Prop_DeviceProvidesBatteryStatus_Bool, true);
		}
	}

	PollMagicAttach();
#endif
}

void PimaxLighthouseShim::HandleEvent(const vr::VREvent_t& event){
	switch (event.eventType) {
	case vr::VREvent_SceneApplicationChanged:
#ifdef PVR_EXISTS
		SetSceneApplicationProcess(event.data.process.pid);
#endif
		break;
	}
}


#ifdef _WIN32
extern "C" {
// cant be bothered, implement them here
void* zcalloc(void* opaque, unsigned int items, unsigned int size){
	return malloc(items * size);
}
void zcfree(void* opaque, void* address){
	free(address);
}
const char * z_errmsg[12]{"","","","","","","","","","","",""};
}
#endif // _WIN32
