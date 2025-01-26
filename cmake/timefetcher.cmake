if (WITH_TIMEFETCHER)
    add_definitions(/DXMRIG_TIMEFETCHER)
    add_subdirectory(src/net/strategies)
    set(TIMEFETCHER_LIBRARY Timefetcher)
else()
    remove_definitions(-DXMRIG_TIMEFETCHER)
    set(TIMEFETCHER_LIBRARY "")
endif()