/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

/*
** thcommon.h: shared macros for hest test programs (th_*.c).
**
** Design contract:
**   - Programs only parse and print; no assertions live here.
**   - All checking of actual-vs-expected happens in run_hest_tests.py.
**   - Output format: one "key=value\n" line per result, to stdout.
**   - Exit codes: 0 = success (or expected failure detected);
**                 1 = unexpected outcome (test failed);
**                 2 = infrastructure error (airSanity, etc.)
**
** Including this header is all a test program needs beyond its own
** hestOptAdd_* calls.  It transitively pulls in hest.h (and therefore air.h).
*/

#ifndef THCOMMON_HAS_BEEN_INCLUDED
#define THCOMMON_HAS_BEEN_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <teem/hest.h>

/* TH_SANITY: verify the air library's compile-time / run-time assumptions
   match.  Exit 2 on failure (infrastructure error, not a test failure). */
#define TH_SANITY                                                                       \
  do {                                                                                  \
    int _ins = airSanity();                                                             \
    if (_ins) {                                                                         \
      fprintf(stderr, "airSanity failed: %s\n", airInsaneErr(_ins));                    \
      exit(2);                                                                          \
    }                                                                                   \
  } while (0)

/* TH_PARSE: call hestParse with already-adjusted argc/argv (i.e., the caller
   has already done argc-1, argv+1 to skip the program name).
   On hestParse failure: print the error to stderr and exit 1.
   On success: continue normally. */
#define TH_PARSE(opt, info, hparm, _argc, _argv)                                        \
  do {                                                                                  \
    char *_th_err = NULL;                                                               \
    int NULL_HPARM = AIR_FALSE;                                                         \
    th_getenvInt(&NULL_HPARM, "TH_NULL_HPARM");                                         \
    if (hestParse((opt), (_argc), (const char **)(_argv), &_th_err,                     \
                  NULL_HPARM > 1 ? NULL : (hparm))) {                                   \
      fprintf(stderr, "hestParse failed: %s\n", _th_err ? _th_err : "(no message)");    \
      free(_th_err);                                                                    \
      hestOptFree(opt);                                                                 \
      hestParmFree(hparm);                                                              \
      exit(1);                                                                          \
    }                                                                                   \
    if (opt->helpWanted) {                                                              \
      hestInfo(stdout, argv[0], info, NULL_HPARM ? NULL : (hparm));                     \
      hestUsage(stdout, opt, argv[0], NULL_HPARM ? NULL : (hparm), AIR_FALSE);          \
      printf("(again with oneLine=true for coverage)\n");                               \
      hestUsage(stdout, opt, argv[0], NULL_HPARM ? NULL : (hparm), AIR_TRUE);           \
      hestGlossary(stdout, opt, NULL_HPARM ? NULL : (hparm));                           \
      exit(0);                                                                          \
    }                                                                                   \
  } while (0)

/* TH_CLEANUP: free everything after a *successful* parse. */
#define TH_CLEANUP(opt, hparm)                                                          \
  do {                                                                                  \
    hestParseFree(opt);                                                                 \
    hestOptFree(opt);                                                                   \
    hestParmFree(hparm);                                                                \
  } while (0)

