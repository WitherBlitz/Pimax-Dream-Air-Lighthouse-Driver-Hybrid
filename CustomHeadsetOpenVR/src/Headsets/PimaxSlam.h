#pragma once
#include "BaseHeadset.h"
#include "../Helpers/PimaxCommon.h"
#include <atomic>
#include <thread>
#ifdef PVR_EXISTS
#include "../Tracking/PimaxRuntimePose.h"
#endif

class PimaxSlamDriver : public BaseHeadsetShim, public PimaxCommon {
public:
	PimaxSlamDriver();
	virtual bool IsDesiredHeadset(std::string model, vr::PropertyContainerHandle_t container) override;

	virtual Config::BaseHeadsetConfig& GetConfig() override;
	virtual Config::BaseHeadsetConfig& GetConfigOld() override;

	virtual void PosTrackedDeviceGetComponent(const char*& pchComponentNameAndVersion, void*& returnComponent) override;
	virtual void PosTrackedDeviceActivate(uint32_t& unObjectId, vr::EVRInitError& returnValue) override;
	virtual void SubDeactivate() override;
	virtual void RunFrame() override;
	virtual void RunPvrBackground() override;
	virtual void HandleEvent(const vr::VREvent_t& event) override;

	void StartPvrTracking();
	void StopPvrTracking();

private:
	void PvrTrackingThread();

	std::thread pvrTrackingThread;
	std::atomic<bool> pvrTrackingRunning{false};
	bool nativeLighthouseMode = false;
	uint64_t nativeUniverseId = 0;
	double nativeVelocityYaw = 0;
	bool nativeUniverseMismatchReported = false;
#ifdef PVR_EXISTS
	dreamair::PimaxRuntimePose nativeMapper;
#endif
};
