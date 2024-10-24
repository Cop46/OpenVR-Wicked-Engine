#include <stdafx.h>
#include "WickedEngine.h"
#include "EngineVrManager.h"
#include <algorithm>

EngineVrManager* EngineVrManager::instance = nullptr;

EngineVrManager::EngineVrManager() {};

EngineVrManager::~EngineVrManager() {};

void EngineVrManager::startVrSession()
{
	//Disable VSync
	wi::eventhandler::SetVSync(false);

	if (dynamic_cast<wi::graphics::GraphicsDevice_DX12*>(wi::graphics::GetDevice()))
	{
		dx12 = true;
	}

	//save value of camera before VR session
	eye = wi::scene::GetCamera().Eye;
	up = wi::scene::GetCamera().Up;
	at = wi::scene::GetCamera().At;
	projection = wi::scene::GetCamera().Projection;

	//Get the transformation of camera for moving VR
	cameraTransform.world = wi::scene::GetCamera().InvView;

	//loading openVR runtime
	vr::EVRInitError error = vr::VRInitError_None;
	hmd = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None)
	{
		wi::backlog::post("Failed to init VR runtime.", wi::backlog::LogLevel::Error);
	}

	renderModels = (vr::IVRRenderModels*)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &error);
	if (!renderModels)
	{
		stopVrSession();
		return;
	}
	
	std::string m_strDriver = "No Driver";
	std::string m_strDisplay = "No Display";

	m_strDriver = GetTrackedDeviceString(hmd, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
	m_strDisplay = GetTrackedDeviceString(hmd, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

	hmd->GetRecommendedRenderTargetSize(&widthTexture,&heightTexture);

	mat4ProjectionLeft = GetHMDMatrixProjectionEye(vr::Eye_Left);
	mat4ProjectionRight = GetHMDMatrixProjectionEye(vr::Eye_Right);
	mat4eyePosLeft = GetHMDMatrixPoseEye(vr::Eye_Left);
	mat4eyePosRight = GetHMDMatrixPoseEye(vr::Eye_Right);

	memset(devClassChar, 0, sizeof(devClassChar));

	if (!vr::VRCompositor())
	{
		wi::backlog::post("Compositor initialization failed.\n",wi::backlog::LogLevel::Error);
	}

	isVrRunning = true;
}

void EngineVrManager::stopVrSession()
{
	isVrRunning = false;
	if (hmd != nullptr)
	{
		hmd = nullptr;
		vr::VR_Shutdown();
	}

	//restore camera after VR session
	wi::scene::GetCamera().Eye = eye;
	wi::scene::GetCamera().Up = up;
	wi::scene::GetCamera().At = at;
	wi::scene::GetCamera().Projection = projection;
	wi::scene::GetCamera().UpdateCamera();

	wi::scene::CameraComponent* cameraLeft = wi::scene::GetScene().cameras.GetComponent(cameraEntityLeft);
	wi::scene::CameraComponent* cameraRight = wi::scene::GetScene().cameras.GetComponent(cameraEntityRight);

	if (cameraLeft != nullptr)
	{
		wi::scene::GetScene().Entity_Remove(cameraEntityLeft, true);
		cameraEntityLeft = wi::ecs::INVALID_ENTITY;
	}

	if (cameraRight != nullptr)
	{
		wi::scene::GetScene().Entity_Remove(cameraEntityRight, true);
		cameraEntityRight = wi::ecs::INVALID_ENTITY;
	}
}

void EngineVrManager::createVrCameras()
{
	if (
		cameraEntityLeft == wi::ecs::INVALID_ENTITY || 
		cameraEntityRight == wi::ecs::INVALID_ENTITY || 
		(wi::scene::GetScene().cameras.GetComponent(cameraEntityLeft) == nullptr &&
		wi::scene::GetScene().cameras.GetComponent(cameraEntityRight) == nullptr))
	{
		cameraEntityLeft = wi::ecs::CreateEntity();
		cameraEntityRight = wi::ecs::CreateEntity();
		wi::scene::CameraComponent* cameraRight = &wi::scene::GetScene().cameras.Create(cameraEntityRight);
		wi::scene::CameraComponent* cameraLeft = &wi::scene::GetScene().cameras.Create(cameraEntityLeft);

		renderPathLeft.scene = &wi::scene::GetScene();
		cameraLeft->width = (float)widthTexture;
		cameraLeft->height = (float)heightTexture;
		renderPathLeft.camera = cameraLeft;
		renderPathLeft.width = widthTexture;
		renderPathLeft.height = heightTexture;
		renderPathLeft.resolutionScale = 0.75f;
		renderPathLeft.ResizeBuffers();

		renderPathRight.scene = &wi::scene::GetScene();
		cameraRight->width = (float)widthTexture;
		cameraRight->height = (float)heightTexture;
		renderPathRight.camera = cameraRight;
		renderPathRight.width = widthTexture;
		renderPathRight.height = heightTexture;
		renderPathRight.resolutionScale = 0.75f;
		renderPathRight.ResizeBuffers();
	}
}

void EngineVrManager::updateVrSession( float dt)
{
	if (isVrRunning && hmd != nullptr)
	{
		createVrCameras();

		// Process controller state
		for (vr::TrackedDeviceIndex_t unDevice = 0; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++)
		{
			vr::VRControllerState_t state;
			if (hmd->GetControllerState(unDevice, &state, sizeof(state)))
			{
				getControllerActions(state, unDevice, dt);
			}
		}

		//Update HMD pose
		vr::EVRCompositorError compError = vr::VRCompositor()->WaitGetPoses(trackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);
		if (compError != vr::VRCompositorError_None)
		{
			wi::backlog::post("Error waiting for compositor pose", wi::backlog::LogLevel::Error);
		}

		int validPoseCount = 0;
		std::string strPoseClasses = "";
		for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
		{
			if (trackedDevicePose[nDevice].bPoseIsValid)
			{
				if (!hmd->IsTrackedDeviceConnected(nDevice))
					continue;

				validPoseCount++;
				mat4DevicePose[nDevice] = ConvertSteamVRMatrixToXMMATRIX(trackedDevicePose[nDevice].mDeviceToAbsoluteTracking);

				if (devClassChar[nDevice] == 0)
				{
					switch (hmd->GetTrackedDeviceClass(nDevice))
					{
					case vr::TrackedDeviceClass_Controller:        devClassChar[nDevice] = 'C'; break;
					case vr::TrackedDeviceClass_HMD:               devClassChar[nDevice] = 'H'; break;
					case vr::TrackedDeviceClass_Invalid:           devClassChar[nDevice] = 'I'; break;
					case vr::TrackedDeviceClass_GenericTracker:    devClassChar[nDevice] = 'G'; break;
					case vr::TrackedDeviceClass_TrackingReference: devClassChar[nDevice] = 'T'; break;
					default:                                       devClassChar[nDevice] = '?'; break;
					}
				}
				strPoseClasses += devClassChar[nDevice];
			}
		}

		if (trackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
		{
			mat4HMDPose = mat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
		}

		////capture steamVR events here
		//vr::VREvent_t event;
		//while (hmd->PollNextEvent(&event, sizeof(event))) 
		//{
		//	switch (event.eventType) {
		//	case vr::VREvent_TrackedDeviceActivated:
		//		wi::backlog::post("Device activated");
		//		break;
		//	case vr::VREvent_TrackedDeviceDeactivated:
		//		wi::backlog::post("Device deactivated");
		//		break;
		//	case vr::VREvent_EnterStandbyMode:
		//		wi::backlog::post("Enter standby mode");
		//		break;
		//	case vr::VREvent_LeaveStandbyMode:
		//		wi::backlog::post("Leave standby mode");
		//		break;
		//	case vr::VREvent_FocusEnter:
		//		wi::backlog::post("Focus Entered");
		//		break;
		//	case vr::VREvent_FocusLeave:
		//		wi::backlog::post("Focus Left");
		//		break;
		//	}
		//}
	}
}

void EngineVrManager::getControllerActions(vr::VRControllerState_t state, int unDevice, float dt)
{
	//if undevice == 1 left or ==2 right	
	if (unDevice == 1 || unDevice == 2)
	{
		if (unDevice == 1)
		{
			controllerVR.controllerDir = CONTROLLER::TOUCHPAD_LEFT;
		}
		else if (unDevice == 2)
		{
			controllerVR.controllerDir = CONTROLLER::TOUCHPAD_RIGHT;
		}

		if (state.rAxis[0].x != 0.0f || state.rAxis[0].y != 0.0f)
		{
			float x = state.rAxis[0].x;
			float y = state.rAxis[0].y;

			if (fabs(x) > 0.7f)
			{
				x = 0.0f;
			}

			if (fabs(y) > 0.7f)
			{
				y = 0.0f;
			}

			controllerVR.axis = XMFLOAT2(state.rAxis[0].x,state.rAxis[0].y);
		}

		if (state.ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)7))//X et A
		{
			controllerVR.butonState = true;
			if (unDevice == 1)
			{
				controllerVR.controller = CONTROLLER::BUTTON_X;
			}
			else
			{
				controllerVR.controller = CONTROLLER::BUTTON_A;
			}
		}

		if (state.ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)2))//Y et B
		{
			controllerVR.butonState = true;
			if (unDevice == 1)
			{
				controllerVR.controller = CONTROLLER::BUTTON_Y;
			}
			else
			{
				controllerVR.controller = CONTROLLER::BUTTON_B;
			}
		}

		if (state.ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)33))//Trigger
		{
			controllerVR.butonState = true;
			if (unDevice == 1)
			{
				controllerVR.controller = CONTROLLER::BUTTON_TRIGGER_LEFT_B;
			}
			else
			{
				controllerVR.controller = CONTROLLER::BUTTON_TRIGGER_RIGHT_B;
			}
		}
	}

	//moveVrFromTouchs(dt);

	controllerVR.axis.x = 0.0f;
	controllerVR.axis.y = 0.0f;
	controllerVR.butonState = false;
	controllerVR.controller = CONTROLLER::NONE;
	controllerVR.controllerDir = CONTROLLER::NONE;
}

