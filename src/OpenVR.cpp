#include "GL/glew.h"

#include "OpenVR_backend.h"

#include "openvr.h"
#include "MathUtils.h"

#include <iostream>
#include <assert.h>
#include <cstdio>
#include <string>

typedef enum eLibStatus
{
	LIB_UNLOADED = 0,
	LIB_FAILED,
	LIB_INITIALIZED,
} eLibStatus;

class DllExport OpenVRImpl : public BackendImpl
{
public:
	friend class OpenVR;

	OpenVRImpl();
	~OpenVRImpl();

	bool setup(const unsigned int color_texture_left, const unsigned int color_texture_right);

	bool update(float *r_orientation_left, float *r_position_left, float *r_orientation_right, float *r_position_right);

	bool update(
		float *r_yaw_left, float *r_pitch_left, float *r_roll_left, float *r_position_left,
		float *r_yaw_right, float *r_pitch_right, float *r_roll_right, float *r_position_right);

	bool update(
		float *r_yaw_left, float *r_pitch_left, float *r_roll_left, float *r_orientation_left, float *r_position_left,
		float *r_yaw_right, float *r_pitch_right, float *r_roll_right, float *r_orientation_right, float *r_position_right);

	bool update(const bool is_right_hand, float *r_matrix_left, float *r_matrix_right);

	bool frameReady(void);

	bool reCenter(void);

	void getProjectionMatrixLeft(const float nearz, const float farz, const bool is_opengl, const bool is_right_hand, float *r_matrix);

	void getProjectionMatrixRight(const float nearz, const float farz, const bool is_opengl, const bool is_right_hand, float *r_matrix);

private:
	bool isConnected(void);
	unsigned int getProjectionMatrixFlags(const bool is_opengl, const bool is_right_hand);
	bool initializeLibrary(void);
	bool initializeCompositor(void);
	bool initializeOverlay(void);
	void SetupCameras();

	Matrix4 GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye);
	Matrix4 GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye, float fNearClip, float fFarClip, bool isOpenGL);

	Matrix4	GetHMDMatrixPoseEye(vr::Hmd_Eye nEye);
	Matrix4 ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix34_t &matPose);
	Matrix4 GetCurrentViewProjectionMatrix(vr::Hmd_Eye nEye);
	void UpdateHMDMatrixPose(void);
	void UpdateHMDPositionalInfo(void);
	void ProcessVREvent(const vr::VREvent_t & event);

public:
	static eLibStatus m_lib_status;

private:
	float m_fNearClip;
	float m_fFarClip;
	unsigned int m_frame;
	vr::IVRSystem *m_pHMDy;

	Matrix4 m_mat4eyeProjection[2];
	Matrix4 m_mat4eyePos[2];
	vr::IVRRenderModels *m_pRenderModels;
	vr::IVRCompositor *m_pCompositor;
	vr::VROverlayHandle_t m_OverlayHandle;
	Matrix4 m_rMat4TrackedPose[vr::k_unMaxTrackedDeviceCount];
	Matrix4 m_rmat4DevicePose[ vr::k_unMaxTrackedDeviceCount ];
	Matrix4 m_mat4HMDPose;
	Quaternion *m_hmdRotation;
	Vector3 *m_hmdPosition;

	// Update() Members
	Matrix4 m_mat_view_left;
	Matrix4 m_mat_view_right;
	Quaternion m_q_left;
	Quaternion m_q_right;
	Vector3 m_v3_left;
	Vector3 m_v3_right;

	char m_rDevClassChar[vr::k_unMaxTrackedDeviceCount];   // for each device, a character representing its class
	vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
	GLuint m_fbo[2];
};

eLibStatus OpenVRImpl::m_lib_status = LIB_UNLOADED;

