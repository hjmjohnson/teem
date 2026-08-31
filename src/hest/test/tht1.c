/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/1de6190b-7b97-46bf-adcf-7d03cbd1e579
*/

/*
** tht1.c: unit tests for hest internals (Tier 1).
**
** These tests do NOT call hestParse; they exercise:
**   - hest__PlainWord (from privateHest.h)
**   - hestArgVecSprint and related argv building (argvHest.c)
**   - hestOptCheck / hest__OPCheck validation (methodsHest.c)
**
** Build: link against the hest (and transitively air, biff) library.
** Recommended: compile with -fsanitize=address,undefined to catch memory
* errors.
**
** Usage:
**   ./tht1           -- run all tests, print summary
**   ./tht1 test_name -- run one named test, exit 0 on pass / 1 on fail
*/

#include "../hest.h"
#include "../privateHest.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Test infrastructure
 * ========================================================================= */

/* Every test function has this signature.
   Returns 0 on pass; returns 1 with a message in `err` on failure. */
typedef int (*TestFn)(char err[AIR_STRLEN_HUGE + 1]);

typedef struct {
  const char *name;
  TestFn fn;
  const char *desc;
} TestEntry;

/* Fill `err` and return 1 from the current function. */
#define FAIL(msg)                                                                       \
  do {                                                                                  \
    airStrcpy(err, AIR_STRLEN_HUGE + 1, (msg));                                         \
    return 1;                                                                           \
  } while (0)

/* Printf-style fill of `err`, then return 1. */
#define FAILF(...)                                                                      \
  do {                                                                                  \
    snprintf(err, AIR_STRLEN_HUGE + 1, __VA_ARGS__);                                    \
    return 1;                                                                           \
  } while (0)

/* =========================================================================
 * Tier 1a: hest__PlainWord
 * =========================================================================
 *
 * hest__PlainWord(s) returns non-zero if the string s can be written to the
 * shell without any quoting or escaping.  The current implementation accepts:
 *   isalnum(c)  OR  c in a set of characters deemed non-special, as well as
 * simple position-dependent handling of ~ and !
 *
 * These tests document that contract.  When hest__PlainWord is extended to
 * allow additional characters (e.g. ':', '+', '=') the expected_plain_char
 * helper and the over_quoted spot-checks below must be updated to match, so
 * that the test suite serves as a specification of the new rule, not just a
 * regression of the old one.
 */

/* expected_plain_char: the current rule, expressed independently of the
   implementation so the test can catch divergence in either direction. */
static int
expected_plain_char(char c) {
  return isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.' || c == ','
      || c == '/' || c == '!' || c == '#' || c == '%' || c == '+' || c == ':' || c == '='
      || c == '@' || c == '^';
}

static int
test_plain_word_chars(char err[AIR_STRLEN_HUGE + 1]) {
  /* The empty string is not a plain word regardless of character rules. */
  if (hest__PlainWord("")) {
    FAIL("empty string should not be plain");
  }
  /* Test every printable ASCII character in isolation. */
  for (int c = 0x20; c <= 0x7E; c++) {
    char s[2] = {(char)c, '\0'};
    int got = !!hest__PlainWord(s); /* normalize to 0/1 */
    int want = !!expected_plain_char((char)c);
    if (got != want) {
      FAILF("char 0x%02X '%c': got %s, expected %s", c, isprint(c) ? c : '?',
            got ? "plain" : "not-plain", want ? "plain" : "not-plain");
    }
  }
  return 0;
}

