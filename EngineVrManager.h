#pragma once
#include <WickedEngine.h>
#include "wiGraphicsDevice_DX12.h"
#include "wiGraphicsDevice_Vulkan.h"

#include <d3d12.h>
//#include <vulkan/vulkan.h>

#include <wrl/client.h>

#include "openvr.h"

class EngineVrManager
{
public:
	EngineVrManager();
	~EngineVrManager();

	static EngineVrManager* getInstance()
	{
		if (instance == nullptr)
		{
			instance = new EngineVrManager();
		}

		return instance;
	}

	static void removeInstance()
	{
		if (instance != 0)
		{
			delete instance;
			instance = nullptr;
		}
	}
	void startVrSession(wi::scene::Scene& scene);
	void stopVrSession();
	bool isVrSessionActive();
	void render(float dt);
	//void moveVrFromTouchs(float dt);

	bool isLeftPadPressed();
	bool isRightPadPressed();
	XMFLOAT2 getPadValues();
	float getPadValueX();
	float getPadValueY();
	bool isButtonX();
	bool isButtonY();
	bool isButtonMenu();
	bool isButtonHome();
	bool isButtonA();
	bool isButtonB();
	bool isButtonTriggerLeftA();
	bool isButtonTriggerLeftB();
	bool isButtonTriggerRightA();
	bool isButtonTriggerRightB();

private:
	static EngineVrManager* instance;

	enum CONTROLLER
	{
		NONE,
		TOUCHPAD_LEFT,
		TOUCHPAD_RIGHT,
		BUTTON_X,
		BUTTON_Y,
		BUTTON_A,
		BUTTON_B,
		BUTTON_HOME,
		BUTTON_MENU,
		BUTTON_TRIGGER_LEFT_A,
		BUTTON_TRIGGER_LEFT_B,
		BUTTON_TRIGGER_RIGHT_A,
		BUTTON_TRIGGER_RIGHT_B
	};

	struct Control
	{
		CONTROLLER controller = EngineVrManager::CONTROLLER::NONE;
		CONTROLLER controllerDir = EngineVrManager::CONTROLLER::NONE;
		XMFLOAT2 axis = XMFLOAT2(0, 0);
		bool butonState = false;
		//int controllerDevice = -1;
	};

	//Textures
	wi::graphics::Texture rtLeftTexture;
	wi::graphics::Texture rtRightTexture;

	//Cameras
	wi::ecs::Entity cameraEntityLeft = wi::ecs::INVALID_ENTITY;
	wi::ecs::Entity cameraEntityRight = wi::ecs::INVALID_ENTITY;

	//hands models
	wi::ecs::Entity rightHand = wi::ecs::INVALID_ENTITY;
	wi::ecs::Entity leftHand = wi::ecs::INVALID_ENTITY;

	//RenderPath
	wi::RenderPath3D renderPathLeft, renderPathRight;

	void updateVrSession(float dt);
	void RenderRt(vr::Hmd_Eye nEye, float dt);
	std::string GetTrackedDeviceString(vr::IVRSystem* pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError* peError = nullptr);
	XMMATRIX ConvertSteamVRMatrixToXMMATRIX(const vr::HmdMatrix34_t& matPose);
	XMMATRIX GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye);
	XMMATRIX GetHMDMatrixPoseEye(vr::Hmd_Eye nEye);
	void createVrCameras();
	void getControllerActions(vr::VRControllerState_t state, int unDevice, float dt);
	wi::graphics::Texture resizeImage(const wi::graphics::Texture& image, int width, int height);

	bool isVrRunning = false;

	vr::IVRSystem* hmd;
	vr::IVRRenderModels* renderModels;
	vr::Hmd_Eye eyes;//vr::Eye_Right
	vr::TrackedDevicePose_t trackedDevicePose[vr::k_unMaxTrackedDeviceCount];
	XMMATRIX mat4DevicePose[vr::k_unMaxTrackedDeviceCount];

	uint32_t widthTexture = 0;
	uint32_t heightTexture = 0;

	XMMATRIX mat4HMDPose, mat4eyePosLeft, mat4ProjectionLeft, mat4ProjectionRight, mat4eyePosRight;

	wi::scene::TransformComponent cameraTransform;
	XMFLOAT4X4 projection;
	XMFLOAT3 up, eye, at;
	wi::scene::Scene* sceneVR;

	Control controllerVR;
	bool dx12 = false;
};
