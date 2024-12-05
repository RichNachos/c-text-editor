# Warm Editor

## Terminal Text Editor Written in C.

This repository contains the code for a personal project I wanted to write for a long time, a terminal text editor (just like `nano`) written completely in C from scratch.

The text editor has several features:
* Opening viewing and editing existing files
* Modifying and saving files (Ctrl + S)
* Creating new files (Ctrl + S with file name prompt)
* Finding occurances of a query (Ctrl + F with query prompt)
* Finding next and previous occurances with using arrow keys
* Current line, total lines, and status bar
* Syntax highlighting in case of .c file
* Comment highlighting
* Exit mapped to Ctrl + Q
* In case of unsaved changes Ctrl + Q must be pressed 3 times

## Compilation
```
$ sudo apt install make gcc

$ make
```

## Run

Running without arguments opens up an empty file, which you can save. The editor will ask you for the filename when saving a new file. Passing an argument will open that file.
```
$ ./warm example.txt

$ ./warm
```