teem_create_test_bin(nrrd_sanity sanity.c)
teem_add_test(nrrd_sanity)

teem_create_test_bin(nrrd_trand trand.c USE_TMPDIR)
teem_add_test(nrrd_trand)

teem_create_test_bin(nrrd_tload tload.c USE_TMPDIR)
teem_add_test(nrrd_tload)

teem_create_test_bin(nrrd_tskip tskip.c USE_TMPDIR)
teem_add_test(nrrd_tskip NAME nrrd_tskip11p ARGS -s 101 102 103 -p 66 81     -o tsA.raw tsA.nhdr)
teem_add_test(nrrd_tskip NAME nrrd_tskip11n ARGS -s 101 102 103 -p 66 81 -ns -o tsB.raw tsB.nhdr)
teem_add_test(nrrd_tskip NAME nrrd_tskip01p ARGS -s 101 102 103 -p 0 99      -o tsC.raw tsC.nhdr)
teem_add_test(nrrd_tskip NAME nrrd_tskip01n ARGS -s 101 102 103 -p 0 99 -ns  -o tsD.raw tsD.nhdr)
teem_add_test(nrrd_tskip NAME nrrd_tskip10p ARGS -s 101 102 103 -p 77 0      -o tsE.raw tsE.nhdr)
teem_add_test(nrrd_tskip NAME nrrd_tskip10n ARGS -s 101 102 103 -p 77 0 -ns  -o tsF.raw tsF.nhdr)

teem_create_test_bin(nrrd_macros macros.c)
teem_add_test(nrrd_macros)

teem_create_test_bin(nrrd_bspec tbspec.c)
teem_add_test(nrrd_bspec ARGS -bs bleed wrap pad:42)
# HEY why not more nrrd_bspec tests?

teem_create_test_bin(nrrd_datafilelast tdatafilelast.c USE_TMPDIR)
teem_add_test(nrrd_datafilelast)
teem_create_test_bin(nrrd_kindsnone tkindsnone.c USE_TMPDIR)
teem_add_test(nrrd_kindsnone)
teem_create_test_bin(nrrd_origin torigin.c)
teem_add_test(nrrd_origin)
teem_create_test_bin(nrrd_parseoverflow tparseoverflow.c)
teem_add_test(nrrd_parseoverflow)
teem_create_test_bin(nrrd_cle tcle.c USE_TMPDIR)
foreach(cle_case stale_write space_origin_ws empty_stod empty_decisep_remove
                 vtk_write vtk_read lc_all_nan)
  teem_add_test(nrrd_cle NAME nrrd_cle_${cle_case} ARGS ${cle_case})
  set_tests_properties(nrrd_cle_${cle_case} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()
