cmake_minimum_required (VERSION 2.6)
project (XtalComp)


set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library (XtalComp STATIC xtalcomp.cpp xctransform.cpp xcvector.h xcmatrix.h
  stablecomparison.h)

add_library (XtalCompShared SHARED xtalcomp.cpp xctransform.cpp xcvector.h xcmatrix.h
stablecomparison.h)
set_target_properties(XtalCompShared PROPERTIES OUTPUT_NAME XtalComp)

target_link_libraries(XtalComp)
target_link_libraries(XtalCompShared)

install(TARGETS XtalComp XtalCompShared LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
install(FILES "xcmatrix.h" "xctransform.h" "xcvector.h" "xtalcomp.h" DESTINATION "include/xtalcomp")
