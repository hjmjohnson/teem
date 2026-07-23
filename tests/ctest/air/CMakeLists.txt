
teem_create_test_bin(air_miscAir miscAir.c)
teem_add_test(air_miscAir)

teem_create_test_bin(air_mtrand mtrand.c)
teem_add_test(air_mtrand)

teem_create_test_bin(air_amath amath.c)
teem_add_test(air_amath)

teem_create_test_bin(air_string string.c)
teem_add_test(air_string)

teem_create_test_bin(air_strtok strtok.c)
teem_add_test(air_strtok)

teem_create_test_bin(air_pptest pptest.c)
teem_add_test(air_pptest)

teem_create_test_bin(air_locale locale.c)
teem_add_test(air_locale)
set_tests_properties(air_locale PROPERTIES SKIP_RETURN_CODE 77)
