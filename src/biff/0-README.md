# All about `/* Biff: */` annotations (new with TeemV2)

Teem uses its `biff` library to collect error messages (based on 90s skater culture using "biff" to mean a hard fall). `biff` is not thread-safe, but that has never been the biggest barrier to its use (or Teem's use in general). Instead, a bigger problem is that there's nothing in a function type signature that advertises whether it uses `biff` at all, or what return values from the function indicate the error conditions that would have generated `biff` messages that need to be collected and passed along to the user. Thankfully, there are now some semi-automated tools to help with this.

`teem/src/_util/scan-symbols.py -biff` (which performs something that for no good reason started being called the "biff auto-scan") will scrutinize Teem source code to look at how it uses `biff`. This analysis creates parsable annotations of function definitions to automate whether and how `biffGetDone` is called in response to an error (in, say, a Python wrapper), as documented below.

Even though these annotations were motivated by the needs of Python wrapping (which is only wrapping things in the public API), GLK decided to also do these annotations for "private" functions (which are are available for linking in the library, but are declared in privateLib.h rather than lib.h), and even for static functions that do use biff. The idea is that this is potentially useful information for further analysis or for human coding, and its better to err on the side of more info, discretely packaged, when the quality/correctness of the information is high.

NOTE that the `Biff` annotation on a function currently reflects a simplistic textual analysis of that function code, saying "it looks like this function uses biff in this way." This is not based on any proper parsing of the code AST, so calls to `biff` could be hidden behind a `#define`, and there is certainly no way to know (without execution) whether any other functions called from this function used biff. The formatting induced by the newly adopted `clang-format` is a big help. In any case this seems adequate for Teem's Python wrapping error handling.

Here is an example annotation from `teem/src/nrrd/subset.c`:

```
int /* Biff: 1 */
nrrdSlice(Nrrd *nout, const Nrrd *cnin, unsigned int saxi, size_t pos) {
```

The annotations are a one-line comment, always on the line with the function return type, which is above the function name (this formatting is enforced by new use of `clang-format`). **NOTE** that in Teem code, the remainder of the line after the function return type (with the function name on the next line) is reserved for these kinds of adhoc function annotations. Human-written comments about the return type/qualifers need to be in the previous line. In the future this space may be used for other annotations giving other non-biff info about the function.

The single-space-separated words in the comment are, in order:

#### Required:

- `Biff:` : the information in this annotation has been manually verified or,
- `Biff?` : this annotation automatically generated, and needs verification. `teem/src/\_util/scan-symbols.py -biff` _only_ produces `Biff?` annotations.

#### Optional:

`(private)` : this function is private (as described above). Otherwise, from the qualifiers on the return type of the function (the same line as this annotation), `static` will mean that the function is static, while not having `static` (and absent `(private)`) means the function is declared in `lib.h` and intended for external linkage. Such "private" functions probably aren't even in a python wrapper, but the fact of being private is nice to record once known, since you can't tell by looking at a function definition where it has been declared. Despite the tendency, there is no requirement in Teem code that private function names start with a single `_` underscore.

#### Required:

- `<val>` : The return value `<val>` indicates a biff-reported error, i.e., if the function returns `<val>` then someone needs to retrieve the biff error message. `<val>` must not contain `|`, `:`, or whitespace, and cannot be `nope` or `maybe`. `<val>` is just a string (since it is in a comment), but hopfully it is parsable as the function return type (on this same line, before the comment containing this annotation). Simple integers are easy, but it could get trickier: example returns (currently used in Teem) include `NULL`, `EOF`, `AIR\_FALSE`, `AIR\_NAN`, `UINT_MAX`, `Z_STREAM_ERROR`, and `nrrdField_unknown`. The point is: be prepared to do some work if you're trying to parse and act on Biff annotations.
- or `<v1>|<v2>` : A return value of either `<v1>` or `<v2>` indicates an error has been recorded in `biff`
- or `<v1>|<v2>|<v3>` : Error values are `<v1>` or `<v2>` or `<v3>` (and so on)
- or `maybe:<N>:<val>` : This function uses something like `biffMaybeAddf()`, which may or may not set a biff error message, depending on the value of one of the function parameters (always called `useBiff`, as enforced by biff auto-scan). `useBiff` is the Nth function parameter, in the _1_-based numbering of the function parameters.
- or `nope` : This function does not use `biff`. The function may usefully communicate something about how things went wrong by a returning one of some possible error return values, but that isn't documented here because it doesn't involve `biff`. (Why `nope`: it won't be confused for anything else, and GLK had just seen the excellent [movie of the same name](<https://en.wikipedia.org/wiki/Nope_(film)>))

#### Optional:

`# <comments>` : anything after a `#` is ignored by an annotation parser

## More examples:

```
int /* Biff: (private) maybe:2:nrrdField_unknown */
_nrrdReadNrrdParseField(NrrdIoState *nio, int useBiff) {
```

```
static int /* Biff: nope # unlike other parsers, for reasons described below */
_nrrdReadNrrdParse_number(FILE *file, Nrrd *nrrd, NrrdIoState *nio, int useBiff) {
```

You can see the variety of Biff annotations by, from the top-level teem directory (the one with `air`, `biff`, `hest`, etc as subdirectories), running:

```
grep 'Biff:' */*.c | cut -d: -f 2- | cut -d/ -f 2- | cut -d' ' -f 2- | cut -d\* -f 1 | sort | uniq
```

## What creates `Biff` annotations

The annotations are created with the help of `../_util/scan-symbols.py`, which is re-run as needed when the API changes, and prior to releases. Later, the `Biff:` annotations are read by `../_util/gen_biffdata.py`, which repackages the info and save it into `../../python/cffi/biffdata/*.csv`, one file per Teem library.

`scan-symbols.py` uses the per-library `GNUmakefile`s to make `.a` library archive file to look at with `nm`. Supposing you have your teem source checkout in `~/teem`, then from the `~/teem/src/_util` directory:

```
unset TEEM_SRC
python3 scan-symbols.py ~/teem -biff 3 gage
```

The different `-biff _n_` levels:

- `-biff 1` is just for observing `biff` usage;
- `-biff 2` is for doing annotations where none have been done before, and
- `-biff 3` will over-write old comments and wrong annotations.
  But actually, no original source `.c` files are actually modified over-written, instead new files are created, as described by `scan-symbols.py`, eg:

```
wrote 2 annotations in miscGage-annote.c
wrote 5 annotations in kind-annote.c
wrote 6 annotations in shape-annote.c
```

Thus, it is files ending with `-annote.c` that are written or over-written by `scan-symbols.py`. In a way that helps ensure Teem libraries have the symbols intended, `scan-symbols.py` will loudly fail if it detects that a link-able symbol in library `lib` does not start with `lib` or `\_lib`.

Then to process these in, e.g. `~/teem/src/gage`, in file `miscGage.c`:

```
diff miscGage{-annote,}.c # to inspect what biff auto-scan wrote
mv miscGage{-annote,}.c # to start editing
# open miscGage.c for editing, confirm each annotation,
# change "Biff?" to "Biff:" once every annotation is confirmed.
git diff -U0 miscGage.c # to confirm what was changed
git commit ...
```