bool OpenVRImpl::initializeLibrary()
{
	vr::EVRInitError eError = vr::VRInitError_None;

	switch (OpenVRImpl::m_lib_status) {
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

	// Loading the SteamVR Runtime
	m_pHMDy = vr::VR_Init(&eError, vr::VRApplication_Scene);

	if (eError != vr::VRInitError_None)
	{
		m_pHMDy = NULL;
		char buf[1024];
		sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		// SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VR_Init Failed", buf, NULL);
		return false;
	}
	return true;
}


bool OpenVRImpl::initializeCompositor() 
{
	this->m_pCompositor = vr::VRCompositor();

	if (!this->m_pCompositor)
	{
		return false;
	}
	return true;
}

bool OpenVRImpl::initializeOverlay()
{
	vr::EVROverlayError peError = vr::VROverlayError_None;

	if (vr::VROverlay())
	{
		peError = vr::VROverlay()->CreateOverlay("cdBridgeLibOverlay", "Blender HMD Bridge Overlay", &m_OverlayHandle);

		if (peError == vr::VROverlayError_None) return true;
		else 
		{
			std::string s = std::to_string((int)peError);
			std::cout << "Error: VROverlayError: #" << s << std::endl ;
			return false;
		}
	}
}

void OpenVRImpl::SetupCameras(void)
{
	m_mat4eyeProjection[0] = GetHMDMatrixProjectionEye(vr::Eye_Left).invert();
	m_mat4eyeProjection[1] = GetHMDMatrixProjectionEye(vr::Eye_Right).invert();
	m_mat4eyePos[0] = GetHMDMatrixPoseEye(vr::Eye_Left).invert();
	m_mat4eyePos[1] = GetHMDMatrixPoseEye(vr::Eye_Right).invert();
}


Matrix4 OpenVRImpl::ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix34_t &matPose)
{
	Matrix4 matrixObj(
		matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
		matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
		matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
		matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
		);
	return matrixObj;
}

