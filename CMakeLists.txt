cmake_minimum_required(VERSION 3.13)
project(BambuStudio)

include("version.inc")
include(GNUInstallDirs)
include(CMakeDependentOption)

set(SLIC3R_RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
file(TO_NATIVE_PATH "${SLIC3R_RESOURCES_DIR}" SLIC3R_RESOURCES_DIR_WIN)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (default Release)" FORCE)
endif()

if(DEFINED ENV{SLIC3R_STATIC})
        set(SLIC3R_STATIC_INITIAL $ENV{SLIC3R_STATIC})
else()
        if (MSVC OR MINGW OR APPLE)
                set(SLIC3R_STATIC_INITIAL 1)
        else()
                set(SLIC3R_STATIC_INITIAL 0)
        endif()
endif()

option(SLIC3R_STATIC 			"Compile BambuStudio with static libraries (Boost, TBB, glew)" ${SLIC3R_STATIC_INITIAL})
option(SLIC3R_GUI    			"Compile BambuStudio with GUI components (OpenGL, wxWidgets)" 1)
option(SLIC3R_FHS               "Assume BambuStudio is to be installed in a FHS directory structure" 0)
option(SLIC3R_WX_STABLE         "Build against wxWidgets stable (3.0) as oppsed to dev (3.1) on Linux" 0)
option(SLIC3R_PROFILE 			"Compile BambuStudio with an invasive Shiny profiler" 0)
option(SLIC3R_PCH               "Use precompiled headers" 1)
option(SLIC3R_MSVC_COMPILE_PARALLEL "Compile on Visual Studio in parallel" 1)
option(SLIC3R_MSVC_PDB          "Generate PDB files on MSVC in Release mode" 1)
option(SLIC3R_PERL_XS           "Compile XS Perl module and enable Perl unit and integration tests" 0)
option(SLIC3R_ASAN              "Enable ASan on Clang and GCC" 0)
# If SLIC3R_FHS is 1 -> SLIC3R_DESKTOP_INTEGRATION is always 0, othrewise variable.
CMAKE_DEPENDENT_OPTION(SLIC3R_DESKTOP_INTEGRATION "Allow perfoming desktop integration during runtime" 1 "NOT SLIC3R_FHS" 0)

set(OPENVDB_FIND_MODULE_PATH "" CACHE PATH "Path to OpenVDB installation's find modules.")

set(SLIC3R_GTK "2" CACHE STRING "GTK version to use with wxWidgets on Linux")

set(IS_CROSS_COMPILE FALSE)

set(FLATPAK FALSE CACHE BOOL "Not copy FFMPEG file")


if (APPLE)
    set(CMAKE_FIND_FRAMEWORK LAST)
    set(CMAKE_FIND_APPBUNDLE LAST)
    list(FIND CMAKE_OSX_ARCHITECTURES ${CMAKE_SYSTEM_PROCESSOR} _arch_idx)
    if (CMAKE_OSX_ARCHITECTURES AND _arch_idx LESS 0)
        set(IS_CROSS_COMPILE TRUE)
    endif ()
    if (CMAKE_MACOSX_BUNDLE)
        set(CMAKE_INSTALL_RPATH @executable_path/../Frameworks)
    endif()
    if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)
    endif ()
    message(STATUS "CMAKE_OSX_DEPLOYMENT_TARGET: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif ()

# Proposal for C++ unit tests and sandboxes
option(SLIC3R_BUILD_SANDBOXES   "Build development sandboxes" OFF)
option(SLIC3R_BUILD_TESTS       "Build unit tests" OFF)

if (IS_CROSS_COMPILE)
    message("Detected cross compilation setup. Tests and encoding checks will be forcedly disabled!")
    set(SLIC3R_PERL_XS OFF CACHE BOOL "" FORCE)
    set(SLIC3R_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif ()

# Print out the SLIC3R_* cache options
get_cmake_property(_cache_vars CACHE_VARIABLES)
list (SORT _cache_vars)
foreach (_cache_var ${_cache_vars})
    if("${_cache_var}" MATCHES "^SLIC3R_")
        message(STATUS "${_cache_var}: ${${_cache_var}}")
    endif ()
endforeach()

if (SLIC3R_GUI)
    add_definitions(-DSLIC3R_GUI)
endif ()

if(SLIC3R_DESKTOP_INTEGRATION)
    add_definitions(-DSLIC3R_DESKTOP_INTEGRATION)
endif ()

if (MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    set(IS_CLANG_CL TRUE)

    # clang-cl can interpret SYSTEM header paths if -imsvc is used
    set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-imsvc")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall \
        -Wno-old-style-cast -Wno-reserved-id-macro -Wno-c++98-compat-pedantic")
else ()
    set(IS_CLANG_CL FALSE)
endif ()

if (MSVC)
    if (SLIC3R_MSVC_COMPILE_PARALLEL AND NOT IS_CLANG_CL)
           add_compile_options(/MP)
    endif ()
    # /bigobj (Increase Number of Sections in .Obj file)
    # error C3859: virtual memory range for PCH exceeded; please recompile with a command line option of '-Zm90' or greater
    # Generate symbols at every build target, even for the release.
    add_compile_options(-bigobj -Zm520 /Zi)
    # Disable STL4007: Many result_type typedefs and all argument_type, first_argument_type, and second_argument_type typedefs are deprecated in C++17.
    #FIXME Remove this line after eigen library adapts to the new C++17 adaptor rules.
    add_compile_options(-D_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING)
    # Disable warnings on conversion from unsigned to signed (possible loss of data)
    # C4244: 'conversion' conversion from 'type1' to 'type2', possible loss of data. An integer type is converted to a smaller integer type.
    # C4267: The compiler detected a conversion from size_t to a smaller type.
    add_compile_options(/wd4244 /wd4267)
endif ()

# https://github.com/SoftFever/OrcaSlicer/issues/2233
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang" AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER 15)
    add_compile_definitions(BOOST_NO_CXX98_FUNCTION_BASE _HAS_AUTO_PTR_ETC=0)
endif()

if (MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif ()

if (NOT MSVC)
    # ARMs (Raspberry PI) use an unsigned char by default. Let's make it consistent for BambuStudio on all platforms.
    add_compile_options(-fsigned-char)
endif ()

# Display and check CMAKE_PREFIX_PATH
message(STATUS "SLIC3R_STATIC: ${SLIC3R_STATIC}")
if (NOT "${CMAKE_PREFIX_PATH}" STREQUAL "")
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH} (from cache or command line)")
    set(PREFIX_PATH_CHECK ${CMAKE_PREFIX_PATH})
elseif (NOT "$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")
    message(STATUS "CMAKE_PREFIX_PATH: $ENV{CMAKE_PREFIX_PATH} (from environment)")
    set(PREFIX_PATH_CHECK $ENV{CMAKE_PREFIX_PATH})
else ()
    message(STATUS "CMAKE_PREFIX_PATH: (default)")
endif ()

foreach (DIR ${PREFIX_PATH_CHECK})
    if (NOT EXISTS "${DIR}")
        message(WARNING "CMAKE_PREFIX_PATH element doesn't exist: ${DIR}")
    endif ()
endforeach ()

# Add our own cmake module path.
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules/)
message(STATUS "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

enable_testing ()

# Enable C++17 language standard.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    # Add DEBUG flags to debug builds.
    add_compile_options("$<$<CONFIG:DEBUG>:-DDEBUG>")
endif()

# To be able to link libslic3r with the Perl XS module.
# Once we get rid of Perl and libslic3r is linked statically, we can get rid of -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# WIN10SDK_PATH is used to point CMake to the WIN10 SDK installation directory.
# We pick it from environment if it is not defined in another way
if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/scripts/hooks/pre-commit"  AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        message(STATUS ".git: directory")
        configure_file(
            "${CMAKE_SOURCE_DIR}/scripts/hooks/pre-commit"
            "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit"
        )
    endif()

    find_package(PkgConfig REQUIRED)
    if(NOT DEFINED WIN10SDK_PATH)
        if(DEFINED ENV{WIN10SDK_PATH})
                set(WIN10SDK_PATH "$ENV{WIN10SDK_PATH}")
        endif()
    endif()
    if(DEFINED WIN10SDK_PATH)
        #BBS: modify win10sdk_path
        if (EXISTS "${WIN10SDK_PATH}/winrt/windows.graphics.printing3d.h")
            set(WIN10SDK_INCLUDE_PATH "${WIN10SDK_PATH}")
        else()
            message("WIN10SDK_PATH is invalid: ${WIN10SDK_PATH}")
            message("${WIN10SDK_PATH}/winrt/windows.graphics.printing3d.h was not found")
            message("STL fixing by the Netfabb service will not be compiled")
            unset(WIN10SDK_PATH)
        endif()
    else()
        # Try to use the default Windows 10 SDK path.
        set(WIN10SDK_INCLUDE_PATH "$ENV{WindowsSdkDir}/Include/$ENV{WindowsSDKVersion}")
        if (NOT EXISTS "${WIN10SDK_INCLUDE_PATH}/winrt/windows.graphics.printing3d.h")
            message("${WIN10SDK_INCLUDE_PATH}/winrt/windows.graphics.printing3d.h was not found")
            message("STL fixing by the Netfabb service will not be compiled")
            unset(WIN10SDK_INCLUDE_PATH)
        endif()
    endif()
    if(WIN10SDK_INCLUDE_PATH)
        message("Building with Win10 Netfabb STL fixing service support")
        add_definitions(-DHAS_WIN10SDK)
        include_directories("${WIN10SDK_INCLUDE_PATH}")
    else()
        message("Building without Win10 Netfabb STL fixing service support")
    endif()
endif()

if (APPLE)
    message("OS X SDK Path: ${CMAKE_OSX_SYSROOT}")
    if (CMAKE_OSX_DEPLOYMENT_TARGET)
        message("OS X Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else ()
        message("OS X Deployment Target: (default)")
    endif ()
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)

    # Boost on Raspberry-Pi does not link to pthreads.
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)

    find_package(DBus REQUIRED)
    include_directories(${DBUS_INCLUDE_DIRS})
endif()

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUXX)
    # Adding -fext-numeric-literals to enable GCC extensions on definitions of quad float literals, which are required by Boost.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fext-numeric-literals" )