static int
test_plain_word_strings(char err[AIR_STRLEN_HUGE + 1]) {
  /* Multi-character strings that should be plain. */
  static const char *const plain_cases[]
    = {"hello",      "foo_bar",        /* simple strings */
       "3.14",                         /* a decimal number */
       "+3.14",                        /* positive decimal number */
       "-3.14",                        /* negative decimal */
       "0,1,2",                        /* comma-separated */
       "/usr/local",                   /* Unix path fragment */
       "some-flag",                    /* flag-like */
       "abc123",                       /* simple alphanumeric */
       "a+b",        "0:10",    "x=y", /* does not need quoting */
       "!",          "bingo!",         /* not history-expanded */
       "bingo~",                       /* not $HOME-expanded */
       "A",                            /* single upper-case letter */
       "z",                            /* single lower-case letter */
       "9",                            /* single digit */
       "_",                            /* single underscore */
       "-",                            /* single hyphen */
       ".",                            /* single dot */
       ",",                            /* single comma */
       "/",                            /* single slash */
       NULL};
  for (int i = 0; plain_cases[i]; i++) {
    if (!hest__PlainWord(plain_cases[i])) {
      FAILF("string \"%s\" should be plain", plain_cases[i]);
    }
  }

  /* Multi-character strings that should NOT be plain. */
  static const char *const not_plain_cases[] = {"",            /* empty */
                                                "hello world", /* space */
                                                "\"quoted\"",  /* double-quote */
                                                "it's",        /* single-quote */
                                                "!bingo",      /* is history-expanded */
                                                "~/data",      /* tilde */
                                                "$HOME",       /* dollar */
                                                "a b",         /* embedded space */
                                                "foo\tbar",    /* tab */
                                                "foo\nbar",    /* newline */
                                                NULL};
  for (int i = 0; not_plain_cases[i]; i++) {
    if (hest__PlainWord(not_plain_cases[i])) {
      FAILF("string \"%s\" should not be plain", not_plain_cases[i]);
    }
  }
  return 0;
}

/* =========================================================================
 * Tier 1b: hestArgVecSprint
 * =========================================================================
 *
 * hestArgVecSprint reconstructs a shell-like command string from an
 * argument vector: plain words appear verbatim, and words that need quoting
 * are wrapped in single or double quotes (whichever is shorter).
 *
 * These tests verify:
 *   - plain words are passed through unmodified
 *   - words that need quoting are wrapped in quotes
 *   - the output is a space-separated concatenation
 *   - the over-quoting of ':' etc. is documented (will need updating after fix)
 */

/* Build a havec from `n` strings, sprint it (showIdx=false), compare to `want`.
   Cleans up the havec.  Writes to `err` and returns 1 on mismatch. */
static int
check_sprint(const char *const *strs, unsigned int n, const char *want, const char *ctx,
             char err[AIR_STRLEN_HUGE + 1]) {
  hestArgVec *havec = hestArgVecNew();
  for (unsigned int i = 0; i < n; i++) {
    hestArgVecAppendString(havec, strs[i]);
  }
  char *got = hestArgVecSprint(havec, AIR_FALSE);
  hestArgVecNix(havec);
  /* compare before any free */
  int match = got ? !strcmp(got, want) : !strcmp("", want);
  if (!match) {
    snprintf(err, AIR_STRLEN_HUGE + 1, "%s: got \"%s\", expected \"%s\"", ctx,
             got ? got : "(NULL)", want);
  }
  free(got);
  return match ? 0 : 1;
}

static int
test_sprint_plain(char err[AIR_STRLEN_HUGE + 1]) {
  /* Empty havec → empty string. */
  if (check_sprint(NULL, 0, "", "empty havec", err)) return 1;

  /* Single plain word → that word verbatim. */
  {
    const char *strs[] = {"hello"};
    if (check_sprint(strs, 1, "hello", "single plain word", err)) return 1;
  }
  /* Multiple plain words → space-separated. */
  {
    const char *strs[] = {"foo", "bar", "baz"};
    if (check_sprint(strs, 3, "foo bar baz", "three plain words", err)) return 1;
  }
  /* Multiple words that should be treated as plain */
  {
    const char *strs[] = {"0:10", "+3.14", "key=val", "user@host"};
    if (check_sprint(strs, 4, "0:10 +3.14 key=val user@host", "four plain-ish words",
                     err))
      return 1;
  }
  /* Verify the specific plain punctuation characters that are allowed. */
  {
    const char *strs[] = {"3.14", "0,1,2", "/usr/local", "my_flag"};
    if (check_sprint(strs, 4, "3.14 0,1,2 /usr/local my_flag", "plain punctuation", err))
      return 1;
  }
  /* Single-character plain words. */
  {
    const char *strs[] = {"a", "1", "_", "-", ".", ",", "/"};
    if (check_sprint(strs, 7, "a 1 _ - . , /", "single plain chars", err)) return 1;
  }
  return 0;
}

