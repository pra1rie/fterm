# fuckterm
Simple (fucky) VTE-based terminal emulator

## Building
dependencies: gcc, libgtk-3.0, libvte-2.91 and libx11

compile with:
```
$ make
```

## Installing
```
$ make install
```

## Usage
```
$ fterm -h, --help               show this help and exit
$ fterm -v, --version            print version information
$ fterm -c, --config path        load config file from path
$ fterm -w, --window window_id   embed fterm into another X11 window
```
default configuration is stored in [~/.config/fterm/fterm.cfg](/fterm.cfg)

## Images
![neovim within fterm](https://i.imgur.com/CD0ikMl.png)
