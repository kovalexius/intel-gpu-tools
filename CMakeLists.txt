cmake_minimum_required(VERSION 2.8)

project(netup_gpu_meter)

add_definitions( -Wall -Wl,--no-as-needed -pthread -lpthread -std=c++11 -std=c11)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Libdrm REQUIRED)
if(Libdrm_FOUND)
    message(STATUS "Libdrm_LIBRARIES=${Libdrm_LIBRARIES}")
    message(STATUS "Libdrm_INCLUDE_DIRS=${Libdrm_INCLUDE_DIRS}")
else()
    message(STATUS "Libdrm not found!!!")
endif()


find_package(Cairo REQUIRED)
if(CAIRO_FOUND)
    message(STATUS "CAIRO_LIBRARIES=${CAIRO_LIBRARIES}")
    message(STATUS "CAIRO_INCLUDE_DIRS=${CAIRO_INCLUDE_DIRS}")
else()
    message(STATUS "Cairo not found!!!")
endif()


find_package(PCIAccess REQUIRED)
if(PCIACCESS_FOUND)
    message(STATUS "PCIACCESS_LIBRARIES=${PCIACCESS_LIBRARIES}")
    message(STATUS "PCIACCESS_INCLUDE_DIR=${PCIACCESS_INCLUDE_DIR}")
else()
    message(STATUS "PCIAccess not found!!!")
endif()


find_package(Libunwind REQUIRED)
if(LIBUNWIND_FOUND)
    message(STATUS "LIBUNWIND_LIBRARIES=${LIBUNWIND_LIBRARIES}")
    message(STATUS "LIBUNWIND_INCLUDE_DIR=${LIBUNWIND_INCLUDE_DIR}")
else()
    message(STATUS "Libunwind not found!!!")
endif()


include_directories( 
                     ${Libdrm_INCLUDE_DIRS}
                     ${CAIRO_INCLUDE_DIRS}
                     ${PCIACCESS_INCLUDE_DIR}
                     ./
                     lib/
                    )

set( INTEL_SRC
     lib/intel_device_info.c
     lib/intel_chipset.c
     lib/intel_mmio.c
     lib/intel_reg_map.c
     lib/instdone.c
     lib/igt_core.c
     lib/igt_aux.c
     lib/igt_stats.c
     lib/igt_rand.c
     lib/igt_kms.c
     lib/stubs/drm/intel_bufmgr.c
     lib/ioctl_wrappers.c
     lib/ioctl_wrappers.c
     lib/drmtest.c
     lib/igt_debugfs.c
     lib/igt_sysfs.c
     lib/uwildmat/uwildmat.c
     lib/igt_gt.c
   )
   
set( INTEL_LIBS
     ${Libdrm_LIBRARIES}
     ${CAIRO_LIBRARIES}
     ${PCIACCESS_LIBRARIES}
     "-pthread"
     "-lrt"            # time.h: timer_delete()
     "-lunwind"        # double link: Libunwind
     "-lpciaccess"     # double link: libpciaccess
   )
                    
add_executable( netup_gpu_meter
                ${INTEL_SRC}
                tools/netup_gpu_meter.cc
                tools/netup_get_statistics.c
               )
               
target_link_libraries( netup_gpu_meter
                       ${INTEL_LIBS}
                       "-lfcgi"          # libfcgi
                     )
                                 
add_executable( test_write_buffer

                ${INTEL_SRC}

                tools/test_netup_write_to_buffer.cc
                tools/netup_get_statistics.c
               )
               
target_link_libraries(
                        test_write_buffer
                        ${INTEL_LIBS}
                     )