if (WITH_HOOHASHV1 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(/DXMRIG_ALGO_HOOHASHV1)
    list(APPEND SOURCES_CRYPTO
        src/crypto/cn/hoohashv1/hoohash.c
        src/crypto/cn/hoohashv1/bigint.c
    )
else()
    remove_definitions(/DXMRIG_ALGO_HOOHASHV1)
endif()
