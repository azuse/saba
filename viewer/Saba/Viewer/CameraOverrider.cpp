#include "CameraOverrider.h"

namespace mmd
{
	CameraOverrider::~CameraOverrider()
	{
	}

	void CameraOverrider::Override(ViewerContext* ctxt, Camera* camera)
	{
		OnOverride(ctxt, camera);
	}
}

