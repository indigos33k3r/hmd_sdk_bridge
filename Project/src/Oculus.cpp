#include "Bridge.h"
#include "Oculus.h"
#include "HMD.h"
#include "OVR_CAPI_0_7_0.h"

#include <GL/CAPI_GLE.h>
#include "OVR_CAPI_GL.h"

#define MAX(a,b) a > b ? a : b;

eLibStatus Oculus::m_lib_status = LIB_UNLOADED;

bool Oculus::initializeLibrary()
{
	switch (Oculus::m_lib_status) {
	case LIB_FAILED:
		return false;
		break;
	case LIB_INITIALIZED:
		return true;
		break;
	case LIB_UNLOADED:
	default:
		/* try to load the library */
		break;
	}

	ovrResult result;
	result = ovr_Initialize(nullptr);

	if (OVR_FAILURE(result)) {
		Oculus::m_lib_status = LIB_FAILED;
		return false;
	}
	else {
		Oculus::m_lib_status = LIB_INITIALIZED;
		return true;
	}
}

Oculus::Oculus() :HMD()
{
	/* Make sure the library is loaded */
	if (Oculus::initializeLibrary() == false)
		throw "libOVR could not initialize";

	ovrHmd hmd;
	ovrGraphicsLuid luid;

	/* initialize the device */
	ovrResult result = ovr_Create(&hmd, &luid);
	if (OVR_FAILURE(result)) {
		ovr_Shutdown();
		throw "Oculus could not initialize";
	}

	ovrHmdDesc desc = ovr_GetHmdDesc(hmd);
	ovrSizei resolution = desc.Resolution;

	/* setup tracking system */
	ovr_ConfigureTracking(
		hmd,
		ovrTrackingCap_Orientation |
		ovrTrackingCap_MagYawCorrection |
		ovrTrackingCap_Position,
		0);

	ovrSizei recommendedTex0Size = ovr_GetFovTextureSize(hmd, ovrEye_Left, desc.DefaultEyeFov[0], 1.0f);
	ovrSizei recommendedTex1Size = ovr_GetFovTextureSize(hmd, ovrEye_Right, desc.DefaultEyeFov[1], 1.0f);

	/* store data */
	this->m_hmd = hmd;
	this->m_desc = desc;
	this->m_eyeRenderDesc[0] = ovr_GetRenderDesc(hmd, ovrEye_Left, desc.DefaultEyeFov[0]);
	this->m_eyeRenderDesc[1] = ovr_GetRenderDesc(hmd, ovrEye_Right, desc.DefaultEyeFov[1]);
	this->m_hmdToEyeViewOffset[0] = this->m_eyeRenderDesc[0].HmdToEyeViewOffset;
	this->m_hmdToEyeViewOffset[1] = this->m_eyeRenderDesc[1].HmdToEyeViewOffset;
	this->m_frame = -1;
	this->m_width[0] = recommendedTex0Size.w;
	this->m_height[0] = recommendedTex0Size.h;
	this->m_width[1] = recommendedTex1Size.w;
	this->m_height[1] = recommendedTex1Size.h;
}

Oculus::~Oculus()
{
	ovr_DestroySwapTextureSet(this->m_hmd, this->m_textureSet[0]);
	ovr_DestroySwapTextureSet(this->m_hmd, this->m_textureSet[1]);
	ovr_Destroy(this->m_hmd);
	ovr_Shutdown();
}

bool Oculus::isConnected()
{
	/* Make sure the library is loaded */
	if (Oculus::initializeLibrary() == false)
		throw "libOVR could not initialize";

	ovrHmdDesc desc = ovr_GetHmdDesc(nullptr);
	if (desc.AvailableHmdCaps) {
		return true;
	}
	else {
		return false;
	}
}

