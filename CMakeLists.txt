# This is the root ITK CMakeLists file.
CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

PROJECT(PBNRR)


# Find ITK.
FIND_PACKAGE(ITK REQUIRED)
IF(ITK_FOUND)
    INCLUDE(${ITK_USE_FILE})
ELSE(ITK_FOUND)
    MESSAGE(FATAL_ERROR "ITK not found. Please set ITK_DIR.")
ENDIF(ITK_FOUND)


ADD_EXECUTABLE(PBNRR PBNRR.cxx itkVTKTetrahedralMeshReader.hxx)
ADD_EXECUTABLE(itkBlockMatchingImageFilterExample itkBlockMatchingImageFilterExample.cxx)
ADD_EXECUTABLE(ImageRegistration8 ImageRegistration8.cxx )
ADD_EXECUTABLE(ImageRegistration20 ImageRegistration20.cxx )
ADD_EXECUTABLE(MultiResImageRegistration3 MultiResImageRegistration3.cxx )
ADD_EXECUTABLE(itkExhaustiveOptimizerExample itkExhaustiveOptimizerExample.cxx )


TARGET_LINK_LIBRARIES(PBNRR ${ITK_LIBRARIES})
TARGET_LINK_LIBRARIES(itkBlockMatchingImageFilterExample ${ITK_LIBRARIES})
TARGET_LINK_LIBRARIES(ImageRegistration8 ${ITK_LIBRARIES})
TARGET_LINK_LIBRARIES(ImageRegistration20 ${ITK_LIBRARIES})
TARGET_LINK_LIBRARIES(MultiResImageRegistration3 ${ITK_LIBRARIES})
TARGET_LINK_LIBRARIES(itkExhaustiveOptimizerExample ${ITK_LIBRARIES})


 
