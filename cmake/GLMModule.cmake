find_package(glm CONFIG REQUIRED)

get_target_property(GLM_INCLUDE_DIR glm::glm-header-only INTERFACE_INCLUDE_DIRECTORIES)

add_library(glm-module)

target_compile_features(glm-module PUBLIC cxx_std_23)
target_sources(glm-module
    PUBLIC 
        FILE_SET CXX_MODULES 
        BASE_DIRS "${GLM_INCLUDE_DIR}"
        FILES "${GLM_INCLUDE_DIR}/glm/glm.cppm"
)

target_link_libraries(glm-module PRIVATE glm::glm)