include(${TINYREFL_SOURCE_DIR}/cmake/externals.cmake)

if(NOT TINYREFL_CTTI_REPO_URL)
    set(TINYREFL_CTTI_REPO_URL https://gitlab.com/Manu343726/ctti.git)
endif()
if(NOT TINYREFL_CTTI_VERSION)
    set(TINYREFL_CTTI_VERSION master)
endif()
if(NOT TINYREFL_JSON_REPO_URL)
    set(TINYREFL_JSON_REPO_URL https://github.com/nlohmann/json.git)
endif()
if(NOT TINYREFL_JSON_VERSION)
    set(TINYREFL_JSON_VERSION v3.1.2)
endif()

set(JSON_BuildTests OFF CACHE BOOL "")

external_dependency(ctti ${TINYREFL_CTTI_REPO_URL} ${TINYREFL_CTTI_VERSION})
external_dependency(nlohmann_json ${TINYREFL_JSON_REPO_URL} ${TINYREFL_JSON_VERSION})