static int
test_sprint_quoted(char err[AIR_STRLEN_HUGE + 1]) {
  /* Helper: sprint a single string and return the result (caller frees). */
#define SPRINT1(s)                                                                      \
  do {                                                                                  \
    hestArgVec *_hv = hestArgVecNew();                                                  \
    hestArgVecAppendString(_hv, (s));                                                   \
    _got = hestArgVecSprint(_hv, AIR_FALSE);                                            \
    hestArgVecNix(_hv);                                                                 \
  } while (0)

  char *_got = NULL;

  /* Empty string must be quoted (cannot be reproduced verbatim). */
  SPRINT1("");
  {
    int is_quoted = _got && (_got[0] == '\'' || _got[0] == '"');
    free(_got);
    _got = NULL;
    if (!is_quoted) FAIL("empty string should be quoted in sprint output");
  }

  /* String containing a space must be quoted. */
  SPRINT1("hello world");
  {
    int is_quoted = _got && (_got[0] == '\'' || _got[0] == '"');
    free(_got);
    _got = NULL;
    if (!is_quoted) FAIL("\"hello world\" should be quoted");
  }

  /* String containing a single-quote: single-quoting alone can't work,
     so the result must differ from the raw input. */
  SPRINT1("it's");
  {
    int differs = _got && strcmp(_got, "it's") != 0;
    free(_got);
    _got = NULL;
    if (!differs) FAIL("\"it's\" should be quoted/escaped, not passed verbatim");
  }

  /* String containing a double-quote: must be escaped or single-quoted. */
  SPRINT1("say \"hello\"");
  {
    int differs = _got && strcmp(_got, "say \"hello\"") != 0;
    free(_got);
    _got = NULL;
    if (!differs) FAIL("string with double-quote should be quoted/escaped");
  }

#undef SPRINT1
  return 0;
}

/* =========================================================================
 * Tier 1c: hestOptCheck — valid configurations
 * =========================================================================
 *
 * One sub-case per kind (1–5), plus a flagged and an unflagged kind-2, and
 * a small multi-option array.  hestOptCheck must return 0 for each.
 */

static int
test_optcheck_valid(char err[AIR_STRLEN_HUGE + 1]) {
  char *chkErr = NULL;

/* Macro: build an hopt block, check it, clean up, and fail if check returns
 * non-zero. */
#define CHECK_VALID(ctx)                                                                \
  chkErr = NULL;                                                                        \
  if (hestOptCheck(hopt, &chkErr)) {                                                    \
    snprintf(err, AIR_STRLEN_HUGE + 1, "%s: hestOptCheck failed: %s", (ctx),            \
             chkErr ? chkErr : "(no msg)");                                             \
    free(chkErr);                                                                       \
    hestOptFree(hopt);                                                                  \
    return 1;                                                                           \
  }                                                                                     \
  free(chkErr);                                                                         \
  hestOptFree(hopt)

  /* kind 1: stand-alone flag */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    hestOptAdd_Flag(&hopt, "v", &v, "be verbose");
    CHECK_VALID("kind-1 stand-alone flag");
  }

  /* kind 2: single fixed parameter, flagged */
  {
    hestOpt *hopt = NULL;
    int n = 0;
    hestOptAdd_1_Int(&hopt, "n", "count", &n, "1", "how many");
    CHECK_VALID("kind-2 flagged int");
  }

  /* kind 2: single fixed parameter, unflagged (positional) */
  {
    hestOpt *hopt = NULL;
    int n = 0;
    hestOptAdd_1_Int(&hopt, NULL, "count", &n, "1", "how many");
    CHECK_VALID("kind-2 unflagged int");
  }

  /* kind 2: single fixed string parameter */
  {
    hestOpt *hopt = NULL;
    char *fname = NULL;
    hestOptAdd_1_String(&hopt, "i", "file", &fname, "input.nrrd", "input file");
    CHECK_VALID("kind-2 string");
  }

  /* kind 3: multiple fixed parameters (N=4, for RGBA) */
  {
    hestOpt *hopt = NULL;
    float rgba[4] = {0};
    hestOptAdd_4_Float(&hopt, "c,color", "R G B A", rgba, "1 1 1 1", "color");
    CHECK_VALID("kind-3 four floats");
  }

  /* kind 3: multiple fixed parameters (N=2, unflagged) */
  {
    hestOpt *hopt = NULL;
    int wh[2] = {0};
    hestOptAdd_2_Int(&hopt, NULL, "W H", wh, "640 480", "width height");
    CHECK_VALID("kind-3 unflagged two ints");
  }

  /* kind 4: single variadic parameter (flagged; default must be a plain word)
   */
  {
    hestOpt *hopt = NULL;
    int thresh = 0;
    hestOptAdd_1v_Int(&hopt, "t", "thresh", &thresh, "128", "threshold");
    CHECK_VALID("kind-4 int with plain-word default");
  }

  /* kind 5: multiple variadic parameters (flagged; sawP required) */
  {
    hestOpt *hopt = NULL;
    int *vals = NULL;
    unsigned int saw = 0;
    hestOptAdd_Nv_Int(&hopt, "x", "val", 1, -1, &vals, "0", "values", &saw);
    CHECK_VALID("kind-5 flagged variadic ints");
  }

  /* kind 5: multiple variadic strings (flagged) */
  {
    hestOpt *hopt = NULL;
    char **fnames = NULL;
    unsigned int saw = 0;
    hestOptAdd_Nv_String(&hopt, "i", "file", 1, -1, &fnames, NULL, "input files", &saw);
    CHECK_VALID("kind-5 flagged variadic strings");
  }

  /* kind 5: multiple variadic, unflagged (the only unflagged variadic allowed)
   */
  {
    hestOpt *hopt = NULL;
    char **args = NULL;
    unsigned int saw = 0;
    hestOptAdd_Nv_String(&hopt, NULL, "arg", 1, -1, &args, NULL, "positional args",
                         &saw);
    CHECK_VALID("kind-5 unflagged variadic strings");
  }

  /* Mixed array: flag + fixed + variadic */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    float f = 0;
    float *fs = NULL;
    unsigned int saw = 0;
    hestOptAdd_Flag(&hopt, "v", &v, "verbose");
    hestOptAdd_1_Float(&hopt, "x", "val", &f, "0", "a float");
    hestOptAdd_Nv_Float(&hopt, "xs", "vals", 1, -1, &fs, "0", "floats", &saw);
    CHECK_VALID("mixed flag + fixed + variadic");
  }

  /* short,long flag syntax */
  {
    hestOpt *hopt = NULL;
    int n = 0;
    hestOptAdd_1_Int(&hopt, "n,num", "count", &n, "0", "count");
    CHECK_VALID("short,long flag");
  }

