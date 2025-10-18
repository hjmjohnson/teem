# `hest`: command-line parsing

## Introduction

The purpose of `hest` is to bridge the `int argc`, `char *argv[]` command-line arguments and a collection of C variables that need to be set for a C program to run. The variables can be of most any type (boolean, `int`, `float`, `char *` strings, or user-defined types), and the variables can hold single values (such as `float thresh`) or multiple values (such as `float RGBA[4]`). `hest` has become essential for the utility of command-line tools like `unu`. To the extent that `hest` bridges the interactive shell's command-line with compiled C code, it approaches some of the roles that in other contexts are served by scripting languages with C extensions (no matter how ill-advised that sounds).

`hest` was created in 2002 out of frustration with limitations of other C command-line parsing libraries. The [`argtable` (version 1)](https://argtable.sourceforge.io/) library came closest to the needed feature set. It directly inspired `hest`'s array-of-per-option-structs approach, and early versions of `hest` supported `argtable`-style static definition of the `hestOpt` array (current `hest` usage involves dynamic creation of the option list). The newer [Argtable3](https://www.argtable.org/) looks great, but still does not directly support response files or parsing arbitrary objects via callbacks. The `hest` code was revisited in 2023 to add long-overdue support for `--help`, and to add typed functions for specifying options like `hestOptAdd_4_Float`. Re-writing the code in 2025 fixed long-standing bugs with how quoted strings were handled and how response files were parsed, and to add `-{`, `}-` comments and other features.

`hest` is powerful and not simple. This note attempts to give a technical description useful for anyone thinking about using `hest`, as well as anyone trying wrap their head around the `hest` source code, including its author.

First, some examples ...

More examples may help show `hest`'s utility ... (IN PROGRESS) ...
... Give some specific examples, flagged and unflagged ...
.. show a response file being used, show -{ }- commenting

## Terminology and concepts

`hest` has possibly non-standard terminology for the elements of command-line parsing. Here is a bottom-up description of the command-line and how `hest` interprets it:

- What `main()` gets as `char *argv[]` is the vector of _arguments_ or _args_; each one is a `char*` string. An arg can contain spaces and other arbitrary characters if the user quoted strings or escaped characters; that is between the user and shell (the shell is responsible for taking the command-line and tokenizing it into `char *argv[]`).
- Arguments like `-v` and `-size`, which identify the variable to be set, are called _flags_. Some flags are really just flags; no further information is given beyond their presence or absence. Other flags introduce subsequent arguments that together supply information for setting one variable.
- The sub-sequence of arguments that logically belong together (perhaps following a flag) in the service of setting a variable are called _parameters_ (or _parms_). There is some slippage of terminology between the `char *` string argument that specifies the parameter, and the value (such an `int`) parsed from the parameter string.
- Separately, and possibly confusingly, `hest`'s behavior has many knobs and controls, stored in the `hestParm` struct. The pointer-to-struct is always named `hparm` in the code, to try to distinguish it from the parameters appearing on the command-line.
- One _option_ determines how to set one C variable. In the C code, one `hestOpt` struct stores everything about how to parse one option _plus_ intermediate state during the parsing process _plus_ the final results of that parsing. An array of `hestOpt` structs (which we can call an _option list_) is how a `hest`-using program communicates what to learn from the command-line. The `hestOpt` array is built up by repeated calls to the `hestOptAdd_` family of functions.
- On the command-line, the option may be communicated by a flag (e.g. `-sz`) and its associated parms (e.g. `3 640 480`); this is a _flagged_ option. Options may also be _unflagged_, or what others call "positional" arguments, because determining which option a given argument belongs to is disambiguated by where that option's `hestOpt` struct occurs with the option list.
- An option may have no parms, one _single_ parm, a _fixed_ number of _multiple_ parms, or a variable number of parms. Options with a variable number of parms are called _variadic_ options (to distinguish from the C _variable_ that the option sets). With `mv *.txt dir`, the `*.txt` filenames could be parsed as the parms for one unflagged variadic option, and `dir` would be a fixed single parm for a second unflagged option.
- Sometimes multiple command-line options need to be saved and re-used together, over a time span longer than any one shell. Command-line options can thus be stored in _response files_, and the contents of response files effectively expanded into the command-line. Response files can have comments (from `#` to end of line, just like shell scripts), and response files can in turn name other response files.
- The main `hest` function that does the parsing is `hestParse`. Its job is to set one variable (which may have multiple components) for every `hestOpt`. Information for setting each variable can come from the command-line, or from the default string stored in the `hestOpt`, but it has to come from somewhere. In this sense, `hest`'s "options" are badly named, because they are not really optional.

Pre-dating `hest` are the [POSIX conventions for command-line arguments](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html#tag_12_01), wherein the elements of `argv` can be first "options" and then "operands", where "options" are indicated by an initial `-` character, and may have zero or more "option-arguments" (what `hest` calls "parameters"). However, `hest` does not attempt to follow POSIX conventions (or terminology) for command-line usage. In particular, `hest` has no idea of an "operand" as it tries to interpret _every_ element of `argv` arg as some argument of some (possibly unflagged) option. Disregard for POSIX has not limited `hest`'s expressivity and flexibility, or its utility for the scientific computing and visualization contexts that it was built for.

## The different `kind`s of options, and how to `hestOptAdd_` them.

The complexity of `hest` arises from the how much information is in one `hestOpt`. Two of the most important yet unconventional fields in the `hestOpt` are `min` and `max`: the min and max number of parameters that may be parsed for that option. All the various traditional uses of the command-line can be parameterized in terms of `min` and `max`, even though they aren't normally characterized this way. Also, the full range of possibilities of `min`,`max` (which `hest` supports) include rarely-used cases. Once the possibility of mapping out all possibilities for command-line options in terms of `min` and `max` was recognized, `hest` implementation was organized around that, even if typical applications of `hest` to command-line parsing are not conceived that way. Refer to the **concrete examples** above.

The _`kind`_ is `hest`'s term for a numeric identifier for the kind of option that a `hestOpt` describes, as determined by `min` and `max`. The following ASCII-art illustrates:

- The `min` number and `max` number of parms along the horizontal and vertical axes
- numeric `kind`, shown as `(`_n_`)`; for _n_ = 1,2,3,4,5
- the prose name for that kind of option, which appears in the code and a description of its operation.
- Which `hestOptAdd_` function or family of functions is used to parse that kind of option. Here, `hestOptAdd_` is abbreviated to `hOA_` and the final `_T` stands for the type: `T` is one of: `Bool`, `Short`, `UShort`, `Int`, `UInt`, `Long`, `ULong,`, `Size_t`, `Float`, `Double`, `Char`, `String`, `Enum` (an `airEnum`), or `Other` (parsed via callback).

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

The `kind` of option is mostly independent of whether it is flagged or unflagged, and independent of being optional (due to the `hestOpt` having a default string) versus required (when no default is given). For unflagged options, `kind`s 1 and 4 are disallowed. At most one unflagged variadic option (kind 5) is permitted, and it is allowed to have `min == 0` only if there are no other unflagged options (of any `kind`). Knowing the different `kind`s is required to understand the `hest` implementation (and helps to understand some error messages). For the end-user, however, this is too low-level: they will probably never need to worry about `kind`, and `kind` does not appear in the API calls to create options and parse command-lines.

## Over-all `hestParse` process: phases and limitations

Given an `argc`,`argv` command-line and a `hestOpt` option array, `hestParse` goes through **multiple phases** to set the per-option variables from the command-line:

1. Validate the given `hestOpt` array
1. Convert given `argc`,`argv` command-line to an internal (`hestArgVec`) representation of the argument vector (called `havec` in the code). This is the phase in which response files are expanded and `-{`,`}-` comments are handled.
1. Extract from `havec` all the arguments associated with flagged options: the flag args, plus any immediately subsequent associated parm args. Arguments are transferred to the per-option arg vector within the `hestOpt` struct.
1. What remains in the command-line `havec` should be the concatenation of args associated with the unflagged (positional) options. As long as there is at most one unflagged variadic option, there is an unambigious assignment of arguments to options, so transfer these args to their corresponding option.
1. An option that isn't assigned args from the command-line will use its own default string to save into its per-option arg vector. Whether by command-line, response file, or default, every option must have the information it needs.
1. Parse the per-option arg vectors (of strings) to set the value(s) of C variable pointed to by each `hestOpt`. Each `hestOpt` also remembers the source of information for setting the variable (command-line vs response-file vs default string).

One of the things that `hestParse` must resolve is how to assign each arg in of `argv` to one of the options (either as its flag or one of its parms). If there are no variadic options (i.e. limiting oneself to kinds 1, 2, and 3), then this is straight-forward, even with flagged options being able to appear on the command-line in any order. The option list has some fixed number of parameter slots. The flag arguments for the flagged options, and the position of arguments of unflagged options, implies how to assign each `argv` element to one parameter slot.

Things became more complicated with variadic options. Considering the `mv *.txt dir` example: what if instead just a single destination `dir` there were multiple `dir1 dir2 ...` directories (all of which are to get copies of the `txt` files)? There would be no automatic way for `mv` to know when the `*.txt` args end and the `dirI` args begin. Fans of [POSIX Utility Guidelines](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html#tag_12_02) may be now thinking about how Guideline 10 describes the role of `--`:

> The first -- argument that is not an option-argument should be accepted as a delimiter indicating the end of options. Any following arguments should be treated as operands, even if they begin with the `-` character. So `--` marks the end of some "option-arguments".

With this context, some further details and limitations of `hest`'s processing can be outlined, including the limited role of `--`:

- To avoid further complexity: there can be at most one _unflagged_ multiple variadic option (kind 5). Having more than creates ambiguity about which option gets which args, and `hest` currently has nothing (not even `--`) to resolve this. There can be, however, more than one _flagged_ multiple variadic option.
- `hest` does use `--` to indicate the explicit end of arguments for a _flagged_ variadic option. But the `--` is not necessary: any other option flag also marks the end of variadic args, as does the end of the command-line. Thus for `hest` the `--` mark is for separating phases 3 and 4 above, rather than separating options from operands.
- `hestProc` strives to interpret the **entire** `argc`,`argv` argument vector you give it; there is currently no way to tell `hest` (via `--` or otherwise): "stop processing `argv` here, and leave the rest as operands for something else to interpret".
- Arguments for options (flagged or unflagged) can only come from the user (the `argc`,`argv` command-line or a response file it invokes) or from the option's default string, but not from any mix of both: there is no way to supplement arguments from the user with those from the default string. Though it will probably be more confusing than helpful, options can get arguments from a mix of the command-line and response files, since response files are effectively expanded into the command-line prior to any per-option processing.
- Flagged options can appear in any order on the command-line, and the same option can be repeated: the last appearance over-rides all earlier appearances. `hest` currently cannot remember a multiple occurances of repeated options (is is used in, say, `sed -e ... -e ... `. )

## `hestParse` orderings of arguments and options

Fully specifying the behavior of `hestParse` requires defining various orderings. First, there is the _appearance order_ of arguments on the command-line (after in-place expansion of any response files). Then, there are **3** kinds of ordering among the options:

- _Definition order_: The order in which the `hestOpt` option list is built up by calls to `hestOptAdd` functions, which is the same as the order in which the options are stored in the `hestOpt *hopt` array.
- _Assignment order_: The order in which options are assigned (string-valued) arguments for each of their parameters, from either the command-line or their own default strings. The assignment process consumes args from the command-line (or rather the internal `hestArgVec` representation of it): an arg can be assigned to only one option.
- _Parsing order_: The order in each option's assigned (string-valued) arguments are parsed (with things like `sscanf`) to recover the values to set in the associated C variable.

How these orderings interact is different for flagged and unflagged options. For flagged options, the definition order is moot: the assignment order is the appearance order, with later appearances of the option overwriting earlier ones. The positional nature of unflagged options, however, means that the definition order interacts with the appearance order of the args remaining after phase 3 (assignment and extraction of args for flagged options), as follows:

- If there is no unflagged (kind 5) variadic multiple option, then the assignment option order _is_ the definition order, which follows the arg appearance order: the first unflagged option to be assigned args is the first unflagged option in the definition order, and subsequent options in the definition order are sequentially assinged args that are extracted from the beginning of the remaining command-line.
- If there is an unflagged variadic multiple option, then the assignment order goes in two passes: _first going forwards_ from the beginning of definition order (consuming args from the beginning of their appearance order) up until and stopping before the unflagged variadic multiple option, _and then going backwards_ from the end of definition order (consumings args from the end of their appearance order). Any remaining args are finally assigned to the single unflagged variadic multiple option.
- As a further wrinkle: whether an unflagged option consumes command-line args depends on whether the option has a default. That is, for each option within the assignment order just described, `hestParse` asks: are there enough remaining args (from the command-line) to satisfy the `min` number of args this option wants? If yes, those args are extracted from the command-line and assigned to the option. If no, the command-line args are left as is and the option instead consumes its own default string (and it is an error if in this case the option has no default string).

One take-away is: **it is simpler for unflagged options to not have defaults**. Defaults in unflagged options can break what would otherwise be a contiguous mapping between arg appearance order and option definition order.

Once every option has been assigned args, every option can independently parse its own args to set its C variable, so a separate _parsing order_ seems moot. It makes sense, however, to enable parsing some options last, so that any parsing errors in all the other options can be detected (and complained about to the user) sooner rather than later. For example, we don't want to wait to read in a large `-i` input data file only to _then_ discover that some other `-sz` option couldn't parse the three integers it needs. But it also makes sense for usage and glossary documentation to have `-i` come before `-sz` in definition order. Thus, each `hestOpt` has an integer `priority` value (defaulting to 0) that can be used to move it earlier (higher `priority`) or later (lower `priority`) in parsing order. **The parsing order is determined by first traversing all priority levels used (from high to low), and then within one priority level, the definition order determines the parse order.**

Even though it thus possible to control the relative ordering of option parsing, options are still parsed independently: there is as yet no simple way for the parsing of one option to use information generated by the previous parsing of another option. This kind of inter-dependence could be implemented for `Other`-typed values parsed via callbacks (the callback functions could set and read a global variable), but future `hest` development may create a cleaner approach.

## Tokenizing values

All values set in per-`hestOpt` C variables start as `char*` strings, so the process of setting values involves two kinds of string operations: tokenizing into (string-valued) arguments, and parsing arguments into values. Tokenizing is described here, with reference to the phase numbers in "Over-all `hestParse` process: phases and limitations".

Before Phase 2, the shell already did tokenizing to turn the single line of text you type on the command-line into `argc`,`argv`. In the case of a `@resp.txt` response file, however, the file contents are read in and tokenized by `hest` in way that intends to be indentical to the shell, and inserted in that place of the argument sequence. Response file expansion includes handling [single- vs double-quoted strings](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_02) and [token formation](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_03), although `hest` (since it is not the shell) does not attempt anything with parameter expansion, command substitution, or arithmetic expansion.

Besides expanding response files, `hest` does additional processing in Phase 2 on the argument sequence as part of building up its internal `hestArgVec` representation to handle `-{`,`}-` comments. While response files do support `#`-to-end-of-line comments, `hest` recognizes that sometimes you want to add comments to (or comment out part of) a command-line, whether in the shell directly or in a response file. As long as `hestParm->respectDashBraceComments` is non-zero (as it defaults to), then `-{` and `}-` arguments will be ignored, as well as any intervening arguments. Also, `-{`,`}-` comments can be nested. To be recognized as distinct tokens by the shell, the `-{` and `}-` sequences have to be space-separated from any other tokens; this will prevent the shell from attempting any brace expansion.

Phases 3 and 4 extract user-supplied arguments from the `hestArgVec` and assign them to individual options by moving each arg into one of the per-option arg vectors (`hestOpt->havec`) ...

In Phase 5, non-NULL per-option default strings (`hestOpt->dflt`) are used to populate the per-option arg vectors that weren't set from the user. `hestOpt->dflt` being NULL is how the `hest` user signifies that this option (except for `kind`=1 stand-along flags) requires user specification. It might be reasonable for a programmer-supplied default value for the per-option C variable to actually be of that same variable type, instead of only being a string. `hest`'s design, however, embraces the universal expressivity of strings: as long as `hest` is in the business of parsing values from command-line-supplied strings, in the interests of consistency it may as well use the same machinery to parse values from the default strings. This also helpfully simplifies the storage of default values within the `hestOpt`.

In general, the per-option default string is tokenized to extract multiple args (for that option) just like the shell does and like `hest` handles response file contents, to build up a per-option arg vector. There are some exceptions to this tokenizing:

- For `kind`=5 (multiple variadic) options with `min`=0, a non-NULL empty default string `""` means: 0 arguments, _even for string-type options_. The non-NULL string means that there is a programmer-supplied default (so the user doesn't need to specify the option), but the emptiness means that 0 args are supplied. If the type is string, use `"''"` to indicate a default value of a single empty string.
- For `kind`=2 (single fixed) options of type string: The default string value _is_ the default string, without any tokenization. The programmer is already quoting the string literal to make it a string; they shouldn't have to do additional quoting just because that string may contain a space.

## Parsing values

The final phase (Phase 6) of `hestParse` is to convert string-valued per-option arg vectors into values that can be set in the per-option C variable.

... `hestOpt->CB` for parsing via callback.
