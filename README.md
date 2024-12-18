# f(uck)term
shitty VTE-based terminal emulator

## Building
dependencies: gcc, libgtk-3.0, libvte-2.91 and libx11

compile with:
```sh
$ make
```

## Installing
```sh
$ make install
```

## Usage
```
$ fterm -w <xid>   embeds fterm within another X application
$ fterm -c <file>  reads config from <file>
```
