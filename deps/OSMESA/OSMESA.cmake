if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    ExternalProject_Add(dep_OSMESA
        URL https://archive.mesa3d.org/mesa-25.0.7.tar.xz
        URL_HASH SHA256=592272DF3CF01E85E7DB300C449DF5061092574D099DA275D19E97EF0510F8A6    
        DOWNLOAD_DIR ${DEP_DOWNLOAD_DIR}/OSMESA
        CONFIGURE_COMMAND meson setup build
            --prefix=${DESTDIR}/usr/local
            --libdir=lib
            -Dosmesa=true
            -Dgallium-drivers=softpipe
            -Dllvm=disabled
            -Dshared-glapi=enabled
            -Dglx=disabled -Degl=disabled -Dgbm=disabled
            -Dplatforms=[]
            -Dvulkan-drivers=[]
            -Dgallium-vdpau=disabled -Dgallium-va=disabled
            --buildtype=release
        BUILD_IN_SOURCE ON
        BUILD_COMMAND meson compile -C build
        INSTALL_COMMAND meson install -C build
    )
endif()