include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(install_interface_library TARGET_NAME NAMESPACE LIBRARY_NAME INCLUDE_PATH)

  target_include_directories(
    ${TARGET_NAME} INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

  set_property(TARGET ${TARGET_NAME} PROPERTY EXPORT_NAME ${LIBRARY_NAME})

  install(
    TARGETS ${TARGET_NAME}
    EXPORT ${TARGET_NAME}-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


  write_basic_package_version_file(
    "${TARGET_NAME}-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

  configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/lib-config.cmake.in"
    "${PROJECT_BINARY_DIR}/${TARGET_NAME}-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

  install(
    EXPORT ${TARGET_NAME}-targets
    FILE ${TARGET_NAME}-targets.cmake
    NAMESPACE ${NAMESPACE}::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

  install(FILES "${PROJECT_BINARY_DIR}/${TARGET_NAME}-config.cmake"
                "${PROJECT_BINARY_DIR}/${TARGET_NAME}-config-version.cmake"
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

  install(DIRECTORY ${PROJECT_SOURCE_DIR}/${INCLUDE_PATH}/ DESTINATION include)
endfunction()
