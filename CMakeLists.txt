cmake_minimum_required(VERSION 3.10)

set(PATCH_VERSION "2" CACHE INTERNAL "Patch version")
set(PROJECT_VESRION 0.1.${PATCH_VERSION})

project(bayan_prg VERSION ${PROJECT_VESRION})

find_package(Boost COMPONENTS unit_test_framework program_options REQUIRED)
#find_package(Boost COMPONENTS program_options REQUIRED)
message(${Boost_LIBRARIES})

#find_package(Boost REQUIRED COMPONENTS program_options)
#include_directories(${Boost_INCLUDE_DIRS})


add_executable(bayan main.cpp)
add_executable(test_bayan test_bayan.cpp)

set_target_properties(bayan test_bayan  PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
set_target_properties(test_bayan PROPERTIES
    COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
    INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
)

set_target_properties(bayan PROPERTIES
    #COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
    INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
)

# target_link_libraries(helloworld_cli PRIVATE
#     helloworld
# )

target_link_libraries(test_bayan
    ${Boost_LIBRARIES}    
)
target_link_libraries(bayan
    ${Boost_LIBRARIES}
)

#target_link_libraries(bayan ${Boost_LIBRARIES})

if (MSVC)
    target_compile_options(bayan PRIVATE
        /W4
    )
    target_compile_options(bayan_prg PRIVATE
        /W4
    )
    target_compile_options(test_bayan PRIVATE
        /W4
    )
else ()
    target_compile_options(bayan PRIVATE
        -Wall -Wextra -pedantic -Werror
    )
    target_compile_options(test_bayan PRIVATE
        -Wall -Wextra -pedantic -Werror
    )
endif()



install(TARGETS bayan RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_CONTACT example@example.com)

include(CPack)

enable_testing()

add_test(test_bayan test_bayan)
