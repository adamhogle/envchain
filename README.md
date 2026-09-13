# envchain - set environment variables with macOS keychain, D-Bus secret service, or Windows Credential Manager

## What?

Secrets for common computing environments, such as `AWS_SECRET_ACCESS_KEY`, are
set with environment variables.

A common practice is to set them in shell's intialization files such as `.bashrc` and `.zshrc`.

Putting these secrets on disk in this way is a grave risk.

`envchain` allows you to secure credential environment variables to your secure vault, and set to environment variables only when you called explicitly.

`envchain` supports macOS Keychain, D-Bus Secret Service (gnome-keyring), and **Windows Credential Manager** as a vault.

Don't give any credentials implicitly!

## Requirements

### macOS

- macOS 10.7 (Lion) or later
  - Confirmed to work on OS X 10.11 (El Capitan), macOS 10.12 (Sierra)

### Linux

- readline
- libsecret
- D-Bus Secret Service
    - GNOME keyring
    - KeePassXC

### WSL (for Windows Credential Manager passthrough)

- WSL with Windows interop enabled (`*.exe` invocations from Linux)
- A Windows `envchain.exe` available on PATH in WSL (or configure path with `ENVCHAIN_WINDOWS_BIN`)
- Build in WSL with `make` (auto-selects WSL backend in `Makefile`)

### Windows

- Windows Vista or later (Windows Credential Manager is built-in)
- **MinGW/MSYS2** build: GCC toolchain (e.g. `pacman -S mingw-w64-x86_64-gcc`)
- **MSVC** build: Visual Studio 2015 or later

## Installation

### From Source (macOS / Linux)

```
$ make

$ sudo make install
(or)
$ cp ./envchain ~/bin/
```

### From Source (Windows — MinGW/MSYS2)

Open an MSYS2 MinGW 64-bit shell:

```
$ make
$ cp envchain.exe /usr/local/bin/    # or any directory on PATH
```

### From Source (Windows — MSVC)

Open a Visual Studio Developer Command Prompt:

```
nmake /f Makefile.win
nmake /f Makefile.win install INSTALLDIR=C:\Tools\bin
```

### Homebrew (macOS)

```
brew install envchain
```

### Prebuilt releases (this fork)

Tag releases with SemVer plus build metadata, for example:

```
v1.1.0+adamhogle.1
```

The release workflow publishes:

- `envchain-windows-x86_64-<version>.exe`
- `envchain_<version>_amd64.deb` (Debian 13 / trixie target)

## Usage

### Saving variables

Environment variables are set within a specified _namespace._ You can set variables in a single command:

```
envchain --set NAMESPACE ENV [ENV ..]
```

You will be prompted to enter the values for each variable.
For example, we can set two variables... `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY` here, within a namespace called `aws`:

```
$ envchain --set aws AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY
aws.AWS_ACCESS_KEY_ID: my-access-key
aws.AWS_SECRET_ACCESS_KEY: secret
```

Here we define a single new variable within a different namespace:

```
$ envchain --set hubot HUBOT_HIPCHAT_PASSWORD
hubot.HUBOT_HIPCHAT_PASSWORD: xxxx
```

On Windows, credentials are stored in Windows Credential Manager as generic credentials with target names of the form `envchain-NAMESPACE/KEY`. You can view or delete them through **Control Panel → Credential Manager → Windows Credentials**.

### Execute commands with defined variables

```
$ env | grep AWS_ || echo "No AWS_ env vars"
No AWS_ env vars
$ envchain aws env | grep AWS_
AWS_ACCESS_KEY_ID=my-access-key
AWS_SECRET_ACCESS_KEY=secret
$ envchain aws s3cmd blah blah blah
⋮
```

```
$ envchain hubot env | grep AWS_ || echo "No AWS_ env vars for hubot"
No AWS_ env vars for hubot
$ envchain hubot env | grep HUBOT_
HUBOT_HIPCHAT_PASSWORD: xxxx
```

### Execute Linux commands in WSL with credentials from Windows Credential Manager

In WSL, `envchain` can read secrets from Windows Credential Manager via Windows `envchain.exe`:

```
$ envchain aws env | grep AWS_
AWS_ACCESS_KEY_ID=my-access-key
AWS_SECRET_ACCESS_KEY=secret
```

If `envchain.exe` is not on PATH in WSL, set it explicitly:

```
$ ENVCHAIN_WINDOWS_BIN=/mnt/c/Users/<you>/bin/envchain.exe envchain aws env
```

You may specify multiple namespaces at once, with separating by commas:

```
$ envchain aws,hubot env | grep 'AWS_\|HUBOT_'
AWS_ACCESS_KEY_ID=my-access-key
AWS_SECRET_ACCESS_KEY=secret
HUBOT_HIPCHAT_PASSWORD: xxxx
```


### More options

#### `--list`

List namespaces that have been created
```
$ envchain --list
aws
hubot
```

#### `--noecho`

Do not echo user input
```
$ envchain --set --noecho foo BAR
foo.BAR (noecho):
```

#### `--require-passphrase`

Always ask for keychain passphrase before reading secrets (macOS only).

> **Note:** `--require-passphrase` is not supported on Linux or Windows. The flag is accepted but silently ignored on those platforms.

#### `--no-require-passphrase`

Do not ask for keychain passphrase (macOS only).

## Platform notes

| Feature | macOS | Linux | Windows |
|---|---|---|---|
| Credential store | Keychain | D-Bus Secret Service | Credential Manager |
| `--require-passphrase` | ✅ | ❌ | ❌ |
| `--noecho` | ✅ | ✅ | ✅ |

### WSL notes

- `envchain` built in WSL delegates secret operations to Windows `envchain.exe`.
- Use `ENVCHAIN_WINDOWS_BIN` when `envchain.exe` is not discoverable on WSL PATH.

## Sponsor

<a href='https://ko-fi.com/J3J8CKMUU' target='_blank'><img height='36' style='border:0px;height:36px;' src='https://cdn.ko-fi.com/cdn/kofi3.png?v=3' border='0' alt='Buy Me a Coffee at ko-fi.com' /></a>

### Screenshot

#### OS X Keychain

![](http://img.sorah.jp/20140519_060147_dqwbh_20140519_060144_s1zku_Keychain_Access.png)

#### Seahorse (gnome-keyring)

![](https://img.sorah.jp/2016-06-08_19-46-10_ff9c444.png)

## Author

- Sorah Fukumori <her@sorah.jp>
- eagletmt

## License

MIT License
