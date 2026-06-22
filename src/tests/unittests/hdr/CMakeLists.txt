add_cmocka_test(test_hdr_clli
                SOURCES test_hdr_clli.c
                LINK_LIBRARIES cmocka::cmocka m OpenMP::OpenMP_C)
target_include_directories(test_hdr_clli PRIVATE ${CMAKE_SOURCE_DIR}/src)