bool Oculus::setup(unsigned int framebuffer_object_left, unsigned int framebuffer_object_right)
{
	ovrResult result;
	return true; /* TODO */

	/* TODO */

	ovrSizei bufferSize;
	bufferSize.w = this->m_width[0] + this->m_width[1];
	bufferSize.h = MAX(this->m_height[0], this->m_height[1]);

	result = ovr_CreateSwapTextureSetGL(this->m_hmd, GL_SRGB8_ALPHA8, this->m_width[0], this->m_height[0], &this->m_textureSet[0]);

	if (result != ovrSuccess)
		return false;

	result = ovr_CreateSwapTextureSetGL(this->m_hmd, GL_SRGB8_ALPHA8, this->m_width[1], this->m_height[1], &this->m_textureSet[1]);

	if (result != ovrSuccess)
		return false;

	ovrLayerEyeFov layer;
	layer.Header.Type = ovrLayerType_EyeFov;
	layer.Header.Flags = 0;
	layer.ColorTexture[0] = this->m_textureSet[0];
	layer.ColorTexture[1] = this->m_textureSet[1];
	layer.Fov[0] = this->m_eyeRenderDesc[0].Fov;
	layer.Fov[1] = this->m_eyeRenderDesc[1].Fov;
	layer.Viewport[0].Pos.x = 0;
	layer.Viewport[0].Pos.y = 0;
	layer.Viewport[0].Size.w = bufferSize.w / 2;
	layer.Viewport[0].Size.h = bufferSize.h / 2;
	layer.Viewport[1].Pos.x = bufferSize.w / 2;
	layer.Viewport[1].Pos.y = 0;
	layer.Viewport[1].Size.w = bufferSize.w / 2;
	layer.Viewport[1].Size.h = bufferSize.h / 2;
	// layer.RenderPose is updated later per frame

	/* store data */
	this->m_framebuffer_object[0] = framebuffer_object_left;
	this->m_framebuffer_object[1] = framebuffer_object_right;
	this->m_layer = layer;

	return true;
};

bool Oculus::update(float *r_head_transform[4][4], float *r_eye_left[3], float* r_eye_right[3])
{
	/* Get both eye poses simultaneously, with IPD offset already included */
	ovrFrameTiming ftiming = ovr_GetFrameTiming(this->m_hmd, ++this->m_frame);
	ovrTrackingState hmdState = ovr_GetTrackingState(this->m_hmd, ftiming.DisplayMidpointSeconds);

	if ((hmdState.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked)) != 0) {
		ovr_CalcEyePoses(hmdState.HeadPose.ThePose, this->m_hmdToEyeViewOffset, this->m_layer.RenderPose);

		this->m_layer.RenderPose[0].Position;
		this->m_layer.RenderPose[0].Orientation;
		this->m_layer.RenderPose[1].Position;
		this->m_layer.RenderPose[1].Orientation;

		/* TODO */

		return true;
	}
	return false;
};

bool Oculus::frameReady()
{
	ovrLayerHeader *layers = &this->m_layer.Header;
	ovrResult result = ovr_SubmitFrame(this->m_hmd, 0, nullptr, &layers, this->m_frame);

	if (ovrSuccess == result) {
		return true;
	}
	else{
		return false;
	}
};

bool Oculus::reCenter()
{
	ovr_RecenterPose(this->m_hmd);
	return true;
};

void Oculus::getProjectionMatrixLeft(const float nearz, const float farz, float *r_matrix[4][4])
{
	ovrMatrix4f matrix = ovrMatrix4f_Projection(
		this->m_eyeRenderDesc[0].Fov,
		nearz,
		farz,
		ovrProjection_RightHanded);

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			*r_matrix[i][j] = matrix.M[i][j];
}

void Oculus::getProjectionMatrixRight(const float nearz, const float farz, float *r_matrix[4][4])
{
	ovrMatrix4f matrix = ovrMatrix4f_Projection(
		this->m_eyeRenderDesc[1].Fov,
		nearz,
		farz,
		ovrProjection_RightHanded);

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			*r_matrix[i][j] = matrix.M[i][j];
}