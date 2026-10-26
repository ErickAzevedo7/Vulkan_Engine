# The reflection generator. It runs during the build, so it is built before the engine.
add_executable(VKE_Reflect
    Main.cpp
    HeaderParser.cpp
    CodeWriter.cpp
)

set_target_properties(VKE_Reflect PROPERTIES FOLDER "Tools")
