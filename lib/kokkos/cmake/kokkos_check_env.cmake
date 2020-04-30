SET(CRAYPE_VERSION $ENV{CRAYPE_VERSION})
IF (CRAYPE_VERSION)
  SET(KOKKOS_IS_CRAYPE TRUE)
  SET(CRAYPE_LINK_TYPE $ENV{CRAYPE_LINK_TYPE})
  IF (CRAYPE_LINK_TYPE)
    IF (NOT CRAYPE_LINK_TYPE STREQUAL "dynamic")
      MESSAGE(WARNING "CRAYPE_LINK_TYPE is set to ${CRAYPE_LINK_TYPE}. Linking is likely to fail unless this is set to 'dynamic'")
    ENDIF()
  ELSE()
      MESSAGE(WARNING "CRAYPE_LINK_TYPE is not set. Linking is likely to fail unless this is set to 'dynamic'")
  ENDIF()
ENDIF()