#undef CHECK_VALID
  return 0;
}

/* =========================================================================
 * Tier 1d: hestOptCheck — invalid configurations
 * =========================================================================
 *
 * For each case: build the (invalid) hestOpt, expect hestOptCheck to return 1,
 * then clean up.
 *
 * Note on what can be tested via the public API:
 * hestOptSingleSet already enforces certain invariants before storing fields:
 *   - sawP is zeroed out for non-kind-5 options
 *   - enm is zeroed out for non-airTypeEnum options
 *   - CB  is zeroed out for non-airTypeOther options
 * So hest__OPCheck's guards against these mismatches protect against direct
 * struct manipulation, not API misuse, and are not testable here.  The cases
 * below cover every error path that IS reachable via hestOptAdd_nva.
 */

/* Helper: call hestOptCheck on hopt, free the error string, free hopt.
   Returns 0 if hestOptCheck correctly returned 1 (i.e. caught the error);
   writes to err and returns 1 if hestOptCheck unexpectedly passed. */
static int
expect_invalid(hestOpt *hopt, const char *ctx, char err[AIR_STRLEN_HUGE + 1]) {
  char *chkErr = NULL;
  int ret = hestOptCheck(hopt, &chkErr);
  free(chkErr);
  hestOptFree(hopt);
  if (!ret) {
    FAILF("%s: hestOptCheck passed but should have failed", ctx);
  }
  return 0;
}

