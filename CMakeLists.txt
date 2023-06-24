cmake_minimum_required(VERSION 3.10)

project(neurio
	VERSION 0.1
    DESCRIPTION "Neurio CT Sensor Interface"
)

include(GNUInstallDirs)

find_library ( LIB_RT rt REQUIRED )
find_library ( LIB_CURL curl REQUIRED )

add_executable( ${PROJECT_NAME}
	src/neurio.c
)

target_link_libraries( ${PROJECT_NAME}
    ${LIB_RT}
    varserver
    ${LIB_CURL}
    tjson
)

set_target_properties( ${PROJECT_NAME}
    PROPERTIES OUTPUT_NAME neurio
)

target_include_directories( ${PROJECT_NAME} PRIVATE
	.
	inc
	${CMAKE_BINARY_DIR} )

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )