# mididump

Command line MIDI monitor utility for macOS

![Screenshot](https://raw.githubusercontent.com/anthroid/mididump/master/images/mididump-screenshot.png)
## Usage

Argument | Option | Comment
--- | --- | ---
`-c` | Color | default: `on`
`-d` | Decimal format | default: `off` (hexadecimal)
`-m` | Source number | default: `off`
`-n` | Source name | default: `on`
`-t` | Timestamp | default: `off`
`-x` | Extended format | default: `off`
`-z` | Zero prefix output | default: `off`
`-s` | Single-line output | default: `off`
`-h` | Help | Display comment usage

## Prerequisites

* macOS 10.12 or higher

## Compiling

The program is entirely contained within a single source (`src/mididump.c`), so compile it as you please:
```
$ gcc ./src/mididump.c -o ./bin/mididump -framework CoreMIDI -framework CoreServices
```
Or use the makefile:
* To build: `make`
* To clean the build directory: `make clean`
* To see which commands will be run by `make`: `make -n all`
* To print the makefile variables: `make print`

## Installing

Copy or symlink the executable to a location in your `$PATH`, for example:

```
$ cp ./bin/mididump /usr/local/bin/
```
Or:
```
$ ln -s ./bin/mididump /usr/local/bin/mididump
```

## Examples

Default output format (color, hexadecimal, not zero prefixed)
```
$ mididump
```

Decimal output, zero prefixed:
```
$ mididump -dz
```

Zero-prefixed, color output:
```
$ mididump -zc
```

## Notes

* I have not tested extensively on any platforms other than macOS 10.12+. Nonetheless, no special frameworks or functionality are used, so it should run on any version of macOS (in theory).
