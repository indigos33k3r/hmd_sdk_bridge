#include "HMD.h"
#include "Oculus.h"

extern "C" {
	/* Oculus wrapper */
	Oculus *Oculus_new(){ return new Oculus(); }
	bool Oculus_isConnected(){ return Oculus::isConnected(); }
	bool Oculus_setup(Oculus *oculus, unsigned int framebuffer_object){ return oculus->setup(framebuffer_object); }
	bool Oculus_update(Oculus *oculus, float r_head_transform[4][4], float r_eye_left[3], float r_eye_right[3]) { return oculus->update(r_head_transform, r_eye_left, r_eye_right); }
	bool Oculus_frameReady(Oculus *oculus) { return oculus->frameReady(); }
	bool Oculus_reCenter(Oculus *oculus) { return oculus->reCenter(); }

	unsigned int Oculus_width(Oculus *oculus) { return oculus->getWidth(); }
	unsigned int Oculus_height(Oculus *oculus) { return oculus->getHeight(); }
	void Oculus_projectionMatrixLeft(Oculus *oculus, const float near, const float far, float *r_matrix[4][4]) { return oculus->getProjectionMatrixLeft(near, far, r_matrix); }
	void Oculus_projectionMatrixRight(Oculus *oculus, const float near, const float far, float *r_matrix[4][4]) { return oculus->getProjectionMatrixRight(near, far, r_matrix); }
}