//void EngineVrManager::moveVrFromTouchs(float dt)
//{
//code to move camera or other here...
//}

bool EngineVrManager::isLeftPadPressed()
{
	bool returnValue = false;
	if (controllerVR.controllerDir == CONTROLLER::TOUCHPAD_LEFT)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isRightPadPressed()
{
	bool returnValue = false;
	if (controllerVR.controllerDir == CONTROLLER::TOUCHPAD_RIGHT)
	{
		returnValue = true;
	}
	return returnValue;
}

XMFLOAT2 EngineVrManager::getPadValues()
{
	return controllerVR.axis;
}

float EngineVrManager::getPadValueX()
{
	return controllerVR.axis.x;
}

float EngineVrManager::getPadValueY()
{
	return controllerVR.axis.y;
}


bool EngineVrManager::isButtonX()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_X)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonY()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_Y)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonMenu()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_MENU)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonHome()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_HOME)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonA()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_A)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonB()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_B)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonTriggerLeftA()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_TRIGGER_LEFT_A)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonTriggerLeftB()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_TRIGGER_LEFT_B)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonTriggerRightA()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_TRIGGER_RIGHT_A)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isButtonTriggerRightB()
{
	bool returnValue = false;
	if (controllerVR.butonState && controllerVR.controller == CONTROLLER::BUTTON_TRIGGER_RIGHT_B)
	{
		returnValue = true;
	}
	return returnValue;
}

