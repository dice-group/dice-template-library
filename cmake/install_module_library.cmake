include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(install_module_library TARGET_NAME NAMESPACE LIBRARY_NAME INCLUDE_PATH)

    target_include_directories(
            ${TARGET_NAME} PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    set_property(TARGET ${TARGET_NAME} PROPERTY EXPORT_NAME ${LIBRARY_NAME})

    install(TARGETS ${TARGET_NAME} ${ARGN}
            EXPORT ${TARGET_NAME}-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILE_SET CXX_MODULES DESTINATION modules
    )

    write_basic_package_version_file("${TARGET_NAME}-config-version.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY ExactVersion)

    configure_package_config_file(
            "${PROJECT_SOURCE_DIR}/cmake/lib-config.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}-config.cmake"
            INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${TARGET_NAME}/cmake)
    # here we have two possibilities: either CMAKE_INSTALL_DATAROOTDIR (share) or CMAKE_INSTALL_LIBDIR (lib/lib64)
    # we just have to be consistent for one target

    install(
            EXPORT ${TARGET_NAME}-targets
            FILE ${TARGET_NAME}-targets.cmake
            NAMESPACE ${NAMESPACE}::
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

    install(FILES "${PROJECT_BINARY_DIR}/${TARGET_NAME}-config.cmake"
            "${PROJECT_BINARY_DIR}/${TARGET_NAME}-config-version.cmake"
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_PATH}/
            DESTINATION include
            FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
endfunction()
