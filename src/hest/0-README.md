# `hest`: command-line parsing

## Introduction

Like other command-line parsers, `hest` creates a command-line interface to a C program according to how a sequence of command-line options is specified. `hest` thereby bridges from the `argc`/`argv` command-line to a collection of C variables that need to be given values, so that a C program can usefully run. Unlike most other C command-line parsers, the variables can be of most any type (boolean, `int`, `float`, `char *` strings, or user-defined types), and the variables can hold single values (such as `float thresh`) or multiple values (such as `float RGBA[4]`). `hest` underlies Teem's command-line tools like `unu`. To the extent that `hest` bridges the interactive shell's command-line with compiled C code, it approaches some of the roles that in other contexts are served by scripting languages with C extensions (no matter how ill-advised that is).

I (GLK) created `hest` in 2002 out of frustration with limitations of other C command-line parsing libraries. The [`argtable` (version 1)](https://argtable.sourceforge.io/) library came closest to the needed feature set. It directly inspired `hest`'s array-of-per-option-structs approach, and early versions of `hest` supported `argtable`-style static definition of the `hestOpt` array (though current `hest` usage requires dynamic creation of the option array). The newer [Argtable3](https://www.argtable.org/) looks great, but still does not directly support response files or parsing arbitrary objects via callbacks. The `hest` code was revisited in 2023 to add long-overdue support for `--help`, and to add typed functions for specifying options like `hestOptAdd_4_Float`. Entirely rewriting the code in 2025 fixed long-standing bugs with how quoted strings were handled and how response files were parsed. The rewrite also added `-{`, `}-` command-line comments and other new features. Revisiting the re-write in 2026 resulted in more rigorous testing, and resolving some long-ambiguities about handling options that take zero or more parameters.

`hest` is powerful and not simple. This note attempts to give a technical description useful for anyone considering using `hest`, as well as anyone trying wrap their head around the `hest` source code, including GLK.

First, some examples ...

More examples may help show `hest`'s utility ... (IN PROGRESS) ...
... Give some specific examples, flagged and unflagged ...
... show a response file being used, show -{ }- commenting
... show final unflagged variadic to consume "operands"

## Terminology and concepts

`hest` has possibly non-standard terminology for the elements of command-line parsing. Here is a bottom-up description of the command-line and how `hest` interprets it:

- What `main()` gets as `char *argv[]` is a vector, length `int argc`, of _arguments_ or _args_; each one is a `char*` string. An arg can contain spaces and other arbitrary characters if the user quoted strings or escaped characters; that is between the user and the shell (the shell tokenizes the typed command-line into `argv`).
- Arguments like `-v` and `-size`, which identify a variable to set, are called _flags_. Some flags are really just flags; no further information is given beyond their presence or absence. Other flags introduce subsequent arguments that together supply information for setting a variable.
- The sub-sequence of arguments (perhaps following a flag) that together give information for setting a variable are called _parameters_ or _parms_. There is some slippage of terminology between the `char *` string argument that specifies the parameter, and the value (such an `int`) parsed from that parameter string.
- Separately, and possibly confusingly, `hest`'s behavior has many knobs and controls, stored in the `hestParm` struct. The pointer-to-struct is always named `hparm` in the code, to try to distinguish it from the parameters appearing on the command-line.
- Within `hest`, one _option_ determines how to set one C variable. In the C code, one `hestOpt` struct stores everything about how to parse one option _plus_ intermediate state during the parsing process _plus_ the final results of that parsing. An array of `hestOpt` structs (which we can call an _option array_) is how a `hest`-using program communicates what to learn from the command-line. The `hestOpt` array is built up by repeated calls to the `hestOptAdd_` family of functions.
- On the command-line, the option may be communicated by a flag (e.g. `-sz`) and its associated parms (e.g. `3 640 480`); this is a _flagged_ option. Or, an option may be _unflagged_, or what others call "positional", because assigning a given argument to some option is determined by the position of the argument relative to others, along with the relative ordering unflagged option within the `hestOpt` struct array.
- An option may have no parms, a _single_ parm, a _fixed_ number of _multiple_ parms, or a variable number of parms. Options with a variable number of parms are called _variadic_ options (to distinguish from the C _variable_ that the option sets). With `mv *.txt dir`, the `*.txt` filenames could be parsed as the parms for an unflagged variadic option, and `dir` would be a fixed single parm for a second unflagged option. An option's _kind_ is a way to organize the different possibilities of how many parms the option wants.
- The _type_ of the option is the type of C value that is set from one parm. Hest supports a range of C numeric types, booleans, characters, strings, as well as other user-defined types: a flexible mapping between strings and integer values defined by an `airEnum` struct pointer, or a more general user-defined creation of a C struct by some parsing of a single arg string (either statically or dynamically allocated).
- Sometimes multiple command-line options need to be saved and re-used together, over a time span longer than any one shell. Command-line options can thus be stored in _response files_, and the contents of response files are effectively expanded into the command-line wherever they are named with `@`, e.g. `@resp.txt`. Response files can have comments (from `#` to end of line, just like shell scripts), and response files can in turn name other response files.
- The main `hest` function that does the parsing is `hestParse`. Its job is to set one variable (which may have multiple components) for every `hestOpt`. Information for setting each variable can come from the command-line, or from the default string stored in the `hestOpt`, but it has to come from somewhere. In this sense, `hest`'s "options" are badly named, because they are not really optional.
- Like `git` and other modern command-line tools, `unu` and other Teem utilities have sub-commands (`unu make`, `unu slice`, `unu project`, ...). The implementation of sub-commands is not part of `hest`, but rather the `unrrdu` library first created to implement `unu`.

Pre-dating `hest` are the [POSIX conventions for command-line arguments](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html#tag_12_01), wherein the elements of `argv` can be first "options" and then "operands", where "options" are indicated by an initial `-` character, and may have zero or more "option-arguments" (what `hest` calls "parameters"). However, `hest` does not attempt to follow POSIX conventions (or terminology) for command-line usage. In particular, `hest` has no idea of an "operand": `hest` tries to interpret _every_ arg in `argv` as a part of some (possibly unflagged) option. Disregard for POSIX has not limited `hest`'s expressivity and flexibility, or its utility for the scientific computing and visualization contexts that it was built for.

## The different `kind`s of options, and how to `hestOptAdd_` them.

The complexity of `hest` arises partly from the range of types it supports, but mainly from how it describes the number of command-line args that should be consumed as parameters for an option, or the _`kind`_ of the option. Each option is entirely specified by one `hestOpt` struct. Two important yet unconventional fields in the `hestOpt` are `min` and `max`: the min and max number of parms that may be parsed for that option. All the various traditional uses of the command-line can be parameterized in terms of `min` and `max`, even though they aren't normally characterized this way, and even though it includes rarely-used cases. After recognizing the expressivity of parameterizing options in terms of `min` and `max`, `hest` implementation was organized around that.

The following ASCII-art illustrates:

- The `min` number and `max` number of parms along the horizontal and vertical axes, respectively
- The associated numeric `kind`, shown as `(`_k_`)`; for _k_ = 1,2,3,4,5
- Prose name for that kind of option, which appears in the code and a description of its operation.
- Which `hestOptAdd_` function or family of functions is used to parse that kind of option. Here, `hestOptAdd_` is abbreviated to `hOA_` and the final `_T` stands for the type `T`. `T` is one of: `Bool`, `Short`, `UShort`, `Int`, `UInt`, `Long`, `ULong,`, `Size_t`, `Float`, `Double`, `Char`, `String`, `Enum` (an `airEnum`), or `Other` (parsed via callback).

```
    |               .                      /       .       /
    |               .                   /       .       /
    |               .                /       .       /
    |         (5) multiple          | (3) multiple
 2--|            variadic           |    fixed
    |             parms             |    parms
    |            hOA_Nv_T           |  hOA_{2,3,4,N}_T
    |...............................|/
    |  (4) single   |  (2) single
 1--|    variadic   |     fixed
    |      parm     |     parm
    |    hOA_1v_T   |    hOA_1_T
    |...............|/
    | (1) stand-alone
 0--|      flag;
    |    no parms
    |    hOA_Flag
 ^  |/_____________________________________________
max          |              |              |
    min >    0              1              2
```

Parameterizing with `min` and `max` allows you to specify an option that needs, for example, 3 or more arguments, or between 1 and 4 arguments. This is useful for various technical settings, but is not as straightforward in something like Python's `argparse`. Relative to `argparse` and its `nargs` argument to `.add_argument()`:

- `nargs=N` with `N`>1 is hest's `kind`=3
- `nargs=1` is `kind`=2
- An on/off flag with `action='store_true'` is `kind`=1
- `nargs=?` is `kind`=4
- `nargs=*` is `kind`=5 with `min`=0 and `max`=-1
- `nargs=+` is `kind`=5 with `min`=1 and `max`=-1

Most of the time the details of `kind` will not matter. The end-users of `hest`-based programs certainly never need to worry about `kind`, and the `hest` API for creating a `hest`-based program does not involve `kind`. Still, knowing the different `kind`s is useful for understanding `hest`'s behavior and implementation. Further points about `kind`:

- The `kind` of option is independent of being _optional_ (due to the `hestOpt` having a default string) versus _required_ (because no default string is given). `kind`=1 stand-alone flags have no default, but set an `int` variable to 1 or 0 if the user opted to include the flag on the command-line.
- `kind` is mostly independent of whether an option is flagged or unflagged (i.e. positional). Flags are required for `kind`=1 stand-alone flags (or else it would make no sense) and for `kind`=4 options that take zero or one parm (to reduce confusion).
- `kind` is mostly independent of the option type, the exceptions being `kind`=1 stand-alone flags, and `kind`=4 options, for which the only supported types or boolean or numeric: the semantics of the option only make sense if you can apply C's unary `!` to the value
- Except for the current constraint of allowing _at most one **unflagged** (kind=5) multiple variadic option_, there can be zero, one, or many instances of any kind with an option array.

## Over-all `hestParse` process: Phases and Limitations

Given an `argc`,`argv` command-line and a `hestOpt` option array, `hestParse` goes through **multiple phases** to set the per-option variables:

1. Validate the given `hestOpt` array to ensure that, for example, every option has a non-NULL pointer to a C variable to set.
1. Convert given `argc`,`argv` command-line to an internal (`hestArgVec`) representation of the argument vector. This is the phase in which response files are expanded and `-{`,`}-` comments are handled, resulting in a new effective command-line that we'll still call "the command-line" for this account of the phases.
1. Extract from the command-line all the arguments associated with flagged options: the flag args plus any immediately subsequent associated parm args. A flagged option with variadic parms consumes (from the command-line) the flag and following args up to its _maximum_ number of parms, stopping at another flag, the end of the command-line, or `--` as described below. Arguments are transferred (as strings, without any parsing) to a per-option vector of args inside the `hestOpt` struct, over-riding any default string for that option.
1. What remains in the command-line should be args for unflagged (positional) options, which consume the args in order (first option consuming the first arg(s)) but not in a greedy way: an unflagged variadic option can be followed by unflagged fixed options.
1. An option that isn't assigned args from the command-line will tokenize its own default string to populate its per-option arg vector. Each `hestOpt` remembers the source of information used to set the variable (command-line vs response-file vs default string).
1. By this point every option should have something to parse: whether by command-line, response file, or default, _every_ option _must_ end up with zero or more arg strings, or else this is a run-time error. `hestParse` finishes by, for each option, parsing the option's arg vector to set the option's C variable.

If there are no variadic options (i.e. limiting oneself to kinds 1, 2, and 3), then the assignment of a command-line arg to a `hestOpt` is straight-forward, even with unflagged options and even with flagged options being able to appear on the command-line in any order. Variadic parameters do complicate the mapping of args to options. Considering the `mv *.txt dir` example: what if instead of just a single destination `dir` there were multiple `dir1 dir2 ...` directories (all of which were to get copies of the `.txt` files)? There would be no trivial way for `mv` to know when the `*.txt` args end and the `dirI` args begin. Somewhat related to this, fans of [POSIX Utility Guidelines](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html#tag_12_02) may also recall how Guideline 10 describes the role of `--`:

> The first -- argument that is not an option-argument should be accepted as a delimiter indicating the end of options. Any following arguments should be treated as operands, even if they begin with the `-` character. So `--` marks the end of some "option-arguments".

With this context, some further details and limitations of `hest`'s processing can be outlined, including the role of `--`:

- As noted above, there currently can be at most one _unflagged_ option with multiple variadic parms (`kind`=5). Having more than one will likely create user confusion about which option gets which args. There is hopefully no good reason why you can't instead use more than one _flagged_ option with multiple variadic option. It is okay to have one unflagged multiple variadic option alongside other unflagged options with multiple _fixed_ parms.
- For any variadic option, _flagged_ or _unflagged_, `--` explicitly marks the end of args for the option. This use of `--` can often be avoided: any other option flag also marks the end of variadic args, as does the end of the command-line. Thus for `hest` the `--` mark is for indicating the end of one specific variadic option, rather than (as is POSIX convention) than separating _all_ options from all subsequent "operands". For a variadic option with `min`=0 (the option can take zero or more parms), a `--` alone on the command-line is how the user explicitly says "I am passing zero arguments (possibly overriding the default)". This disambiguates from not giving any args on the command-line, meaning "I am not setting this option", and thereby falling back on its default.
- `hestParse` strives to interpret the **entire** given `argc`,`argv` argument vector; there is currently no way to tell `hest` (via `--` or otherwise): "stop processing `argv` here, and leave the rest as operands for something else outside `hest` to interpret". If you want to collect some variable number of "operands" at the end of the command-line, these can be recovered as the parms of a final unflagged variadic option of type `String`.
- Arguments for options (flagged or unflagged) can only come from the end-user or from the option's default string, but not from some mix of both: there is no way to supplement arguments from the user with more from the default string or vice versa. Though it may be more confusing than helpful, the user can set options from a mix of the command-line and response files, since response files are effectively expanded into the command-line prior to any per-option processing.
- Flagged options can appear in any order on the command-line, and the same option can be repeated on the command-line: the last appearance over-rides all earlier appearances, rather than appending to them. `hest` currently cannot remember multiple occurances of repeated options (is is used in, say, `sed -e ... -e ... `. )
- `hest` currently does not support combining single-character flags, like how `ls -lt` means `ls -l -t`.

## `hestParse` orderings of arguments and options

Understanding the behavior of `hestParse` requires defining various orderings. First, there is the _appearance order_ of _arguments_ on the command-line (after in-place expansion of any response files). For any option (flagged or unflagged, variadic or fixed) with multiple parms, if the option consumes args from the command-line, the appearance order of the args _is_ the order of the parms within the option. Then, there are **3** kinds of ordering among the set of _options_:

- _Definition order_: The order in which the `hestOpt` option array is built up by calls to `hestOptAdd` functions, which is the same as the order in which the options are stored in the `hestOpt *hopt` array. In any usage information (from `--help` or `--usage`), options are described in definition order.
- _Consumption order_: The order in which options consume arguments from the command-line in Phases 3 and 4 above.
- _Parsing order_: The order in each option's assigned (string-valued) arguments are parsed (with things like `sscanf`) to recover the values to set in the associated C variable.

Between flagged options (whether variadic or fixed), the definition order does not affect the (Phase 3) consumption order: args containing a flag and parms for a flagged option are consumed in command-line appearance order, with later appearances of the same flagged option overriding any previous ones. If a flagged option appears on the command-line it overrides its default; flagged options not appearing rely on their default.

Among unflagged (positional) options, however, Phase 4 consumption order is more subtle:

- If there is no unflagged variadic multiple (kind-5) option, then options' consumption order follows definition order: the first unflagged option to consume args is the first unflagged option in the definition order, and subsequent unflagged options in the definition order consume their parameter arguments from the beginning of the remaining command-line. This is straight-forward because all the unflagged options are fixed (the number of expected parms is fixed).
- If there is one unflagged variadic multiple (kind-5) option (and there can be at most one), then consumption by any other unflagged (fixed, non-variadic) options happens in two passes: _first going forwards_ from the beginning of definition order up until and stopping before the kind-5 option, _and then going backwards_ from the end of definition order. This is how non-greedy consumption of args by the kind-5 option is implemented. Within the second backward pass, args are still assigned to options in left-to-right (appearance) order. Finally, any remaining args are consumed by the kind-5 option.
- Within the consumption order just described, each unflagged option asks: "are there remaining command-line arguments for me to consume?" If yes, that option consumes them, and thereby overrides its default string. The presence of a default string does not inform whether an unflagged option tries to consume args from the command-line, since otherwise the mapping of args to options would be too complicated to reason about, even if it was deterministic.

Once every option has been assigned args, every option can independently parse its own args to set its C variable, so a separate _parsing order_ seems moot. It makes sense, however, to enable parsing some options last, so that any parsing errors in all the other options can be detected (and complained about to the user) sooner rather than later. For example, we don't want to wait to read in a large `-i` input data file only to _then_ discover that some other `-sz` option couldn't parse the three integers it needs. But it also makes sense for usage and glossary documentation to have `-i` come before `-sz` in definition order. Thus, each `hestOpt` has an integer `priority` value (defaulting to 0) that can be used to move it earlier (higher `priority`) or later (lower `priority`) in parsing order. **The parsing order is determined by first traversing all priority levels used (from high to low), and then within one priority level, the definition order determines the parse order.**

Even though it thus possible to control the relative ordering of option parsing, options are still parsed independently: there is as yet no simple way for the parsing of one option to use information generated by the previous parsing of another option.

## Tokenizing and representing values

All values set in per-`hestOpt` C variables start as `char*` strings, so the process of setting values involves two kinds of string operations: tokenizing into (string-valued) arguments, and parsing arguments into values. Tokenizing is described here, with reference to the Phase numbers in "Over-all `hestParse` process: Phases and Limitations".

Before Phase 2, the shell already did tokenizing to turn the single line of text you type on the command-line into `argc`,`argv`. In the case of a `@resp.txt` response file, however, the file contents are read in and tokenized by `hest` in way that intends to be indentical to the shell, and inserted in that place of the argument sequence. Response file expansion includes handling [single- vs double-quoted strings](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_02) and [token formation](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_03), although `hest` (since it is not the shell) does not attempt anything with parameter expansion, command history, command substitution, or arithmetic expansion.

Besides expanding response files, `hest` does additional processing in Phase 2 on the argument sequence as part of building up its internal `hestArgVec` representation to handle `-{`,`}-` comments. While response files do support `#`-to-end-of-line comments, `hest` recognizes that sometimes you want to add comments to (or comment out part of) a command-line, whether in the shell directly or in a response file. As long as `hestParm->respectDashBraceComments` is non-zero (as it defaults to), then `-{` and `}-` arguments will be ignored, as well as any intervening arguments. Also, `-{`,`}-` comments can be nested. To be recognized as distinct tokens by the shell, the `-{` and `}-` sequences have to be space-separated from any other tokens; this will prevent the shell from attempting any brace expansion.

Phases 3 and 4 extract (or consume) user-supplied arguments from the command-line `hestArgVec` and assign them to individual options by moving each arg into one of the per-option arg vectors of strings (`hestOpt->havec`), according to the description of consumption order above.

In Phase 5, non-NULL per-option default strings (`hestOpt->dflt`) are used to populate the per-option arg vectors that weren't already set from the user. `hestOpt->dflt` being NULL is how the `hest`-using programmer signifies that this option (except for `kind`=1 stand-alone flags) requires user specification. It might be reasonable for a programmer-supplied default value for the per-option C variable to actually be of that same variable type, instead of only being a string. `hest`'s design, however, embraces the universal expressivity of strings: as long as `hest` is in the business of parsing values from command-line-supplied strings, in the interests of consistency it may as well use the same machinery to parse values from the default strings. This also helpfully simplifies the storage of default values within the `hestOpt`.

In general, the per-option default string is tokenized to extract multiple args (for that option) just like the shell does and like `hest` handles response file contents, to build up a per-option arg vector (of strings). There are some exceptions to this tokenizing:

- For `kind`=5 (multiple variadic) options with `min`=0, a non-NULL empty default string `""` means: 0 arguments, _even for string-type options_. The non-NULL string means that there is a programmer-supplied default (so the user doesn't need to specify the option), but the emptiness means that 0 args are supplied. If the type is string, use `"''"` to indicate a default value of a single empty string.
- For `kind`=2 (single fixed) options of type string: The default string value _is_ the default string, without any tokenization or quoting. The programmer is already quoting the string literal to make it a string; they shouldn't have to do additional quoting just because that string may contain a space.

## Parsing values

The final phase (Phase 6) of `hestParse` is to convert string-valued per-option arg vectors into values that can be set in the per-option C variable. The following account of he different `kind`s refers to the `void *valueP` field of the `hestOpt`, which is how the user identifies (the address of) the C variable to set for that option. The C variable will have a specific type, which is communicated to `hest` via the `int type;` field of the `hestOpt`, which takes on values from the `airType`_`X`_ enum values defined in `hest.h`. The type is also part of the name of the various `hestOptAdd_` functions in the API outlined above (e.g. calling `hestOptAdd_1_Bool` will create option for which `type == airTypeBool`). Describing how a single value of some type is set is easier with expressions like _`type`_ `*vv` to identify a new variable `vv` that points to some C type _`type`_:

- Both the `Bool` and the `Enum` types in `hest` assume `int *vv`.
- Other scalar types (`Short`, `UShort`, `Int`, `UInt`, `Long`, `ULong,`, `Size_t`, `Float`, `Double`, `Char`) have the expected C types for `*vv`.
- Type `String` assumes `char **vv`, and `*vv` is set to a `strdup()` of the given string.
- For type `Other` hest uses `void *vv` and the user-defined handling in the given `hestCB *CB` struct (see `hest.h`). `CB->parse()` can either set values within memory that `vv` points to, or set `vv` to point to a newly heap-allocated and filled-in space (to be freed later by `CB->destroy`).

Now we can describe how values are set according to option kind:

- `kind`=1 stand-alone flag: No string value is actually parsed. `valueP` is cast to `int *vv`, and `*vv` is set to `0` or `1`, according to whether the flag never appeared (`0`), or if it appeared on the commandline or a response file it referenced (`1`).
- `kind`=2 single fixed parm: `valueP` is cast to a _`type`_ `*vv`, and `*vv` is set according to how the string was parsed.
- `kind`=3 multiple fixed option with `N` parms: cast `valueP` to _`type`_ `vv[`_`N`_`]` and set each `vv[i]` as above from the `N` args consumed by this option
- The `kind`=4 single variadic option has unusual semantics: if the flag does not appear, `*vv` is set to the single value parsed from the (required) default string; let's call this `D`. If the flag does appear but without parms (e.g. `-u` alone), then `*vv = !D`: this is why this kind can only be of a type for which `!` makes sense. If the flag appears with a parm (e.g. `-u 4`), then `*vv` is set to the value parsed from the given parm.
- `kind`=5 multiple variadic option passed `N` parms (where `N` is only learned at run-time): `valueP` is cast to _`type`_ `**vvP`, the address of an array of _`type`_. `*vvP` is set to the newly allocated array of `N` _`type`_, and then all the `(*vvP)[i]` are set individually as above. Then corresponding `hestOpt` has an non-NULL `unsigned int *sawP` field, and the number of values seen is recorded as `*sawP = N`.

## Testing

NOTE: As August 2026, with `clang --version` "Apple clang version 17.0.0 (clang-1700.6.3.2)" and `llvm-cov --version` "Homebrew LLVM version 22.1.0", `clang -DNDEBUG -O0 -fprofile-instr-generate -fcoverage-mapping` and `llvm-cov show` claims that there's at least one uncovered region (in `argvHest.c`), but that seems to be a false positive given all testing and checking that GLK did in `test/coverage`. [The llvm source-based coverage docs](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) says "100% branch coverage for a function implies 100% region coverage for a function" but in `argvHest.c` that is not actually true: there is a zero-length region that may be an instrumentation artifact like the ones generated by switch statements, as acknowledged at the end of the docs.