bool EngineVrManager::isVrSessionActive()
{
	return isVrRunning;
}

std::string EngineVrManager::GetTrackedDeviceString(vr::IVRSystem* pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError* peError)
{
	uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, nullptr, 0, peError);
	if (unRequiredBufferLen == 0)
		return "";

	char* pchBuffer = new char[unRequiredBufferLen];
	unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
	std::string sResult = pchBuffer;
	delete[] pchBuffer;
	return sResult;
}

XMMATRIX EngineVrManager::GetHMDMatrixPoseEye(vr::Hmd_Eye nEye)
{
	if (!hmd)
		return XMMATRIX();

	vr::HmdMatrix34_t matEyeRight = hmd->GetEyeToHeadTransform(nEye);

	return ConvertSteamVRMatrixToXMMATRIX(matEyeRight);
}

XMMATRIX EngineVrManager::ConvertSteamVRMatrixToXMMATRIX(const vr::HmdMatrix34_t& matPose)
{
	XMMATRIX matrixObj(
		matPose.m[0][0], matPose.m[1][0], -matPose.m[2][0], 0.0,
		matPose.m[0][1], matPose.m[1][1], -matPose.m[2][1], 0.0,
		-matPose.m[0][2], -matPose.m[1][2], matPose.m[2][2], 0.0,
		matPose.m[0][3], matPose.m[1][3], -matPose.m[2][3], 1.0f
	);

	return matrixObj;
}

