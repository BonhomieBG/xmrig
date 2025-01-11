if (WITH_HOOHASH AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(/DXMRIG_ALGO_HOOHASH)
    list(APPEND SOURCES_CRYPTO
        src/crypto/hoohash/hoohash.c
        src/crypto/hoohash/bigint.c
    )
else()
    remove_definitions(/DXMRIG_ALGO_HOOHASH)
endif()
