add_library(VKE_Reflection INTERFACE)

# Only the marks the reflection generator reads; no code, no dependencies.
target_include_directories(VKE_Reflection
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/public
)

set_target_properties(VKE_Reflection PROPERTIES FOLDER "Engine")
