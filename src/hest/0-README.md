# `hest`: command-line parsing

## Introduction

Like other command-line parsers, `hest` creates a command-line interface to a C program according to a set of command-line option specifications. With its C89 API, `hest` thereby bridges from the `argc`/`argv` command-line to a collection of C variables that need to be given values, so that a C program can usefully run. Unlike with other C command-line parsers, the variables can be of any type (boolean, `int`, `float`, `char *` strings, plus user-defined types), and the variables can hold single values (such as `float thresh`) or multiple values (such as `float RGBA[4]`), including having the number of values be learned at run-time. `hest` underlies Teem's command-line tools like `unu`. To the extent that `hest` bridges the command-line with compiled C code, it approaches some of the roles that in other contexts are served by higher-level languages with C extensions: `hest`-using programs can run complex computations, even more so with shell scripts around `hest`-using programs. (Still, TeemV2 does include a Python extension wrapping all of Teem's libraries.)

I (GLK) created `hest` in 2002 out of frustration with limitations of other C command-line parsing libraries. The [`argtable` (version 1)](https://argtable.sourceforge.io/) library came closest to the needed feature set. It directly inspired `hest`'s array-of-per-option-structs approach, and early versions of `hest` supported `argtable`-style static definition of the `hestOpt` array (though current `hest` usage requires dynamic creation of the option array). The newer [Argtable3](https://www.argtable.org/) looks great, but does not seem to easily support flagged multiple and variadic options (multiple values after one flag), tokenizing multiple values from default strings, or response files. The `hest` code was revisited in 2023 to add long-overdue support for `--help`, and to add typed functions for specifying options like `hestOptAdd_4_Float`. Entirely rewriting the code in 2025 fixed long-standing bugs with how quoted strings were handled and how response files were parsed. The rewrite also added `-{`, `}-` command-line comments and other new features. Revisiting the re-write in 2026 (prior to the TeemV2 release) created rigorous testing, added some final features (like parsing priority), and resolved some long-standing ambiguities about handling options with zero or more parameters (helped by new `-.` variadic end flag).

`hest` is powerful and not simple. This note attempts to give a technical description useful for anyone considering using `hest`, as well as anyone trying wrap their head around the `hest` source code, including GLK. The `teem-src/src/hest/hest.h` header file includes lovingly written and detailed explanatory comments. The implementation of `hest` uses C99 (but should compile everywhere, including on Windows) and is well-commented.

First, some examples ...

More examples may help show `hest`'s utility ... (IN PROGRESS) ...
... Give some specific examples, flagged and unflagged ...
... show a response file being used, show -{ }- commenting
... show final unflagged variadic to consume "operands"

## Terminology and concepts

`hest` has possibly non-standard terminology for the elements of command-line parsing. Here is a bottom-up description of the command-line and how `hest` interprets it:

- What `main()` gets as `char *argv[]` is a vector, length `int argc`, of _arguments_ or _args_; each one is a `char*` string. An arg can contain spaces and other arbitrary characters if the user quoted strings or escaped characters; that is between the user and the shell (the shell tokenizes the typed command-line into `argv`).
- Arguments like "`-v`" and "`--size`", which identify a variable to set, are called _flags_. Some flags are just stand-alone flags; no further information is given beyond their presence or absence. Other flags introduce subsequent arguments that together supply information for setting a variable.
- The sub-sequence of arguments (perhaps following a flag) that together give information for setting a variable are called _parameters_ or _parms_. There is some slippage of terminology between the `char *` string arg that specifies the parm, and the value (such an `int`) parsed from that parm string.
- Separately, `hest`'s behavior has many knobs and controls, stored in the `hestParm` struct. The pointer-to-struct is always named `hparm` in the code, to distinguish it from the parameters on the command-line.
- Within `hest`, one _option_ determines how to set one C variable. In the C code, one `hestOpt` struct stores everything about how to parse one option _plus_ intermediate state during parsing _plus_ the final results of that parsing. An array of `hestOpt` structs (which we can call an _option array_) is how a `hest`-using program communicates what to learn from the command-line. The `hestOpt` array is built up by repeated calls to the `hestOptAdd_` family of functions.
- On the command-line, the option may be communicated by a flag (e.g. `-sz`) and its associated parms (e.g. `3 640 480`); this is a _flagged_ option. Or, an option may be _unflagged_, or what others call "positional", because assigning a given argument to some option is determined by the position of the argument relative to others, along with the relative ordering of the unflagged options within the option array.
- An option may have no parms, a _single_ parm, a _fixed_ number of _multiple_ parms, or a variable number of parms. Options with a variable number of parms are called _variadic_ options (to distinguish from the C _variable_ that the option sets). With "`mv *.txt dir`", the "`*.txt`" filenames could be parsed as the parms for an unflagged variadic option, and "`dir`" could be a fixed single parm for a second unflagged option. An option's numeric _kind_ is how `hest` organizes the different possibilities of how many parms the option can get.
- The _type_ of the option is the type of C value that is set from one parm. Hest supports a range of C numeric types, booleans, characters, strings, as well as other user-defined types: a flexible mapping between strings and integer values defined by an `airEnum` struct pointer, or a more general user-defined creation of a C struct by some parsing of a single parm string (either statically or dynamically allocated).
- Sometimes multiple command-line options need to be saved and re-used together, over a time span longer than any one shell. Command-line options can thus be stored in _response files_, and the contents of response files are effectively expanded into the command-line wherever they are named with `@`, e.g. `@resp.txt`. Response files can have comments (from `#` to end of line, just like shell scripts), and response files can in turn name other response files.
- The main `hest` function that does the parsing is `hestParse`. A helpful wrapper function `hestParseOrExit` usefully handles things like `--help` documentation generation. The job of `hestParse` is to set one variable (which may have multiple components) for every `hestOpt` in the option array. The parameters for setting each variable can come from the command-line, or from the default string stored in the `hestOpt`, but they have to come from somewhere. In this sense, "option" is an odd term for the unit of `hest` functionality, because an "option" is not really optional.
- Like `git` and other modern command-line tools, `unu` and other Teem utilities have sub-commands (`unu make`, `unu slice`, `unu project`, etc). The implementation of sub-commands is not part of `hest`, but rather the `unrrdu` library within Teem that was first created to implement `unu`.

Pre-dating `hest` are the [POSIX conventions for command-line arguments](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html#tag_12_01), wherein the elements of `argv` can be first "options" and then "operands", where "options" are indicated by an initial `-` character, and may have zero or more "option-arguments" (what `hest` calls "parameters"). `hest` does not strictly follow POSIX conventions or terminology for command-line usage. While `hest` does (by default) respect `--` as the signal to stop processing args on the command-line, `hest` itself does not traffic in "operands" per se.

## The different `kind`s of options, and how to `hestOptAdd_` them.

Each `hest` option is entirely specified by one `hestOpt` struct. Two essential yet unconventional fields in the `hestOpt` are `min` and `max`: the minimum and maximum number of parms to parse for that option. All the various traditional uses of the command-line can be parameterized in terms of `min` and `max`, even if they aren't normally characterized this way, and even though it covers rarely-used cases.

After recognizing the expressivity of parameterizing options in terms of `min` and `max`, `hest` implementation was organized around that. A numeric _`kind`_ value summarizes `min` and `max`. The following ASCII-art illustrates:

- The `min` number and `max` number of parms along the horizontal and vertical axes, respectively. `max=-1` means "unbounded".
- The associated numeric `kind`, shown as `(`_`k`_`)`; for _`k`_ = 1,2,3,4,5
- Prose name for that kind of option, which appears in the code and a description of its operation.
- Which `hestOptAdd_` function or family of functions is used to parse that kind of option. Here, `hestOptAdd_` is abbreviated to `hOA_` and the final `_T` stands for the type `T`. `T` is one of: `Bool`, `Short`, `UShort`, `Int`, `UInt`, `Long`, `ULong`, `Size_t`, `Float`, `Double`, `Char`, `String`, `Enum` (an `airEnum`), or `Other` (parsed via callback).

```
    :                 .                      /       .        /
    |                 .                    /       .        /
    |                 .                  /       .        /
    |          (5) multiple             |  (3) multiple   |
 2--|             variadic              |     fixed       |
    |              parms                |     parms       |
    |             hOA_Mv_T              | hOA_{2,3,4,N}_T |
    |...................................|.................|
    |   (4) single    |   (2) single    |
 1--|     variadic    |      fixed      |
    |       parm      |      parm       |
    |     hOA_1v_T    |     hOA_1_T     |
    |.................|.................|
    | (1) stand-alone |
 0--|      flag;      |
    |    no parms     |
    |    hOA_Flag     |
 ^  |________ ________|_________________|_________________|__..
max          |                 |                 |
    min >    0                 1                 2
```

Parameterizing options with `min` and `max` allows you to specify an option that needs, for example, 3 or more arguments, or between 1 and 4 arguments. This is useful for various technical settings, but is not as straightforward in something like Python's `argparse`. Relative to `argparse` and its `nargs` argument to `.add_argument()`:

- `nargs=N` with `N`>1 is hest's `kind`=3
- `nargs=1` is `kind`=2
- An on/off flag with `action='store_true'` is `kind`=1
- `nargs=?` is `kind`=4
- `nargs=*` is `kind`=5 with `min`=0 and `max`=-1
- `nargs=+` is `kind`=5 with `min`=1 and `max`=-1

Most of the time the details of `kind` will not matter. The `hest` API for creating a `hest`-based program does not involve `kind`, and end-users don't have to understand `kind` to use a `hest`-based program. Still, knowing the different `kind`s is useful for understanding `hest`'s behavior and sometimes its error messages. Some further notes on `kind`:

- The `kind` of option is independent of being _optional_ (due to the `hestOpt` having a default string) versus _required_ (because no default string is given). `kind`=1 stand-alone flags have no default, but set an `int` variable to 1 or 0 if the user opted to include the flag on the command-line.
- `kind` is mostly independent of whether an option is flagged or unflagged (i.e. positional). Flags are required for `kind`=1 stand-alone flags (or else it would make no sense) and for `kind`=4 options that take zero or one parm. The unusual semantics of `kind`=4 options, which involve logical negation with C's unary `!` of a required default value, deserve explicit indication on the command-line, rather than looking like the "zero or more" collection of values in a `kind`=5 option.
- `kind` is mostly independent of the option type, the exceptions being `kind`=1 stand-alone flags, and `kind`=4 options, which only support boolean and numeric types.
- Except for the current constraint of allowing _at most one **unflagged** multiple variadic option (kind=5)_, there can be zero, one, or many instances of any kind in the option array.

## Over-all `hestParse` process: Phases and Limitations

Given an `argc`,`argv` command-line and a `hestOpt` option array, `hestParse` goes through **multiple phases** to set the per-option variables:

1. Validate the given `hestOpt` array to ensure that, for example, every option has a non-NULL pointer to a C variable to set.
1. Convert given `argc`,`argv` command-line to an internal (`hestArgVec`) representation of the argument vector. This is the phase in which response files are expanded and "`-{`", "`}-`" comments are handled, resulting in a new effective command-line that we'll still refer to here as "the command-line". This is also when "`--`" is detected on the command-line as the "stop processing" flag.
1. Collect from the command-line all the arguments associated with flagged options: the flag args plus any immediately subsequent associated parm args. A flagged option with variadic parms consumes (extracts from the command-line) the flag and following args up to its _maximum_ number of parms, stopping at another option's flag, the end of the command-line, or `-.` as described below. A valid `hestOpt` flag string cannot start with a digit or "`.`", so args like "`-1.5`" or "`-.1`" are unambiguously parms rather than flags, which side-steps [some confusion that other command-line parsers have](https://docs.python.org/3/library/argparse.html#arguments-containing). Arguments are transferred (as strings, without any parsing) to a per-option vector of args inside the `hestOpt` struct, over-riding any default string for that option.
1. What remains in the command-line should be args for unflagged (positional) options. These options consume args in order (first option consuming the first arg(s)) but not in a greedy way: an unflagged variadic option can be followed by unflagged fixed options.
1. An option that isn't assigned args from the command-line will tokenize its own default string to populate its per-option arg vector. Each `hestOpt` remembers the origin of args used to set its variable (command-line vs response-file vs default string).
1. By this point every option should have something to parse: whether by command-line, response file, or default, _every_ option _must_ end up with zero or more arg strings, or else this is a run-time error. `hestParse` finishes by, for each option, parsing the option's arg vector to set the option's C variable.

Within each of these phases, lots of things could go wrong. Internally, `hest` uses Teem's `biff` library for accumulating human-readable error messages, so that they can be printed to inform how to fix the command invocation. `hestParse` takes an `char **errP` argument, in which to pass the address `&err` of a `char *err`; `hest` will dynamically allocate the buffer for holding the accumulated error message (the caller `free`s this later). For example, if trying to get a 1D scanline out of 3D volume `vol.nrrd` with `unu slice -i vol.nrrd -a 0 AX -p 40 50 -o line.nrrd`, where `AX` should have been some numeric `$AX`, the resulting error message is:

```
unu slice: error:
[hest] problem with parsing and setting values
[hest] problem parsing arg[1] (of 2) for (kind=5) "-a,--axis" option[0]
[hest] couldn't parse "AX" (from command-line) as unsigned int
```

Or, if the `-a` option was omitted entirely with `unu slice -i vol.nrrd -p 40 50 -o line.nrrd`, the error message is:

```
unu slice: error:
[hest] problem extracting args for options
[hest] from original command-line: -i vol.nrrd -p 1 2 -o tmp.nrrd
[hest] didn't get required (default-less) "-a,--axis" option[0]
```

Supporting response files, with their need for shell-like tokenization, involves somewhat complicated logic, which is nonetheless fastidious about reporting its various possible errors. Here is an example from a different test program:

```
error: [hest] problem with initial processing of command-line
[hest] from original command-line: -s "hi there" @resp/bad-dqescape.txt
[hest] (iter 5, on response-file) unable to get next arg
[hest] trouble getting next arg
[hest] trouble at character 13 of response-file "resp/bad-dqescape.txt"
[hest] hit input end after \ escape from double-quoted string
```

Some of this error message is more helpful to a developer than to the end-user, but the last two lines give actionable info for the user, and all of it is useful for debugging. Printing the error message to `stderr` is done here by `hestParseOrExit`, but you can write your own wrapper around `hestParse` to handle the error message differently.

If there are no variadic options (i.e. limiting oneself to kinds 1, 2, and 3), then the assignment of a command-line arg to a `hestOpt` is straight-forward, even with unflagged options and even with flagged options being able to appear on the command-line in any order. Variadic parameters do complicate the mapping of args to options. Considering the "`mv *.txt dir`" example: what if instead of just a single destination "`dir`" there were multiple "`dir1 dir2 ...`" directories (all of which were to get copies of the `.txt` files)? There would be no trivial way for `mv` to know when the "`*.txt`" args end and the "`dir`_`I`_" args begin. With this context, some further limitations and details of `hest`'s processing can be outlined, including the role of special flags "`--`" and "`-.`":

- `hest` currently does not support combining single-character stand-alone flags, like how `ls -lt` means `ls -l -t`.
- Flags for `hest` options (of any kind) can have both short (e.g. "`-s`") and long (e.g. "`--size`") forms, by giving a `char *flag` arg to `hestOptAdd_` that contains "`,`" (e.g. "`s,size`"). However, `hest` does not support using "`=`" to set the option value with long-form flags (e.g. "`--prefix=/usr/local`"). The contexts that motivated `hest`'s creation usually involve giving multiple values per option, and there is no convention with `=` for handling multiple options; `hest` uses space-separated arguments instead (e.g. "`--size 3 640 480`").
- Flagged options can appear in any order on the command-line, and the same flagged option can be repeated on the command-line: the last appearance over-rides all earlier appearances, rather than appending to them. `hest` currently cannot accumulate multiple occurances of repeated options (as is created with, say, "`sed -e ... -e ... `"). (Unflagged/positional options, by nature, cannot be repeated.)
- Inspired by [POSIX Utility Guidelines](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html#tag_12_02): a "`--`" flag appearing on the command-line tells (by default) `hestParse` to stop processing arguments (including detecting flags for flagged options), and to leave subsequent arguments for something else outside of `hest` to process. Absent "`--`", however, `hestParse` will attempt to consume and process the **entire** given `argc,argv` command-line as options and their values. It is currently an error to have extra args that cannot be attributed to any option. With its origin as a special command-line flag, the "`--`" is only recognized on the original given command-line, rather than in response files or default strings.
- Parameters for options (flagged or unflagged) can only come from the end-user or from the option's default string, but not from some mix of both: there is no way to supplement arguments from the user with more from the default string or vice versa. Though it may be more confusing than helpful, the user can set options from a mix of the command-line and response files, since response files are expanded (in Phase 2) into a new effective command-line prior to any per-option processing.
- As noted above, there currently can be at most one _unflagged_ multiple variadic option (`kind`=5). Having more than one will likely create user confusion about which args belong to which option. There is no limit on the number of _flagged_ multiple variadic options. It is also okay to have one unflagged multiple variadic option alongside other unflagged options with multiple _fixed_ parms.
- For any multiple variadic option, _flagged or unflagged_, "`-.`" can be used to explicitly mark the end of args for the option. This use of "`-.`" can often be avoided: the end of the command-line also marks the end of any variadic args, and any other option flag marks the end of variadic args for a flagged option. For an unflagged multiple variadic option _with_ a default and with `min`=0 (i.e. the option can take zero or more parms), a lone "`-.`" argument is how the user explicitly says "I am passing zero arguments, and overriding the default". On the other hand, not giving any args for such an option means "I am not setting this option, and want the default". Though unconventional, "`-.`" resolves annoying ambiguities around this corner case in other command-line parsers.
- For an unflagged multiple variadic option with _no_ default and with `min`=0, giving no arguments also works to say "I am passing zero arguments", rather than being an error. For a flagged multiple variadic option with `min`=0, the presence of the flag alone, with no following parms, is what communicates "I am passing zero arguments (overriding the default if there is one)". Thus for multiple variadic option with `min`=0 with flag "`f`", both "`-f -.`" and "`-f`" (with no non-flag arg following) both mean "I am passing zero arguments".

## `hestParse` orderings of arguments and options

Understanding the behavior of `hestParse` requires defining various orderings. First, there is the _appearance order_ of _arguments_ on the command-line (after in-place expansion of any response files in Phase 2). For any option (flagged or unflagged, variadic or fixed) with multiple parms, if the option consumes args from the command-line, the appearance order of the args _is_ the order of the parms within the option. Then, there are **three** kinds of ordering among the set of _options_:

- _Definition order_: The order in which the `hestOpt` option array is built up by calls to `hestOptAdd` functions, which is the same as the order in which the options are stored in the `hestOpt *hopt` array. In any usage information (from `--help` or `--usage`), options are described in definition order.
- _Consumption order_: The order in which options consume arguments from the command-line in Phases 3 and 4.
- _Parsing order_: The order in which each option's assigned (string-valued) arguments are parsed in Phase 6 (with things like `sscanf`) to recover the values to set in the option's C variable.

Between flagged options (whether variadic or fixed), the definition order does not affect the (Phase 3) consumption order: args containing a flag and parms for a flagged option are consumed in command-line appearance order, with later appearances of the same flagged option overriding any previous ones. The appearance of a flagged option on the command-line overrides the option's default; flagged options not appearing rely on their default.

Among unflagged (positional) options, however, Phase 4 consumption order is more subtle:

- If there is no unflagged multiple variadic (`kind`=5) option, then options' consumption order follows definition order: the first unflagged option to consume args is the first unflagged option in the definition order, and subsequent unflagged options in the definition order consume their parameter arguments from the beginning of the remaining command-line. This is straight-forward because all the unflagged options are fixed (the number of expected parms is fixed).
- If there is one unflagged variadic multiple (`kind`=5) option (and there can be at most one), then consumption by any other unflagged (fixed, non-variadic) options happens in two passes: _first going forwards_ from the beginning of definition order up until and stopping before the `kind`=5 option, _and then going backwards_ from the end of definition order. This is how non-greedy consumption of args by the `kind`=5 option is implemented. Within the second backward pass, args are still assigned to options in left-to-right (appearance) order. Finally, any remaining args are consumed by the `kind`=5 option.

Within the consumption order just described, each unflagged option asks (in Phase 4): "are there remaining command-line arguments for me to consume?" If yes, that option consumes them, and thereby overrides its default string. Whether an unflagged option has default string does **not** inform whether the option tries to consume args from the command-line. If it did depend on having a default, the mapping of args to options could become needlessly tricky to reason about, because it would depend on the user knowing not only the order of unflagged options, but also knowing which ones had defaults.

Once on option has consumed all the args it needs as parameters, the option can parse its own parmeter strings to set its C variable. A separate _parsing order_ may therefore seem moot. It makes sense, however, to enable parsing some options last, so that any parsing errors in all the other options can be detected (and complained about to the user) sooner rather than later. For example, we don't want to wait to read in a large `-i` input data file only to _then_ discover that some other `-s` option couldn't parse the three integers it needs. But it also makes sense for usage and glossary documentation to list `-i` before `-s`, in their definition order. Thus, each `hestOpt` has an integer `priority` value (defaulting to 0) that can be used to move it earlier (higher `priority`) or later (lower `priority`) in parsing order. **The parsing order is determined by first traversing all priority levels used (from high to low), and then within one priority level, the definition order determines the parse order.**

Even though it is thus possible to control the relative ordering of option parsing, options are still parsed independently: there is currently no `hest`-enabled way for the parsing of one option to use information generated by the previous parsing of another option.

## Tokenizing args and storing results

All values set in per-`hestOpt` C variables start as `char*` strings, so the process of setting values involves two kinds of string operations: tokenizing into (string-valued) arguments, and parsing arguments into values. Tokenizing is described here, with reference to the Phase numbers in "Over-all `hestParse` process: Phases and Limitations".

Before Phase 2, the shell already did tokenizing to turn the single line of text you type on the command-line into `argc`,`argv`. In the case of a `@resp.txt` response file, however, the file contents are read in and tokenized by `hest` in way that intends to be indentical to the shell, and inserted in that place of the argument sequence. The `hestParm` struct as field `responseFileEnable` for enabling response files, and `hparm->responseFileEnable` is true by default. Response file expansion includes handling [single- vs double-quoted strings](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_02) and [token formation](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_03), although `hest` (since it is not the shell) does not attempt anything with parameter expansion, command history, command substitution, or arithmetic expansion.

Besides expanding response files, `hest` does additional processing in Phase 2 on the argument sequence as part of building up its internal `hestArgVec`. While response files do support "`#`"-to-end-of-line comments, `hest` recognizes that sometimes you want to add comments to (or comment out part of) a command-line, whether in the shell directly or in a response file. As long as `hparm->respectDashBraceComments` is non-zero (as it defaults to), then "`-{`" and "`}-`" arguments will be ignored, as well as any intervening arguments they contain. Also, "`-{`","`}-`" comments can be nested. To be recognized as distinct tokens by the shell and to prevent accidental brace expansion, the "`-{`" and "`}-`" sequences have to be space-separated from any other tokens. As noted before, the special non-flag "`--`" tells hest to stop processing (as long as `hparm->respectDashDashMeansStop` is non-zero, which it is by default): args in the command-line (after response file expansion) that follow "`--`" are collected in the NULL-terminated `char **remainArgv` in the first `hestOpt`, containing `remainArgc` strings.

Phases 3 and 4 extract (or consume) user-supplied arguments from the command-line `hestArgVec` and assign them to individual options by moving each arg into one of the per-option arg vectors of strings (`hestOpt->havec`), according to the description of consumption order above, including the role of `hest`-specific flag "`-.`" (which works as long as `hparm->respectDashPeriodMeansEnd` is non-zero, which it is by default).

In Phase 5, non-NULL per-option default strings (`hestOpt->dflt`) are used to populate the per-option arg vectors that weren't already set from the user. `hestOpt->dflt` being NULL is how the `hest`-using programmer signifies that this option (except for `kind`=1 stand-alone flags) must be set by the command-line user. It might seem reasonable for a programmer-supplied default value for the per-option C variable to actually be of that same variable type, instead of only being a string. `hest`'s design, however, embraces the universal expressivity of strings: as long as `hest` is in the business of parsing values from command-line-supplied strings, in the interests of consistency it may as well use the same machinery to parse values from the default strings. This also helpfully unifies and simplifies the storage of default values within the `hestOpt`, for any type and kind.

In general, the per-option default string is tokenized to extract multiple args (for that option) just like the shell does and like `hest` handles response file contents, to build up a per-option arg vector (of strings). There are some exceptions to this tokenizing:

- For `kind`=5 (multiple variadic) options with `min`=0, a non-NULL empty default string `""` means: 0 arguments, _even for string-type options_. The non-NULL string means that there is a programmer-supplied default (so the user doesn't need to specify the option), but the emptiness means that 0 args are supplied. If the type is string, use `"''"` to indicate a default value of a single empty string.
- For `kind`=2 (single fixed) options of type string: The default string value _is_ the default string, without any tokenization or quoting. The programmer is already quoting the string literal to make it a string; they shouldn't have to do additional quoting just because that string may contain a space.

The only container for a set of options, both their (input) specification and the (output) results of their parsing, is the `hestOpt *hopt` array passed to `hestParse`. The implementation decision to make `hestOpt *hopt` the sole state container long predates support for things like `--help`, which created a problem: where should `hestParse` record the detection of `--help`, since it is not specific to any option, but also exists outside the simple binary of zero (success) or non-zero (error) C function return? The inelegant strategy has been to add fields to the `hestOpt` struct that will exist for all options, but will only be set by `hestParse` in the _first_ option, e.g. `hopt[0].helpWanted`. Other fields in the first option (`hopt[0].arrAlloc`, `hopt[0].arrLen`) record the allocated and effective length of the option array, so that `hestOptAdd_` calls can extend the array with a granularity coarser than a single option.

## Value Parsing Details

The final phase (Phase 6) of `hestParse` involves parsing, for each option, a string-valued parm vectors into values that can be set in a C variable. The following account of the different `kind`s refers to the `void *valueP` field of the `hestOpt`, which is how the user identifies (the address of) the C variable to set for that option. The C variable will have a specific type, which is communicated to `hest` via the `int type` field of the `hestOpt`, which takes on values from the `airType`_`X`_ enum values defined in `hest.h`. The type is also part of the name of the various `hestOptAdd_` functions in the API outlined above (e.g. calling `hestOptAdd_1_Bool` will create option for which `type == airTypeBool`). Describing how a single value of some type is set is easier with expressions like _`type`_ `*vv` to identify a new variable `vv` that points to some C type _`type`_:

- Both the `Bool` and the `Enum` types in `hest` assume `int *vv`.
- Other scalar types (`Short`, `UShort`, `Int`, `UInt`, `Long`, `ULong`, `Size_t`, `Float`, `Double`, `Char`) have the expected C types for `*vv`.
- Type `String` assumes `char **vv`, and `*vv` is set to a `strdup()` of the given string.
- For type `Other` hest uses `void *vv` and the user-defined handling in the given `hestCB *CB` struct (see `hest.h`). `CB->parse()` can either set values within memory that `vv` points to, or set `vv` to point to a newly heap-allocated and filled-in space (to be freed later by `CB->destroy`).

Now we can describe how values are set according to option kind:

- `kind`=1 stand-alone flag: No string value is actually parsed. `valueP` is cast to `int *vv`, and `*vv` is set to `0` or `1`, according to whether the flag never appeared (`0`), or if it appeared on the commandline or a response file it referenced (`1`).
- `kind`=2 single fixed parm: `valueP` is cast to a _`type`_ `*vv`, and `*vv` is set according to how the string was parsed.
- `kind`=3 multiple fixed option with `N` parms: cast `valueP` to _`type`_ `vv[`_`N`_`]` and set each `vv[i]` as above from the `N` args consumed by this option
- The `kind`=4 single variadic option has unusual semantics: if the flag does not appear, `*vv` is set to the single value parsed from the required default string; let's call this `D`. If the flag does appear but without parms (e.g. `-u` alone), then `*vv = !D`: this is why this kind can only be of a type for which `!` makes sense. If the flag appears with a parm (e.g. `-u 4`), then `*vv` is set to the value parsed from the given parm.
- `kind`=5 multiple variadic option passed `M` parms (where `M` is only learned at run-time): `valueP` is cast to _`type`_ `**vvP`, the address of an array of _`type`_ (e.g. `char ***vvP` for multiple variadic option of strings). `*vvP` is set to the newly allocated array of `M` _`type`_, and then all the `(*vvP)[i]` are set individually as above. The corresponding `hestOpt` has a non-NULL `unsigned int *sawP` field, and the number of values seen is recorded as `*sawP = M`. In the case of multiple variadic strings, a final NULL entry is added to the `(*vvP)[]` array to mimic the NULL-terminated structure of `argv` itself.

## Usage information and `hestParseOrExit`

`hestUsage` prints (to a given `FILE *file`) a concise invocation line that indicates al the options. ...MORE INFO...

`hestGlossary` prints detailed information about every option, its default if it has one. ...MORE INFO...

`hestInfo` prints a top-level description of what a program is good for, and take additional arguments to tune how the program is identified and how the description is printed.

`hestInfo`, `hestUsage`, and `hestGlossary` all respect the number of columns in the display as recorded in `hparm->columns`. The helper function `hestParmColumnsIoctl` tries to set `hparm->columns` based on the current environment (with calls to `ioctl` on unix, or something analogous on Windows).

Many Teem programs that use `hest` evolved certain patterns around `hestParse`, and these have been captured in the utility function `hestParseOrExit`. If `hestParseOrExit` returns, all the C variables have successfully been assigned values and the computation can proceed. Otherwise, `hestParseOrExit` prints descriptive error messages to describe what didn't work (before `exit(1)`), or it handles things like `--help`, `--usage` and `--version` by printing requested information before calling `exit(0)`. Care is taken to ensure that upon exiting, all allocated memory has been freed.

## Testing

The subtleties of `hest`'s behavior (with the various interactions of options being flagged/unflagged, variadic/fixed, with/without default, in what appearance order) have never been thoroughly tested until 2026, but now in `hest/test/rigor` has a (hacky) `0-go.sh` script that drives `run_hest_tests.py`. That testing script started as LLM (Claude) output, but has since been cleaned up and fixed to become a useful way of testing the assertions made in this documentation, and to exercise the complete `hest` API and code for coverage testing.

The testing currently has 100% function coverage, 100% line coverage, and (in GLK's view) full branch coverage of all the interesting branches. Chasing 100% branch coverage was recognized as a poor use of time once experience showed that a claim made in [The llvm source-based coverage docs](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) is false: "100% branch coverage for a function implies 100% region coverage for a function". With `clang --version` "Apple clang version 17.0.0 (clang-1700.6.3.2)" and `llvm-cov --version` "Homebrew LLVM version 22.1.0", `clang -DNDEBUG -O0 -fprofile-instr-generate -fcoverage-mapping` and `llvm-cov show` claims that there's an uncovered region in `argvHest.c`, even while saying that that file has 100% branch coverage. This is likely an instrumentation artifact: a zero-length region (with count zero) created by LLVM's counter expression arithmetic, comparable to the gap regions from switch statements, as acknowledged at the [end of the docs](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html#switch-statements).

## AI Acknowledgement

I (GLK) wrote all the C code within the `hest` _library_, in the sense that I created and understand the design, I understand the code, and every line of code underwent typing and editing in my editor. No section of `hest` C code was copied from an LLM, but in these specific cases, an LLM helped in prototyping or debugging:

- 2025 ChatGPT (free): (`parsest.c`) prototyping the tokenization DFA of `argstGo()`
- 2025 ChatGPT (free): (`argvHest.c`) prototyping the "does this arg need quoting" testing in `hest__PlainWord()` and the quoting itself in `argAddQuotedString()`
- 2026 Claude (Sonnet 4.6): (`parsest.c`) helped with debugging interactions of token production and input stack operations, but only within 2025 GLK's framework of a second implicit DFA for handling that. Then 2026 GLK realized that design was misbegotten, ripped it out, and handled it all with `popit` and `tokenDone` flags passed between `argsGo()`, `histNextArg()`, and `histProcess()`.
- 2026 Claude (Sonnet 4.6): (`parsest.c`, `parseHest.c`) helped with diagnosing memory leaks as revealed by valgrind.

The following _testing_ code was initially written by LLM. The Teem copyright notice is not applied to it, since subsequent GLK edits do not constitute anything that could stand on its own as copyrightable work.

- 2026 Claude (Sonnet 4.6): created `hest/test/tht1.c`
- 2026 Claude (Sonnet 4.6): within `hest/test/rigor`, created the `run_hest_tests.py` testing script and the other files it uses in that directory (except for GLK-authored `th_kindK.c` and `gen_kind.py` which create kind-specific test programs). GLK's coverage testing work involved extensively adding to the test list in `run_hest_tests.py`, and modifying the test C programs (e.g. with environment variable switches) to exercise more of the `hest` code.