endif()

if (NOT MSVC AND ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))
    if (NOT MINGW)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall" )
    endif ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-reorder" )

    # On GCC and Clang, no return from a non-void function is a warning only. Here, we make it an error.
    add_compile_options(-Werror=return-type)

    add_compile_options(-Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label -Wno-unused-local-typedefs)

    # removes LOTS of extraneous Eigen warnings (GCC only supports it since 6.1)
    # https://eigen.tuxfamily.org/bz/show_bug.cgi?id=1221
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0)
        add_compile_options(-Wno-ignored-attributes) # Tamas: Eigen include dirs are marked as SYSTEM
    endif()

    # Clang reports legacy OpenGL calls as deprecated. Turn off the warning for now
    # to reduce the clutter, we know about this one. It should be reenabled after
    # we finally get rid of the deprecated code.
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        add_compile_options(-Wno-deprecated-declarations)
    endif()

    if((${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang") AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER 15)
        add_compile_options(-Wno-error=enum-constexpr-conversion)
    endif()

    #GCC generates loads of -Wunknown-pragmas when compiling igl. The fix is not easy due to a bug in gcc, see
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66943 or
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53431
    # We will turn the warning of for GCC for now:
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-Wno-unknown-pragmas)
    endif()

endif()

if (SLIC3R_ASAN)
    # ASAN should be available on MSVC starting with Visual Studio 2019 16.9
    # https://devblogs.microsoft.com/cppblog/address-sanitizer-for-msvc-now-generally-available/
    add_compile_options(-fsanitize=address)

    if (NOT MSVC)
        add_compile_options(-fno-omit-frame-pointer)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=address")
    endif ()

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
    endif ()
endif ()

if (APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=partial-availability -Werror=unguarded-availability -Werror=unguarded-availability-new")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=partial-availability -Werror=unguarded-availability -Werror=unguarded-availability-new")
endif ()

if(MSVC)
# 添加编译选项，忽略警告 C4305 （格式转换截断）
add_compile_options(/wd4305)
endif()

# Where all the bundled libraries reside?
set(LIBDIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIBDIR_BIN ${CMAKE_CURRENT_BINARY_DIR}/src)
message(STATUS "LIBDIR: ${LIBDIR}")
message(STATUS "LIBDIR_BIN: ${LIBDIR_BIN}")

# For the bundled boost libraries (boost::nowide)
include_directories(${LIBDIR})
# For generated header files
include_directories(${LIBDIR_BIN}/platform)
# For ligigl
include_directories(${LIBDIR}/libigl)

if(WIN32)
    add_definitions(-D_USE_MATH_DEFINES -D_WIN32 -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
    if(MSVC)
        # BOOST_ALL_NO_LIB: Avoid the automatic linking of Boost libraries on Windows. Rather rely on explicit linking.
        add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_USE_WINAPI_VERSION=0x602 -DBOOST_SYSTEM_USE_UTF8 )
        # Force the source code encoding to UTF-8. See BambuStudio GH pull request #5583
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    endif(MSVC)
endif(WIN32)

add_definitions(-DwxUSE_UNICODE -D_UNICODE -DUNICODE -DWXINTL_NO_GETTEXT_MACRO)

# Disable unsafe implicit wxString to const char* / std::string and vice versa. This implicit conversion breaks the UTF-8 encoding quite often.
add_definitions(-DwxNO_UNSAFE_WXSTRING_CONV)

if (SLIC3R_PROFILE)
    message("BambuStudio will be built with a Shiny invasive profiler")
    add_definitions(-DSLIC3R_PROFILE)
endif ()

# Disable optimization even with debugging on.
if (0)
    message(STATUS "Perl compiled without optimization. Disabling optimization for the BambuStudio build.")
    message("Old CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
    message("Old CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELEASE}")
    message("Old CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /Od /Zi /EHsc /DWIN32 /DTBB_USE_ASSERT")
    set(CMAKE_C_FLAGS_RELEASE "/MD /Od /Zi /DWIN32 /DTBB_USE_ASSERT")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /Od /Zi /EHsc /DWIN32 /DTBB_USE_ASSERT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MD /Od /Zi /DWIN32 /DTBB_USE_ASSERT")
    set(CMAKE_CXX_FLAGS "/MD /Od /Zi /EHsc /DWIN32 /DTBB_USE_ASSERT")
    set(CMAKE_C_FLAGS "/MD /Od /Zi /DWIN32 /DTBB_USE_ASSERT")
endif()

# Find and configure boost
if(SLIC3R_STATIC)
    # Use static boost libraries.
    set(Boost_USE_STATIC_LIBS ON)
    # Use boost libraries linked statically to the C++ runtime.
    # set(Boost_USE_STATIC_RUNTIME ON)
endif()
#set(Boost_DEBUG ON)
# set(Boost_COMPILER "-mgw81")
# boost::process was introduced first in version 1.64.0,
# boost::beast::detail::base64 was introduced first in version 1.66.0
set(MINIMUM_BOOST_VERSION "1.83.0")
set(_boost_components "system;filesystem;thread;log;locale;regex;chrono;atomic;date_time;iostreams")
find_package(Boost ${MINIMUM_BOOST_VERSION} REQUIRED COMPONENTS ${_boost_components})

add_library(boost_libs INTERFACE)
add_library(boost_headeronly INTERFACE)

if (APPLE)
    # BOOST_ASIO_DISABLE_KQUEUE : prevents a Boost ASIO bug on OS X: https://svn.boost.org/trac/boost/ticket/5339
    target_compile_definitions(boost_headeronly INTERFACE BOOST_ASIO_DISABLE_KQUEUE)
endif()

if(NOT SLIC3R_STATIC)
    target_compile_definitions(boost_headeronly INTERFACE BOOST_LOG_DYN_LINK)
endif()

function(slic3r_remap_configs targets from_Cfg to_Cfg)
    if(MSVC)
        string(TOUPPER ${from_Cfg} from_CFG)

        foreach(tgt ${targets})
            if(TARGET ${tgt})
                set_target_properties(${tgt} PROPERTIES MAP_IMPORTED_CONFIG_${from_CFG} ${to_Cfg})
            endif()
        endforeach()
    endif()
endfunction()

if(TARGET Boost::system)
    message(STATUS "Boost::boost exists")
    target_link_libraries(boost_headeronly INTERFACE Boost::boost)

    # Only from cmake 3.12
    # list(TRANSFORM _boost_components PREPEND Boost:: OUTPUT_VARIABLE _boost_targets)
    set(_boost_targets "")
    foreach(comp ${_boost_components})
        list(APPEND _boost_targets "Boost::${comp}")
    endforeach()

    target_link_libraries(boost_libs INTERFACE
        boost_headeronly # includes the custom compile definitions as well
        ${_boost_targets}
        )
    slic3r_remap_configs("${_boost_targets}" RelWithDebInfo Release)
else()
    target_include_directories(boost_headeronly INTERFACE ${Boost_INCLUDE_DIRS})
    target_link_libraries(boost_libs INTERFACE boost_headeronly ${Boost_LIBRARIES})
endif()



# Find and configure intel-tbb
if(SLIC3R_STATIC)
    set(TBB_STATIC 1)
endif()
set(TBB_DEBUG 1)
find_package(TBB REQUIRED)
# include_directories(${TBB_INCLUDE_DIRS})
# add_definitions(${TBB_DEFINITIONS})
# if(MSVC)
#     # Suppress implicit linking of the TBB libraries by the Visual Studio compiler.
#     add_definitions(-D__TBB_NO_IMPLICIT_LINKAGE)
# endif()
# The Intel TBB library will use the std::exception_ptr feature of C++11.
# add_definitions(-DTBB_USE_CAPTURED_EXCEPTION=0)

find_package(OpenSSL REQUIRED)
if (APPLE)
find_package(CURL CONFIG REQUIRED)
else()
find_package(CURL REQUIRED)
endif()

add_library(libcurl INTERFACE)
target_link_libraries(libcurl INTERFACE CURL::libcurl)

find_package(ZLIB REQUIRED)
target_link_libraries(libcurl INTERFACE ZLIB::ZLIB)

# Fixing curl's cmake config script bugs
if (NOT WIN32)
    # Required by libcurl
    #find_package(ZLIB REQUIRED)
    #target_link_libraries(libcurl INTERFACE ZLIB::ZLIB)
    #find_package(Libssh2 REQUIRED)
    #target_link_libraries(libcurl INTERFACE Libssh2::libssh2)
else()
    target_link_libraries(libcurl INTERFACE crypt32)
endif()

if (SLIC3R_STATIC AND NOT SLIC3R_STATIC_EXCLUDE_CURL)
    if (NOT APPLE)
        # libcurl is always linked dynamically to the system libcurl on OSX.
        # On other systems, libcurl is linked statically if SLIC3R_STATIC is set.
        target_compile_definitions(libcurl INTERFACE CURL_STATICLIB)
    endif()
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # As of now, our build system produces a statically linked libcurl,
        # which links the OpenSSL library dynamically.
        find_package(OpenSSL REQUIRED)
        message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
        message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
        target_include_directories(libcurl INTERFACE ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(libcurl INTERFACE ${OPENSSL_LIBRARIES})
    endif()
endif()

## OPTIONAL packages

# Find eigen3 or use bundled version
if (NOT SLIC3R_STATIC)
    find_package(Eigen3 3.3)
endif ()
if (NOT EIGEN3_FOUND)
    set(EIGEN3_FOUND 1)
    set(EIGEN3_INCLUDE_DIR ${LIBDIR}/eigen/)
endif ()
include_directories(BEFORE SYSTEM ${EIGEN3_INCLUDE_DIR})

# Find expat or use bundled version
# Always use the system libexpat on Linux.

find_package(EXPAT)

if (NOT EXPAT_FOUND)
    add_library(expat STATIC
        ${LIBDIR}/expat/xmlparse.c
        ${LIBDIR}/expat/xmlrole.c
        ${LIBDIR}/expat/xmltok.c
    )
    set(EXPAT_FOUND 1)
    set(EXPAT_INCLUDE_DIRS ${LIBDIR}/expat/)
    set(EXPAT_LIBRARIES expat)
endif ()

find_package(PNG REQUIRED)

set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL REQUIRED)

# Find glew or use bundled version
if (SLIC3R_STATIC AND NOT SLIC3R_STATIC_EXCLUDE_GLEW)
    set(GLEW_USE_STATIC_LIBS ON)
    set(GLEW_VERBOSE ON)
endif()

find_package(GLEW REQUIRED)

find_package(glfw3 REQUIRED)

# Find the Cereal serialization library
find_package(cereal REQUIRED)

# l10n
set(L10N_DIR "${SLIC3R_RESOURCES_DIR}/i18n")
set(BBL_L18N_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bbl/i18n")
add_custom_target(gettext_make_pot
    COMMAND xgettext --keyword=L --no-wrap --keyword=_L --keyword=_u8L --keyword=L_CONTEXT:1,2c --keyword=_L_PLURAL:1,2 --add-comments=TRN --from-code=UTF-8 --no-location --debug --boost
        -f "${BBL_L18N_DIR}/list.txt"
        -o "${BBL_L18N_DIR}/BambuStudio.pot"
	COMMAND hintsToPot ${SLIC3R_RESOURCES_DIR} ${BBL_L18N_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Generate pot file from strings in the source tree"
)
add_custom_target(gettext_merge_po_with_pot
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Merge localization po with new generted pot file"
)
file(GLOB BBL_L10N_PO_FILES "${BBL_L18N_DIR}/*/BambuStudio*.po")
foreach(po_file ${BBL_L10N_PO_FILES})
    GET_FILENAME_COMPONENT(po_dir "${po_file}" DIRECTORY)
    SET(po_new_file "${po_dir}/BambuStudio_.po")
    add_custom_command(
        TARGET gettext_merge_po_with_pot PRE_BUILD
        COMMAND msgmerge --no-wrap -N -o ${po_file} ${po_file} "${BBL_L18N_DIR}/BambuStudio.pot"
        DEPENDS ${po_file}
    )
endforeach()
add_custom_target(gettext_po_to_mo
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Generate localization po files (binary) from mo files (texts)"
)
file(GLOB L10N_PO_FILES "${BBL_L18N_DIR}/*/BambuStudio*.po")
foreach(po_file ${BBL_L10N_PO_FILES})
    GET_FILENAME_COMPONENT(SECOND_FOLDER_ABSOLUTE ${po_file} DIRECTORY)
    string(REGEX REPLACE ".*/(.*)" "\\1" po_dir "${SECOND_FOLDER_ABSOLUTE}" )
    SET(mo_file "${L10N_DIR}/${po_dir}/BambuStudio.mo")
    add_custom_command(
        TARGET gettext_po_to_mo PRE_BUILD
        COMMAND msgfmt ARGS --check-format -o ${mo_file} ${po_file}
        #COMMAND msgfmt ARGS --check-compatibility -o ${mo_file} ${po_file}
        DEPENDS ${po_file}
    )
endforeach()

# copy pt-BR/BamabuStudio.mo to pt_br/
SET(PT_BR "${L10N_DIR}/pt-BR/BambuStudio.mo")
SET(PT_BR_DST "${L10N_DIR}/pt_br/")
add_custom_command(
    TARGET gettext_po_to_mo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PT_BR} ${PT_BR_DST}
)

find_package(NLopt 1.4 REQUIRED)


if(SLIC3R_STATIC)
    set(OPENVDB_USE_STATIC_LIBS ON)
    set(USE_BLOSC TRUE)
endif ()

find_package(OpenVDB 5.0 COMPONENTS openvdb)
if(OpenVDB_FOUND)
    slic3r_remap_configs(IlmBase::Half RelWithDebInfo Release)
    slic3r_remap_configs(Blosc::blosc RelWithDebInfo Release)
else ()
    message(FATAL_ERROR "OpenVDB could not be found with the bundled find module. "
                   "You can try to specify the find module location of your "
                   "OpenVDB installation with the OPENVDB_FIND_MODULE_PATH cache variable.")
endif ()


set(TOP_LEVEL_PROJECT_DIR ${PROJECT_SOURCE_DIR})
function(bambustudio_copy_dlls target config postfix output_dlls)
    if ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        set(_bits 64)
    elseif ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        set(_bits 32)
    endif ()

    get_property(_is_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    get_target_property(_alt_out_dir ${target} RUNTIME_OUTPUT_DIRECTORY)

    if (_alt_out_dir)
        set (_out_dir "${_alt_out_dir}")
        message ("set out_dir to _alt_out_dir: ${_out_dir}")
    elseif (_is_multi)
        set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/${config}")
        message ("set out_dir to CMAKE_CURRENT_BINARY_DIR/config: ${_out_dir}")
    else ()
        set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}")
        message ("set out_dir to CMAKE_CURRENT_BINARY_DIR: ${_out_dir}")
    endif ()

    file(COPY ${TOP_LEVEL_PROJECT_DIR}/deps/GMP/gmp/lib/win${_bits}/libgmp-10.dll
            ${TOP_LEVEL_PROJECT_DIR}/deps/MPFR/mpfr/lib/win${_bits}/libmpfr-4.dll
            ${TOP_LEVEL_PROJECT_DIR}/deps/WebView2/lib/win${_bits}/WebView2Loader.dll
        DESTINATION ${_out_dir})

    file(COPY ${CMAKE_PREFIX_PATH}/bin/occt/TKBO.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKBRep.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKCAF.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKCDF.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKernel.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKG2d.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKG3d.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKGeomAlgo.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKGeomBase.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKHLR.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKLCAF.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKMath.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKMesh.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKPrim.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKService.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKShHealing.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKSTEP.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKSTEP209.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKSTEPAttr.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKSTEPBase.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKTopAlgo.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKV3d.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKVCAF.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKXCAF.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKXDESTEP.dll
            ${CMAKE_PREFIX_PATH}/bin/occt/TKXSBase.dll
            ${CMAKE_PREFIX_PATH}/bin/freetype.dll
            ${CMAKE_PREFIX_PATH}/bin/avcodec-61.dll
            ${CMAKE_PREFIX_PATH}/bin/swresample-5.dll
            ${CMAKE_PREFIX_PATH}/bin/swscale-8.dll
            ${CMAKE_PREFIX_PATH}/bin/avutil-59.dll
         DESTINATION ${_out_dir})

    set(${output_dlls}
        ${_out_dir}/libgmp-10.dll
        ${_out_dir}/libmpfr-4.dll
        ${_out_dir}/WebView2Loader.dll

        ${_out_dir}/TKBO.dll
        ${_out_dir}/TKBRep.dll
        ${_out_dir}/TKCAF.dll
        ${_out_dir}/TKCDF.dll
        ${_out_dir}/TKernel.dll
        ${_out_dir}/TKG2d.dll
        ${_out_dir}/TKG3d.dll
        ${_out_dir}/TKGeomAlgo.dll
        ${_out_dir}/TKGeomBase.dll
        ${_out_dir}/TKHLR.dll
        ${_out_dir}/TKLCAF.dll
        ${_out_dir}/TKMath.dll
        ${_out_dir}/TKMesh.dll
        ${_out_dir}/TKPrim.dll
        ${_out_dir}/TKService.dll
        ${_out_dir}/TKShHealing.dll
        ${_out_dir}/TKSTEP.dll
        ${_out_dir}/TKSTEP209.dll
        ${_out_dir}/TKSTEPAttr.dll
        ${_out_dir}/TKSTEPBase.dll
        ${_out_dir}/TKTopAlgo.dll
        ${_out_dir}/TKV3d.dll
        ${_out_dir}/TKVCAF.dll
        ${_out_dir}/TKXCAF.dll
        ${_out_dir}/TKXDESTEP.dll
        ${_out_dir}/TKXSBase.dll

        ${_out_dir}/freetype.dll
        ${_out_dir}/avcodec-61.dll
        ${_out_dir}/swresample-5.dll
        ${_out_dir}/swscale-8.dll
        ${_out_dir}/avutil-59.dll
        PARENT_SCOPE
    )

endfunction()

function(bambustudio_copy_sos target config postfix output_sos)

    set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}")
    message ("set out_dir to CMAKE_CURRENT_BINARY_DIR: ${_out_dir}")

    file(COPY ${CMAKE_PREFIX_PATH}/lib/libavcodec.so
            ${CMAKE_PREFIX_PATH}/lib/libavcodec.so.61
            ${CMAKE_PREFIX_PATH}/lib/libavcodec.so.61.3.100
            ${CMAKE_PREFIX_PATH}/lib/libavutil.so
            ${CMAKE_PREFIX_PATH}/lib/libavutil.so.59
            ${CMAKE_PREFIX_PATH}/lib/libavutil.so.59.8.100
            ${CMAKE_PREFIX_PATH}/lib/libswscale.so
            ${CMAKE_PREFIX_PATH}/lib/libswscale.so.8
            ${CMAKE_PREFIX_PATH}/lib/libswscale.so.8.1.100
            ${CMAKE_PREFIX_PATH}/lib/libswresample.so
            ${CMAKE_PREFIX_PATH}/lib/libswresample.so.5
            ${CMAKE_PREFIX_PATH}/lib/libswresample.so.5.1.100
         DESTINATION ${_out_dir})

    set(${output_dlls}
        ${_out_dir}/libavcodec.so
        ${_out_dir}/libavcodec.so.61
        ${_out_dir}/libavcodec.so.61.3.100
        ${_out_dir}/libavutil.so
        ${_out_dir}/libavutil.so.59
        ${_out_dir}/libavutil.so.59.8.100
        ${_out_dir}/libswscale.so
        ${_out_dir}/libswscale.so.8
        ${_out_dir}/libswscale.so.8.1.100
        ${_out_dir}/libswresample.so
        ${_out_dir}/libswresample.so.5
        ${_out_dir}/libswresample.so.5.1.100
        PARENT_SCOPE
    )
endfunction()

# libslic3r, BambuStudio GUI and the BambuStudio executable.
add_subdirectory(src)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BambuStudio_app_gui)

add_dependencies(gettext_make_pot hintsToPot)

# Perl bindings, currently only used for the unit / integration tests of libslic3r.
# Also runs the unit / integration tests.
#FIXME Port the tests into C++ to finally get rid of the Perl!
if (SLIC3R_PERL_XS)
    add_subdirectory(xs)
endif ()

if(SLIC3R_BUILD_SANDBOXES)
    add_subdirectory(sandboxes)
endif()

if(SLIC3R_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if (NOT WIN32 AND NOT APPLE)
    set(SLIC3R_APP_CMD "bambu-studio")
    configure_file(${LIBDIR}/platform/unix/build_appimage.sh.in ${CMAKE_CURRENT_BINARY_DIR}/build_appimage.sh @ONLY)
endif()

option(BUILD_BBS_TEST_TOOLS          "Build bbs test tools" OFF)
if(BUILD_BBS_TEST_TOOLS)
    add_subdirectory(bbs_test_tools)
endif()


# Resources install target, configure fhs.hpp on UNIX
if (WIN32)
    install(DIRECTORY "${SLIC3R_RESOURCES_DIR}/" DESTINATION "${CMAKE_INSTALL_PREFIX}/resources")
elseif (SLIC3R_FHS)
    # CMAKE_INSTALL_FULL_DATAROOTDIR: read-only architecture-independent data root (share)
    set(SLIC3R_FHS_RESOURCES "${CMAKE_INSTALL_FULL_DATAROOTDIR}/BambuStudio")
    install(DIRECTORY ${SLIC3R_RESOURCES_DIR}/ DESTINATION ${SLIC3R_FHS_RESOURCES}
        PATTERN "*/udev" EXCLUDE
    )
    install(FILES src/platform/unix/BambuStudio.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    foreach(SIZE 32 128 192)
        install(FILES ${SLIC3R_RESOURCES_DIR}/images/BambuStudio_${SIZE}px.png
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${SIZE}x${SIZE}/apps RENAME BambuStudio.png
        )
    endforeach()
elseif (CMAKE_MACOSX_BUNDLE)
    install(DIRECTORY "${SLIC3R_RESOURCES_DIR}/" DESTINATION "${CMAKE_INSTALL_PREFIX}/BambuStudio.app/Contents/resources")
else ()
    install(FILES src/platform/unix/BambuStudio.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/resources/applications)
    install(DIRECTORY "${SLIC3R_RESOURCES_DIR}/" DESTINATION "${CMAKE_INSTALL_PREFIX}/resources")
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT FLATPAK)
    set(LIBRARY_FILES
    ${LIBDIR_BIN}/libavcodec.so.61
    ${LIBDIR_BIN}/libavcodec.so.61.3.100
    ${LIBDIR_BIN}/libavutil.so.59
    ${LIBDIR_BIN}/libavutil.so.59.8.100
    ${LIBDIR_BIN}/libswresample.so.5
    ${LIBDIR_BIN}/libswresample.so.5.1.100
    ${LIBDIR_BIN}/libswscale.so.8
    ${LIBDIR_BIN}/libswscale.so.8.1.100
    )
    install(FILES ${LIBRARY_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif ()

configure_file(${LIBDIR}/platform/unix/fhs.hpp.in ${LIBDIR_BIN}/platform/unix/fhs.hpp)
