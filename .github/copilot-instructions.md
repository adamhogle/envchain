# Copilot instructions for envchain

## Build, test, and lint commands

This repository is a small C CLI with platform-specific backends and no dedicated test or lint targets.

### Build

**macOS / Linux**

```sh
make
```

**Windows (MinGW/MSYS2)**

```sh
make
```

**Windows (MSVC)**

```bat
nmake /f Makefile.win
```

### Install

**POSIX makefile**

```sh
make install DESTDIR=/custom/prefix
```

**MSVC makefile**

```bat
nmake /f Makefile.win install INSTALLDIR=C:\Tools\bin
```

### Single-scenario validation (manual)

There is no automated single-test command. Use this one-namespace smoke flow after building:

```sh
envchain --set testns TEST_VAR
envchain --list
envchain --list testns
envchain testns env | grep TEST_VAR
envchain --unset testns TEST_VAR
```

On Windows CMD, the execution check is:

```bat
envchain testns cmd /c echo %TEST_VAR%
```

## High-level architecture

- `envchain.c` is the only CLI entry point. It parses modes (`--set`, `--list`, `--unset`, or exec mode), prompts for values, and injects loaded secrets into the child process environment.
- `envchain.h` defines a tiny storage backend interface: search namespaces, search key/value pairs, save, and delete.
- Exactly one backend file is linked per platform:
  - `envchain_osx.c`: macOS Keychain (Security/CoreFoundation)
  - `envchain_linux.c`: D-Bus Secret Service via libsecret
  - `envchain_windows.c`: Windows Credential Manager (`CredWrite/CredEnumerate/CredDelete`)
- Runtime flow in exec mode is:
  1. Parse comma-separated namespace list.
  2. For each namespace, call `envchain_search_values(...)`.
  3. Callback sets process environment variables.
  4. Launch target command (`execvp` on POSIX, `_spawnvp(_P_WAIT)` on Windows).

## Key repository conventions

- **Shared behavior lives in `envchain.c`; storage behavior stays backend-specific.** Keep CLI/argument semantics centralized and add platform differences behind the `envchain.h` functions.
- **Namespace-not-found warning format is user-facing contract.** Backends print the same warning style when a namespace does not exist and return non-zero for that lookup.
- **`--require-passphrase` is macOS-specific behavior.** Linux and Windows backends accept the flag but treat enforcement as unsupported.
- **Credential naming convention is platform-consistent.** Backends map secrets by namespace + key; Windows uses `envchain-<namespace>/<key>`, and macOS service names are prefixed with `envchain-`.
- **Build selection is makefile-driven by platform.** Prefer updating `Makefile`/`Makefile.win` object lists and link flags rather than adding platform checks in many places.

## Writing style guardrails (unslop)

Apply these rules to README/docs, issue comments, and generated commit messages:

- Use plain words. Prefer "use/help/if/many" over "utilize/facilitate/in the event that/numerous".
- Avoid stock AI phrasing and vague attribution. Name concrete sources or mechanisms.
- Remove decorative tone and filler. Skip "Great question", "I hope this helps", and similar lead-ins/outros.
- Avoid em dashes. Use periods or commas.
- Keep headings in sentence case.
- Avoid bold-label list items that repeat themselves (`**Performance:** ...`). Write direct prose instead.
- Prefer active voice and concrete facts ("the loader parses the file", not "the file is parsed").
- Split dense sentences. One idea per sentence when possible.
- Say what the code does with mechanism-level detail, not metaphor ("returns SQL string", "exits 2 on parse error").