XMMATRIX EngineVrManager::GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye)
{
	if (!hmd)
		return XMMATRIX();

	float zNear = 1000.0f;
	float zFar = 0.1f;

	vr::HmdMatrix44_t mat = hmd->GetProjectionMatrix(nEye, zNear, zFar);

	XMMATRIX returnMatrix(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
		-mat.m[0][2], -mat.m[1][2], -mat.m[2][2], -mat.m[3][2],
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
	);

	return returnMatrix;
}

void EngineVrManager::render(float dt)
{
	if (isVrSessionActive())
	{
		wi::scene::CameraComponent* cameraVR = wi::scene::GetScene().cameras.GetComponent(cameraEntityLeft);
		
		XMFLOAT4X4 mpj;
		XMMATRIX finalMatrix= XMMatrixIdentity();

		if (cameraVR != nullptr)
		{
			cameraVR->SetCustomProjectionEnabled(true);

			XMStoreFloat4x4(&mpj, mat4ProjectionLeft);
			cameraVR->Projection = mpj;
			finalMatrix = mat4eyePosLeft * mat4HMDPose * XMLoadFloat4x4(&cameraTransform.world);
			cameraVR->TransformCamera(finalMatrix);
			cameraVR->UpdateCamera();
			cameraVR->SetDirty();
			RenderRt(vr::Hmd_Eye::Eye_Left, dt);
		}

		cameraVR = wi::scene::GetScene().cameras.GetComponent(cameraEntityRight);

		if (cameraVR != nullptr)
		{
			cameraVR->SetCustomProjectionEnabled(true);

			XMStoreFloat4x4(&mpj, mat4ProjectionRight);
			cameraVR->Projection = mpj;
			finalMatrix = mat4eyePosRight * mat4HMDPose * XMLoadFloat4x4(&cameraTransform.world);
			cameraVR->TransformCamera(finalMatrix);
			cameraVR->UpdateCamera();
			cameraVR->SetDirty();
			RenderRt(vr::Hmd_Eye::Eye_Right, dt);
		}

		vr::VRTextureBounds_t bounds;
		bounds.uMin = 0.0f;
		bounds.uMax = 1.0f;
		bounds.vMin = 0.0f;
		bounds.vMax = 1.0f;

		if (dx12)
		{
			wi::graphics::GraphicsDevice_DX12* deviceDx12 = (wi::graphics::GraphicsDevice_DX12*)wi::graphics::GetDevice();
			if (deviceDx12 != nullptr)
			{
				if (rtLeftTexture.IsValid())
				{
					vr::D3D12TextureData_t d3d12LeftEyeTexture = { deviceDx12->GetTextureInternalResource(&rtLeftTexture), deviceDx12->GetGraphicsCommandQueue() , 0 };
					vr::Texture_t leftEyeTexture = { (void*)&d3d12LeftEyeTexture, vr::TextureType_DirectX12, vr::ColorSpace_Gamma };
					vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture, &bounds, vr::Submit_Default);
				}

				if (rtRightTexture.IsValid())
				{
					vr::D3D12TextureData_t d3d12RightEyeTexture = { deviceDx12->GetTextureInternalResource(&rtRightTexture),deviceDx12->GetGraphicsCommandQueue() , 0 };
					vr::Texture_t rightEyeTexture = { (void*)&d3d12RightEyeTexture, vr::TextureType_DirectX12, vr::ColorSpace_Gamma };
					vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture, &bounds, vr::Submit_Default);
				}
				vr::VRCompositor()->PostPresentHandoff();
			}
		}
		else
		{
			wi::graphics::GraphicsDevice_Vulkan* deviceVulkan = (wi::graphics::GraphicsDevice_Vulkan*)wi::graphics::GetDevice();
			if (deviceVulkan != nullptr)
			{
				vr::VRVulkanTextureData_t vulkanData;
				vulkanData.m_pDevice = deviceVulkan->GetDevice();
				vulkanData.m_pPhysicalDevice = deviceVulkan->GetPhysicalDevice();
				vulkanData.m_pInstance = deviceVulkan->GetInstance();
				vulkanData.m_pQueue = deviceVulkan->GetGraphicsCommandQueue();
				vulkanData.m_nQueueFamilyIndex = deviceVulkan->GetGraphicsFamilyIndex();
				vulkanData.m_nWidth = rtLeftTexture.desc.width;
				vulkanData.m_nHeight = rtLeftTexture.desc.height;
				vulkanData.m_nFormat = VK_FORMAT_R8G8B8A8_UNORM;
				vulkanData.m_nSampleCount = 0;

				if (rtLeftTexture.IsValid())
				{
					VkImage imageLeft = deviceVulkan->GetTextureInternalResource(&rtLeftTexture);
					vulkanData.m_nImage = (uint64_t)imageLeft;
					vr::Texture_t leftEyeTexture = { &vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Gamma };
					vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture, &bounds);
				}

				if (rtRightTexture.IsValid())
				{
					VkImage imageRight = deviceVulkan->GetTextureInternalResource(&rtRightTexture);
					vulkanData.m_nImage = (uint64_t)(imageRight);
					vr::Texture_t rightEyeTexture = { &vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Gamma };
					vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture, &bounds);
				}
				vr::VRCompositor()->PostPresentHandoff();
			}
		}

		EngineVrManager::getInstance()->updateVrSession(dt);
	}
}

