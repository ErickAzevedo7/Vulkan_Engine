#pragma once

// CPU zones and frame marks for the Tracy profiler. The rest of the engine uses these macros
// and never includes Tracy itself; built without VKE_PROFILING they compile to nothing.
//
//   void Scene::Update(f32 deltaTime) {
//       VKE_PROFILE_FUNCTION();
//       ...
//       {
//           VKE_PROFILE_SCOPE("Physics");
//           m_Physics.Step();
//       }
//   }

#ifdef VKE_PROFILING

#include <tracy/Tracy.hpp>

// Ends a frame in the profiler's timeline. Once per pass of the main loop.
#define VKE_PROFILE_FRAME() FrameMark
// Times the rest of the enclosing scope as a zone named after the function.
#define VKE_PROFILE_FUNCTION() ZoneScoped
// Times the rest of the enclosing scope as a zone named by a string literal.
#define VKE_PROFILE_SCOPE(name) ZoneScopedN(name)
// Same, with a name known only at run time: anything with data() and size().
#define VKE_PROFILE_SCOPE_DYNAMIC(text) \
	ZoneScoped; \
	ZoneName((text).data(), (text).size())

#else

#define VKE_PROFILE_FRAME()
#define VKE_PROFILE_FUNCTION()
#define VKE_PROFILE_SCOPE(name)
#define VKE_PROFILE_SCOPE_DYNAMIC(text)

#endif