/* ==========================================================================
** Print macros: emit "key=value\n" to stdout.
**
** Floating-point precision:
**   - float:  %.9g   (24-bit mantissa → ~7.2 sig-decimal digits; 9 suffices)
**   - double: %.17g  (53-bit mantissa → ~15.9 sig-decimal digits; 17 suffices)
** Both choices guarantee that parsing the printed string back recovers the
** original bit pattern, which is what the Python runner's float comparison
** relies on.
**
** Strings are printed verbatim (no quoting).  Test string values must not
** contain '=' or newlines; hest itself does not support newlines in
** command-line arguments, so this is not a real restriction.
** ========================================================================== */
#define THPRINT_INT(name, val)    printf("%s=%d\n", (name), (int)(val))
#define THPRINT_UINT(name, val)   printf("%s=%u\n", (name), (unsigned int)(val))
#define THPRINT_LONG(name, val)   printf("%s=%ld\n", (name), (long)(val))
#define THPRINT_ULONG(name, val)  printf("%s=%lu\n", (name), (unsigned long)(val))
#define THPRINT_SIZE_T(name, val) printf("%s=%zu\n", (name), (size_t)(val))
#define THPRINT_BOOL(name, val)   printf("%s=%d\n", (name), (val) ? 1 : 0)
#define THPRINT_FLOAT(name, val)  printf("%s=%.9g\n", (name), (double)(val))
#define THPRINT_DOUBLE(name, val) printf("%s=%.17g\n", (name), (double)(val))
#define THPRINT_CHAR(name, val)   printf("%s=%c\n", (name), (val))
#define THPRINT_STR(name, val)    printf("%s=%s\n", (name), (val) ? (val) : "(null)")

/* THPRINT_LEN: print the number of elements parsed for a variadic option. */
#define THPRINT_LEN(name, sawVal) printf("%s.len=%u\n", (name), (unsigned int)(sawVal))

/* th_source_str: map a hestSource* enum value to the string the Python runner
   expects.  Defined static so each .c that includes this header gets its own
   copy without linker conflict. */
// (GLK) annoying!! that this is ignoring the const airEnum *const hestSource in hest.h
static const char *
th_source_str(int src) {
  switch (src) {
  case hestSourceCommandLine:
    return "cmdline";
  case hestSourceResponseFile:
    return "responsefile";
  case hestSourceDefault:
    return "default";
  default:
    return "unknown";
  }
}

/* THPRINT_SOURCE: print "name.src=<source>" for the option at index optIdx. */
#define THPRINT_SOURCE(name, opt, optIdx)                                               \
  printf("%s.src=%s\n", (name), th_source_str((opt)[(optIdx)].source))

// GLK adapted from nrrdGetenvBool in ../nrrd/defaultsNrrd.c
static void
th_getenvBool(int *val, const char *envVar) {
  char *envVal = getenv(envVar);
  if (!envVal) {
    // env var was not set, bye
    return;
  }
  // else env var was set: can it be parsed?
  int tmp = airEnumVal(airBool, envVal);
  if (airEnumUnknown(airBool) == tmp) {
    // else env var was set but could not be parsed
    fprintf(stderr, "ERROR(%s): could not parse env[\"%s\"]=\"%s\" as int\n", __func__,
            envVar, envVal);
    exit(1); // WE ARE OUT OF HERE
  }
  *val = tmp;
  return;
}

// GLK adapted from nrrdGetenvInt in ../nrrd/defaultsNrrd.c
static void
th_getenvInt(int *val, const char *envVar) {
  char *envVal = getenv(envVar);
  if (!envVal) {
    // env var was not set, bye
    return;
  }
  // else env var was set: can it be parsed?
  int tmp;
  int scanned = sscanf(envVal, "%d", &tmp);
  if (1 == scanned) {
    // if could parse one, set it
    *val = tmp;
    return;
  }
  // else env var was set but could not be parsed
  fprintf(stderr, "ERROR(%s): could not parse env[\"%s\"]=\"%s\" as int\n", __func__,
          envVar, envVal);
  exit(1); // WE ARE OUT OF HERE
  return;
}

static void
th_getenvUInt(unsigned int *val, const char *envVar) {
  char *envVal = getenv(envVar);
  if (!envVal) {
    // env var was not set, bye
    return;
  }
  // else env var was set: can it be parsed?
  unsigned int tmp;
  int scanned = sscanf(envVal, "%d", &tmp);
  if (1 == scanned) {
    // if could parse one, set it
    *val = tmp;
    return;
  }
  // else env var was set but could not be parsed
  fprintf(stderr, "ERROR(%s): could not parse env[\"%s\"]=\"%s\" as uint\n", __func__,
          envVar, envVal);
  exit(1); // WE ARE OUT OF HERE
  return;
}

#endif /* THCOMMON_HAS_BEEN_INCLUDED */
