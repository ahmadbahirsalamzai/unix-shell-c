# wish — a small Unix shell in C

`wish` is a minimal command-line shell written in C on top of the POSIX process
API (`fork`, `execv`, `waitpid`, `dup2`). It runs interactively or from a batch
file, resolves programs through a configurable search path, and supports
output redirection and parallel commands.

```
$ ./wish
wish> ls -la /tmp > listing.txt
wish> path /bin /usr/bin
wish> sleep 1 & echo hello & echo world
hello
world
wish> exit
```

## Features

- **Interactive and batch modes**: run `./wish` for a prompt, or `./wish script.txt` to execute commands from a file with no prompt.
- **Search path**: commands are looked up in each directory of the shell path using `access(..., X_OK)`. The default path is `/bin`.
- **Built-in commands**:
  - `exit`: quit the shell (takes no arguments).
  - `cd <dir>`: change directory (takes exactly one argument).
  - `path [dir ...]`: replace the search path. An empty `path` disables running external programs.
- **Redirection**: `cmd args > file` sends both stdout and stderr to `file`, truncating it first.
- **Parallel commands**: `cmd1 & cmd2 args & cmd3` starts every command before waiting for all of them to finish.
- **Whitespace tolerant**: any mix of spaces and tabs is accepted, and operators (`>` and `&`) don't need surrounding whitespace.
- **Single error message**: every error prints `An error has occurred` to stderr. The shell keeps running, except for an invalid invocation or an unreadable batch file, which exit with status 1.

## How it works

The whole shell is in [`src/wish.c`](src/wish.c):

1. **`main`** reads lines with `getline()` from stdin or the batch file and prints the `wish> ` prompt in interactive mode.
2. **`process_line`** splits the line on `&` with `strsep()`. For each command it extracts and validates any `>` target, then tokenizes the arguments on spaces and tabs.
3. **`run_command`** handles the built-ins in-process. For anything else it searches the path, `fork()`s, applies redirection in the child with `dup2()`, and calls `execv()`.
4. Once every command on the line has been launched, the parent `waitpid()`s on all child PIDs, which is how the parallel commands run concurrently.

## Build and run

Requires a C compiler and a POSIX system (Linux or macOS).

```sh
make              # builds ./wish
./wish            # interactive mode
./wish batch.txt  # batch mode
make clean
```

## Tests

```sh
make test-batch                 # build and run the test suite
./tests/run-tests.sh -v -t 7    # run a single test verbosely
./tests/run-tests.sh -c         # keep going after a failure
```

Each test in [`tests/`](tests/) has a `.desc` (description), `.run` (command), and the expected `.out`, `.err`, and `.rc` (exit code). The tests cover the built-ins, bad arguments, path handling, redirection edge cases, batch-mode errors and whitespace handling.

> Test 3 compares against your system's `ls` error text, so its setup step regenerates `tests/3.err` locally. On macOS this shows up as a modified file after a test run, which you can discard with `git checkout tests/3.err`.

## Limitations

- No pipes (`|`), input redirection (`<`), quoting or escaping, environment variables, or job control.
- Commands are resolved only through the shell path, not by absolute or relative paths such as `./prog`.
- Redirection isn't supported for built-ins.

## Acknowledgments

The project specification and test harness come from the *processes-shell*
project in [OSTEP Projects](https://github.com/remzi-arpacidusseau/ostep-projects),
which accompanies *Operating Systems: Three Easy Pieces* by Remzi and Andrea
Arpaci-Dusseau.

<details>
<summary>Background</summary>

<sub>Originally written in Fall 2026 as coursework for CSC 139 (Operating System Principles). Please don't submit it as your own work.</sub>

</details>