void EngineVrManager::RenderRt(vr::Hmd_Eye nEye, float dt)
{

	if (nEye == vr::Hmd_Eye::Eye_Left)
	{
		renderPathLeft.camera = wi::scene::GetScene().cameras.GetComponent(cameraEntityLeft);
		renderPathLeft.setSceneUpdateEnabled(true);
		renderPathLeft.setOcclusionCullingEnabled(false);
		renderPathLeft.PreUpdate();
		renderPathLeft.Update(dt);
		renderPathLeft.PostUpdate();
		renderPathLeft.Render();
		rtLeftTexture = resizeImage(*renderPathLeft.lastPostprocessRT, widthTexture, heightTexture);
	}
	else
	{
		renderPathRight.camera = wi::scene::GetScene().cameras.GetComponent(cameraEntityRight);
		renderPathRight.setSceneUpdateEnabled(false);
		renderPathRight.setOcclusionCullingEnabled(false);
		renderPathRight.PreUpdate();
		renderPathRight.Update(dt);
		renderPathRight.PostUpdate();
		renderPathRight.Render();
		rtRightTexture = resizeImage(*renderPathRight.lastPostprocessRT, widthTexture, heightTexture);
	}
}

wi::graphics::Texture EngineVrManager::resizeImage(const wi::graphics::Texture& image, int width, int height)
{
	wi::graphics::Texture renderTargetResize = {};

	if (image.IsValid())
	{
		wi::graphics::GraphicsDevice* device = wi::graphics::GetDevice();
		wi::graphics::RenderPass resizeTexture;

		wi::graphics::TextureDesc desc;
		desc.width = width;
		desc.height = height;
		desc.format = wi::graphics::Format::R8G8B8A8_UNORM;
		desc.bind_flags = wi::graphics::BindFlag::RENDER_TARGET;// | BindFlag::SHADER_RESOURCE;
		if (device->CreateTexture(&desc, nullptr, &renderTargetResize))
		{
			wi::graphics::RenderPassDesc desc;
			desc.attachments.push_back(wi::graphics::RenderPassAttachment::RenderTarget(renderTargetResize, wi::graphics::RenderPassAttachment::LoadOp::CLEAR));
			if (device->CreateRenderPass(&desc, &resizeTexture))
			{
				desc.attachments[0].texture = renderTargetResize;
			}
		}

		wi::graphics::CommandList cmd = device->BeginCommandList();

		device->EventBegin("ResizeTexture", cmd);

		wi::graphics::Viewport vp;
		vp.width = (float)width;
		vp.height = (float)height;

		wi::image::Params fx;
		fx.enableFullScreen();

		device->BindViewports(1, &vp, cmd);
		device->RenderPassBegin(&resizeTexture, cmd);
		wi::image::Draw(&image, fx, cmd);
		device->RenderPassEnd(cmd);

		device->EventEnd(cmd);

		device->SubmitCommandLists();
	}

	return renderTargetResize;
}