static int
test_optcheck_invalid(char err[AIR_STRLEN_HUGE + 1]) {

  /* NULL hopt: hest__OPCheck handles this explicitly. */
  {
    char *chkErr = NULL;
    int ret = hestOptCheck(NULL, &chkErr);
    free(chkErr);
    if (!ret) FAIL("NULL hopt should fail hestOptCheck");
  }

  /* kind 1 (min=max=0) with a dflt string set. */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    hestOptAdd_nva(&hopt, "v", NULL, airTypeBool, 0, 0, &v,
                   "0", /* dflt: forbidden for kind 1 */
                   "verbose", NULL, NULL, NULL);
    if (expect_invalid(hopt, "kind-1 with dflt", err)) return 1;
  }

  /* kind 1 with no flag (stand-alone flags must have a flag). */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    hestOptAdd_nva(&hopt, NULL, NULL, airTypeBool, 0, 0, &v, NULL, "verbose", NULL, NULL,
                   NULL);
    if (expect_invalid(hopt, "kind-1 with no flag", err)) return 1;
  }

  /* kind 1 with a name set (stand-alone flags must NOT have a name). */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    hestOptAdd_nva(&hopt, "v", "something", airTypeBool, 0, 0, &v, NULL, "verbose", NULL,
                   NULL, NULL);
    if (expect_invalid(hopt, "kind-1 with name", err)) return 1;
  }

  /* Non-kind-1 option with no name (all non-flag options require a name). */
  {
    hestOpt *hopt = NULL;
    int n = 0;
    hestOptAdd_nva(&hopt, "n", NULL, airTypeInt, 1, 1, &n, "0", "count", NULL, NULL,
                   NULL);
    if (expect_invalid(hopt, "kind-2 with no name", err)) return 1;
  }

  /* Flag starting with '-': confusing and forbidden. */
  {
    hestOpt *hopt = NULL;
    int n = 0;
    hestOptAdd_nva(&hopt, "-n", "count", airTypeInt, 1, 1, &n, "0", "count", NULL, NULL,
                   NULL);
    if (expect_invalid(hopt, "flag starting with '-'", err)) return 1;
  }

  /* Empty flag string. */
  {
    hestOpt *hopt = NULL;
    int n = 0;
    hestOptAdd_nva(&hopt, "", "count", airTypeInt, 1, 1, &n, "0", "count", NULL, NULL,
                   NULL);
    if (expect_invalid(hopt, "empty flag string", err)) return 1;
  }

  /* Incompatible min > max (and max != -1): hestOptSingleSet sets kind = -1. */
  {
    hestOpt *hopt = NULL;
    int vals[4] = {0};
    hestOptAdd_nva(&hopt, "x", "vals", airTypeInt, 3, 2, vals, NULL,
                   "three or more vals", NULL, NULL, NULL);
    if (expect_invalid(hopt, "min(3) > max(2)", err)) return 1;
  }

  /* Unflagged kind-4 (min=0, max=1, no flag): forbidden because min must be >=
     1 for unflagged options of kind 1 or 4. */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    hestOptAdd_nva(&hopt, NULL, "thresh", airTypeInt, 0, 1, &v, "128", "threshold", NULL,
                   NULL, NULL);
    if (expect_invalid(hopt, "unflagged kind-4 (min=0)", err)) return 1;
  }

  /* kind-4 (min=0, max=1, flagged) with a default that is not a plain word.
     The space in "one two" makes it non-plain; a single number like "128" would
     pass. */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    hestOptAdd_nva(&hopt, "t", "thresh", airTypeInt, 0, 1, &v,
                   "one two", /* non-plain-word default */
                   "threshold", NULL, NULL, NULL);
    if (expect_invalid(hopt, "kind-4 with non-plain-word dflt", err)) return 1;
  }

  /* kind-4 with an empty default string: also not a plain word. */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    hestOptAdd_nva(&hopt, "t", "thresh", airTypeInt, 0, 1, &v,
                   "", /* empty: not a plain word */
                   "threshold", NULL, NULL, NULL);
    if (expect_invalid(hopt, "kind-4 with empty dflt", err)) return 1;
  }

  /* kind-4 with a disallowed type (airTypeString). */
  {
    hestOpt *hopt = NULL;
    char *s = NULL;
    hestOptAdd_nva(&hopt, "s", "str", airTypeString, 0, 1, &s, "hello", "a string", NULL,
                   NULL, NULL);
    if (expect_invalid(hopt, "kind-4 with airTypeString", err)) return 1;
  }

  /* kind-4 with a disallowed type (airTypeEnum). */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    /* note: airTypeEnum with a NULL enm also triggers the enum-check; we
       need both errors but only the first fires — either is correct here */
    hestOptAdd_nva(&hopt, "e", "val", airTypeEnum, 0, 1, &v, "true", "a flag", NULL,
                   airBool, NULL);
    if (expect_invalid(hopt, "kind-4 with airTypeEnum", err)) return 1;
  }

  /* kind-5 with NULL sawP: required for all kind-5 options. */
  {
    hestOpt *hopt = NULL;
    int *vals = NULL;
    hestOptAdd_nva(&hopt, "x", "vals", airTypeInt, 1, -1, &vals, "0", "values",
                   NULL, /* sawP: NULL — this is the error */
                   NULL, NULL);
    if (expect_invalid(hopt, "kind-5 with NULL sawP", err)) return 1;
  }

  /* airTypeEnum with NULL enm. */
  {
    hestOpt *hopt = NULL;
    int v = 0;
    hestOptAdd_nva(&hopt, "e", "val", airTypeEnum, 1, 1, &v, "true", "an enum val",
                   NULL, /* sawP (not kind-5, so ignored) */
                   NULL, /* enm: NULL — this is the error */
                   NULL);
    if (expect_invalid(hopt, "airTypeEnum with NULL enm", err)) return 1;
  }

  /* airTypeOther with NULL CB. */
  {
    hestOpt *hopt = NULL;
    int v = 0; /* placeholder; type is "other" so size is from CB */
    hestOptAdd_nva(&hopt, "o", "val", airTypeOther, 1, 1, &v, "x", "an other val",
                   NULL,  /* sawP */
                   NULL,  /* enm */
                   NULL); /* CB: NULL — this is the error */
    if (expect_invalid(hopt, "airTypeOther with NULL CB", err)) return 1;
  }

  /* Two unflagged variadic (kind-5) options: at most one is allowed. */
  {
    hestOpt *hopt = NULL;
    int *a = NULL, *b = NULL;
    unsigned int sawA = 0, sawB = 0;
    hestOptAdd_nva(&hopt, NULL, "first", airTypeInt, 1, -1, &a, NULL, "first args",
                   &sawA, NULL, NULL);
    hestOptAdd_nva(&hopt, NULL, "second", airTypeInt, 1, -1, &b, NULL, "second args",
                   &sawB, NULL, NULL);
    if (expect_invalid(hopt, "two unflagged variadic options", err)) return 1;
  }

  return 0;
}

