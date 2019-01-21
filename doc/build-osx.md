macOS Build Instructions and Notes
====================================
The commands in this guide should be executed in a Terminal application.
The built-in one is located in `/Applications/Utilities/Terminal.app`.

Preparation
-----------
Install the macOS command line tools:

`xcode-select --install`

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

Dependencies
----------------------

    brew install automake berkeley-db4 libtool boost miniupnpc openssl pkg-config protobuf python qt libevent qrencode

See [dependencies.md](dependencies.md) for a complete overview.

If you want to build the disk image with `make deploy` (.dmg / optional), you need RSVG:

    brew install librsvg

Berkeley DB
-----------
It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use [the installation script included in contrib/](/contrib/install_db4.sh)
like so:

```shell
./contrib/install_db4.sh .
```

from the root of the repository.

**Note**: You only need Berkeley DB if the wallet is enabled (see [*Disable-wallet mode*](/doc/build-osx.md#disable-wallet-mode)).

Build NIX Core
------------------------

1. Clone the nix source code and cd into `nix`

        git clone https://github.com/nix/nix
        cd nix

2.  Build nix-core:

    Configure and build the headless nix binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.

        ./autogen.sh
        ./configure
        make

3.  It is recommended to build and run the unit tests:

        make check

4.  You can also create a .dmg that contains the .app bundle (optional):

        make deploy

Disable-wallet mode
--------------------
When the intention is to run only a P2P node without a wallet, Bitcoin Core may be compiled in
disable-wallet mode with:

    ./configure --disable-wallet

In this case there is no dependency on Berkeley DB 4.8.

Mining is also possible in disable-wallet mode using the `getblocktemplate` RPC call.

Running
-------

NIX Core is now available at `./src/nixd`

Before running, you may create an empty configuration file:

    touch "/Users/${USER}/Library/Application Support/nix/nix.conf"

    chmod 600 "/Users/${USER}/Library/Application Support/NIX/nix.conf"

The first time you run nixd, it will start downloading the blockchain. This process could take several hours.

You can monitor the download process by looking at the debug.log file:

    tail -f $HOME/Library/Application\ Support/NIX/debug.log

Other commands:
-------

    ./src/nixd -daemon # Starts the nix daemon.
    ./src/nix-cli --help # Outputs a list of command-line options.
    ./src/nix-cli help # Outputs a list of RPC commands when the daemon is running.

Using Qt Creator as IDE
------------------------
You can use Qt Creator as an IDE, for nix development.
Download and install the community edition of [Qt Creator](https://www.qt.io/download/).
Uncheck everything except Qt Creator during the installation process.

1. Make sure you installed everything through Homebrew mentioned above
2. Do a proper ./configure --enable-debug
3. In Qt Creator do "New Project" -> Import Project -> Import Existing Project
4. Enter "nix-qt" as project name, enter src/qt as location
5. Leave the file selection as it is
6. Confirm the "summary page"
7. In the "Projects" tab select "Manage Kits..."
8. Select the default "Desktop" kit and select "Clang (x86 64bit in /usr/bin)" as compiler
9. Select LLDB as debugger (you might need to set the path to your installation)
10. Start debugging with Qt Creator

Notes
-----

* Tested on OS X 10.10 Yosemite through macOS 10.13 High Sierra on 64-bit Intel processors only.

* Building with downloaded Qt binaries is not officially supported. See the notes in [#7714](https://github.com/nix/nix/issues/7714)