void OpenVRImpl::UpdateHMDMatrixPose()
{
	if (!m_pHMDy)
		return;

	this->m_pCompositor->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

	int m_iValidPoseCount = 0;
	std::string m_strPoseClasses = "";
	

	for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
	{
		if (m_rTrackedDevicePose[nDevice].bPoseIsValid)
		{
			m_iValidPoseCount++;
			m_rmat4DevicePose[nDevice] = ConvertSteamVRMatrixToMatrix4(m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
			if (m_rDevClassChar[nDevice] == 0)
			{
				switch (m_pHMDy->GetTrackedDeviceClass(nDevice))
				{
				case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
				case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
				case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
				case vr::TrackedDeviceClass_GenericTracker:    m_rDevClassChar[nDevice] = 'G'; break;
				case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
				default:                                       m_rDevClassChar[nDevice] = '?'; break;
				}
			}
			m_strPoseClasses += m_rDevClassChar[nDevice];
		}
	}

	if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
	{
		m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
	}
}

void OpenVRImpl::UpdateHMDPositionalInfo() 
{
	// The following call modifies m_hmdRotation
	MatrixHelper::GetRotation(*m_hmdRotation, m_mat4HMDPose);

	// The following call modifies m_hmdPosition
	MatrixHelper::GetPosition(*m_hmdPosition, m_mat4HMDPose);
}

void OpenVRImpl::ProcessVREvent(const vr::VREvent_t & event)
{
	switch (event.eventType)
	{
	case vr::VREvent_TrackedDeviceActivated:
	{
		this->setStatus("A Device was Activated.\n");
		this->setStateBool(false);  // To indicate Dirty.
	}
	break;
	case vr::VREvent_TrackedDeviceDeactivated:
	{
		this->setStatus("A Device was Deactivated.\n");
		this->setStateBool(false);  // To indicate Dirty.
	}
	break;
	case vr::VREvent_TrackedDeviceUpdated:
	{
		this->setStatus("A Device was Updated.\n");
		this->setStateBool(false);  // To indicate Dirty.
	}
	break;
	}
}

OpenVRImpl::OpenVRImpl()
	:BackendImpl()
	, m_fNearClip(0.01f)
	, m_fFarClip(100.0f)
{
	/* we need glew to access opengl commands */
	glewInit();


	/* initialize data */
	this->m_frame = -1;
	this->m_fbo[0] = 0;
	this->m_fbo[1] = 0;

	// ovr_GetFovTextureSize on a DK2 is 1182x1464 - hardcode for now.
	this->m_width[0] = 1182;
	this->m_width[1] = 1182;
	this->m_height[0] = 1464;
	this->m_height[1] = 1464;

	// Heap Elements 
	m_hmdRotation = new Quaternion();
	m_hmdPosition = new Vector3();

	/* We set to true and then check some things.  Each failed step is to set this to false */
	this->setStateBool(true);

	/* Make sure the library is loaded */
	if (this->initializeLibrary() == false) {
		
		this->setStatus ( "Error: OpenVR library could not initialize. Is HMD powered on?" );
		this->setStateBool(false);

		std::cout << this->getStatus() << std::endl;
		return;   // Fix: blender crash if we proceed from here.
	}

	if (this->initializeCompositor() == false) {
		this->setStatus("Compositor initialization failed.\n");
		this->setStateBool(false);

		std::cout << this->getStatus() << std::endl;
		return;
	}

	// TODO: There is no case where this could be false and code is still executing here. Refactor.
	if (this->getStateBool())
	{
		this->setStatus("OpenVR properly initialized. Make sure your lighthouses are running or you will get a blank screen. \n");
		std::cout << "OpenVR properly initialized (" << m_width[0] << "x" << m_height[0] << ", " << m_width[1] << "x" << m_height[1] << ")" << std::endl;
	}


	// Setup the Cameras.
	SetupCameras();

	/** Sets tracking space returned by WaitGetPoses */
	this->m_pCompositor->SetTrackingSpace(vr::ETrackingUniverseOrigin::TrackingUniverseStanding);

	return;
}

OpenVRImpl::~OpenVRImpl()
{
	std::cout << "~OpenVRImpl" << std::endl;
	// Clean up heap
	delete m_hmdRotation;
	delete m_hmdPosition;
}

bool OpenVRImpl::isConnected()
{
	// TODO
	return false;
}

bool OpenVRImpl::setup(const unsigned int color_texture_left, const unsigned int color_texture_right)
{
	this->m_color_texture[0] = color_texture_left;
	this->m_color_texture[1] = color_texture_right;

	// Focus
	this->m_pCompositor->CompositorBringToFront();


	std::cout << "OpenVR setup() inside BridgeLib.dll complete." << std::endl;
	return true;
};

bool OpenVRImpl::update(float *r_orientation_left, float *r_position_left, float *r_orientation_right, float *r_position_right)
{
	this->UpdateHMDMatrixPose();

	this->m_mat_view_left = m_mat4HMDPose * m_mat4eyePos[0];
	this->m_mat_view_right = m_mat4HMDPose * m_mat4eyePos[1];
	
	MatrixHelper::CalculateRotation(this->m_q_left, this->m_mat_view_left);
	MatrixHelper::GetPosition(this->m_v3_left, this->m_mat_view_left);

	MatrixHelper::CalculateRotation(this->m_q_right, this->m_mat_view_right);
	MatrixHelper::GetPosition(this->m_v3_right, this->m_mat_view_right);

	Quaternion *orientation_in[2] = { &this->m_q_left, &this->m_q_right };
	Vector3 *position_in[2] = { &this->m_v3_left, &this->m_v3_right };

	float *orientation_out[2] = { r_orientation_left, r_orientation_right };
	float *position_out[2] = { r_position_left, r_position_right };


	for (int eye = 0; eye < 2; eye++) {
		orientation_out[eye][0] = orientation_in[eye]->w;
		orientation_out[eye][1] = orientation_in[eye]->x;
		orientation_out[eye][2] = orientation_in[eye]->y;
		orientation_out[eye][3] = orientation_in[eye]->z;

		position_out[eye][0] = this->m_scale * position_in[eye]->x;
		position_out[eye][1] = this->m_scale * position_in[eye]->y;
		position_out[eye][2] = this->m_scale * position_in[eye]->z;

	}

	return true;
};

bool OpenVRImpl::update(
	float *r_yaw_left, float *r_pitch_left, float *r_roll_left, float *r_position_left,
	float *r_yaw_right, float *r_pitch_right, float *r_roll_right, float *r_position_right)
{
	// TODO
	return false;
};

bool OpenVRImpl::update(
	float *r_yaw_left, float *r_pitch_left, float *r_roll_left, float *r_orientation_left, float *r_position_left,
	float *r_yaw_right, float *r_pitch_right, float *r_roll_right, float *r_orientation_right, float *r_position_right)
{
	// TODO
	return false;
}

bool OpenVRImpl::update(const bool is_right_hand, float *r_matrix_left, float *r_matrix_right)
{
	this->UpdateHMDMatrixPose();
	// m_mat4HMDPose should now be updated.

	// left
	Matrix4 mat_left = this->GetHMDMatrixProjectionEye(vr::EVREye::Eye_Left);
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				r_matrix_left[i * 4 + j] = mat_left[i * 4 + j];
	
	// right
	Matrix4 mat_right = this->GetHMDMatrixProjectionEye(vr::EVREye::Eye_Right);
		for (int k = 0; k < 4; k++)
			for (int l = 0; l < 4; l++)
				r_matrix_right[k * 4 + l] = mat_right[k * 4 + l];

	return true;
}

bool OpenVRImpl::frameReady()
{
	// First check for device changes.. controllers turned off/on etc and set dirty state if changes.
	vr::VREvent_t event;
	while (m_pHMDy->PollNextEvent(&event, sizeof(event)))
	{
		ProcessVREvent(event);
	}


	// We must call WaitGetPoses before the Compositor gets focus
	// The following member function does this...
	this->UpdateHMDMatrixPose();

	if (!this->m_pCompositor->CanRenderScene()) {
		// this->setStatus("CanRenderScene() returned false");
		std::cout << "CanRenderScene() returned false" << std::endl;
		return false;
	}
	vr::EVRCompositorError err = vr::VRCompositorError_None;

	/* Renders the Frame */
	for (int eye = 0; eye < 2; eye++)
	{
		vr::EVREye eEye = (vr::EVREye)eye;

		vr::Texture_t texture = { (void*)this->m_color_texture[eye], vr::TextureType_OpenGL, vr::ColorSpace_Auto };

		vr::VRTextureBounds_t bounds;
		bounds.uMin = (eye == 0) ? 0.0f : 0.0f;
		bounds.uMax = (eye == 0) ? 1.0f : 1.0f;
		bounds.vMin = 0.f;
		bounds.vMax = 1.f;

		err = this->m_pCompositor->Submit(eEye, &texture, &bounds);

		if (err != vr::VRCompositorError_None)
		{
			return false;
		}
	}

	return true;
};

bool OpenVRImpl::reCenter()
{
	m_pHMDy->ResetSeatedZeroPose();
	return true;
};

void OpenVRImpl::getProjectionMatrixLeft(const float nearz, const float farz, const bool is_opengl, const bool is_right_hand, float *r_matrix)
{
	this->m_fNearClip = nearz;
	this->m_fFarClip = farz;

	Matrix4 mat = this->GetHMDMatrixProjectionEye(vr::EVREye::Eye_Left, nearz, farz, is_opengl).get();

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			r_matrix[i * 4 + j] = mat[i * 4 + j];
}

void OpenVRImpl::getProjectionMatrixRight(const float nearz, const float farz, const bool is_opengl, const bool is_right_hand, float *r_matrix)
{
	this->m_fNearClip = nearz;
	this->m_fFarClip = farz;

	Matrix4 mat = this->GetHMDMatrixProjectionEye(vr::EVREye::Eye_Right, nearz, farz, is_opengl).get();

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			r_matrix[i * 4 + j] = mat[i * 4 + j];
}

unsigned int OpenVRImpl::getProjectionMatrixFlags(const bool is_opengl, const bool is_right_hand)
{
	// TODO
	return 0;
}

Matrix4 OpenVRImpl::GetCurrentViewProjectionMatrix(vr::Hmd_Eye nEye)
{
	Matrix4 matMVP;
	if (nEye == vr::Eye_Left)
	{
		matMVP = m_mat4eyeProjection[0] * m_mat4eyePos[0] * m_mat4HMDPose;
	}
	else if (nEye == vr::Eye_Right)
	{
		matMVP = m_mat4eyeProjection[1] * m_mat4eyePos[1] *  m_mat4HMDPose;
	}

	return matMVP;
}

Matrix4 OpenVRImpl::GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye)
{
	return this->GetHMDMatrixProjectionEye(nEye, m_fNearClip, m_fFarClip, true);
}

Matrix4 OpenVRImpl::GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye, float fNearClip, float fFarClip, bool isOpenGL)
{
	if (!m_pHMDy)
		return Matrix4();

	vr::HmdMatrix44_t mat = m_pHMDy->GetProjectionMatrix(nEye, fNearClip, fFarClip);

	return Matrix4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
		);
}

Matrix4 OpenVRImpl::GetHMDMatrixPoseEye(vr::Hmd_Eye nEye)
{
	if (!m_pHMDy)
		return Matrix4();

	vr::HmdMatrix34_t matEyeRight = m_pHMDy->GetEyeToHeadTransform(nEye);
	Matrix4 matrixObj(
		matEyeRight.m[0][0], matEyeRight.m[1][0], matEyeRight.m[2][0], 0.0,
		matEyeRight.m[0][1], matEyeRight.m[1][1], matEyeRight.m[2][1], 0.0,
		matEyeRight.m[0][2], matEyeRight.m[1][2], matEyeRight.m[2][2], 0.0,
		matEyeRight.m[0][3], matEyeRight.m[1][3], matEyeRight.m[2][3], 1.0f
		);

	return matrixObj.invert();
}

void OpenVR::initializeImplementation() {
	m_me = new OpenVRImpl();
}