/* =========================================================================
 * Test table and main
 * ========================================================================= */

static const TestEntry tests[] = {
  {"plain_word_chars", test_plain_word_chars,
   "hest__PlainWord: each printable ASCII char"},
  {"plain_word_strings", test_plain_word_strings,
   "hest__PlainWord: multi-character strings"},
  {"sprint_plain", test_sprint_plain, "hestArgVecSprint: plain words"},
  {"sprint_quoted", test_sprint_quoted, "hestArgVecSprint: words needing quoting"},
  {"optcheck_valid", test_optcheck_valid, "hestOptCheck: valid option configurations"},
  {"optcheck_invalid", test_optcheck_invalid,
   "hestOptCheck: invalid option configurations"},
  {NULL, NULL, NULL}};

int
main(int argc, char **argv) {
  /* Teem startup sanity check. */
  {
    int insane = airSanity();
    if (insane) {
      fprintf(stderr, "airSanity failed: %s\n", airInsaneErr(insane));
      return 1;
    }
  }

  /* Optionally filter to a single named test. */
  const char *only = (argc >= 2) ? argv[1] : NULL;

  int total = 0, passed = 0, failed = 0;
  char err[AIR_STRLEN_HUGE + 1];

  for (int i = 0; tests[i].name; i++) {
    if (only && strcmp(only, tests[i].name) != 0) continue;
    total++;
    err[0] = '\0';
    int ret = tests[i].fn(err);
    if (ret) {
      fprintf(stderr, "[FAIL] %s: %s\n", tests[i].name, err);
      failed++;
    } else {
      printf("[PASS] %s\n", tests[i].name);
      passed++;
    }
  }

  if (only && total == 0) {
    fprintf(stderr, "No test named \"%s\".  Available tests:\n", only);
    for (int i = 0; tests[i].name; i++) {
      fprintf(stderr, "  %-26s  %s\n", tests[i].name, tests[i].desc);
    }
    return 1;
  }

  if (!only) {
    printf("---\n%d passed, %d failed (of %d)\n", passed, failed, total);
  }
  return failed ? 1 : 0;
}
