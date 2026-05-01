set(LASERSPC_HOST_WRITER_SUPPORT_SOURCES
    SpcWriteManager.cpp
    BoardBatchAggregator.cpp
)

set(LASERSPC_HOST_WRITER_SUPPORT_HEADERS
    SpcWriteManager.h
    BoardBatchAggregator.h
)

add_library(LaserSpcHostWriterSupport STATIC
    ${LASERSPC_HOST_WRITER_SUPPORT_SOURCES}
    ${LASERSPC_HOST_WRITER_SUPPORT_HEADERS}
)
target_include_directories(LaserSpcHostWriterSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LaserSpcHostWriterSupport
    PUBLIC
        LaserSpcCore
        Qt${QT_VERSION_MAJOR}::Core
)

if (LASERSPC_BUILD_HOST_WRITER_EXAMPLE)
    add_executable(LaserSpcHostWriterExample
        main.cpp
        HostSpcExampleWindow.h
        HostSpcExampleWindow.cpp
        MesDebugWindow.h
        MesDebugWindow.cpp
    )
    target_include_directories(LaserSpcHostWriterExample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(LaserSpcHostWriterExample
        PRIVATE
            LaserSpcHostWriterSupport
            LaserSpcUi
            Qt${QT_VERSION_MAJOR}::Widgets
    )

    if (COMMAND laserspc_copy_mysql_runtime)
        laserspc_copy_mysql_runtime(LaserSpcHostWriterExample)
    endif()
endif()
