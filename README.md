# Tex 1.0.0

![Travis (.com)](https://img.shields.io/travis/com/jlemanski1/Tex.svg?style=flat-square)
![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/jlemanski1/Tex.svg?style=flat-square)
![GitHub issues](https://img.shields.io/github/issues/jlemanski1/Tex.svg?style=flat-square)
![GitHub](https://img.shields.io/github/license/jlemanski1/Tex.svg?style=flat-square)
![GitHub last commit](https://img.shields.io/github/last-commit/jlemanski1/Tex.svg?style=flat-square)

Tex is a minimalist text editor for bash terminals, written in c, with 0 dependencies. The reason for starting this project
was to have a fun way to implement all the knowledge I've recently learned in school. Especially since my school projects have 
been less exciting, a 64Kb Memory allocator for a recent example. Has no relation to the typesetting system, TeX.
- - - 

## Features
* lightweight minimalist editor with all the basic features you'd expect
* Robust incremental search with ability to jump between matches
* Filetype detection
* Language based syntax highlighting
* Open source
- - -

## Installation
```bash
# clone project from git into current directory
git clone https://github.com/jlemanski1/Tex.git

# Compile
make all

# Install
make install
```
- - -

## Usage

  ```bash
  tex            # Create a new file with Tex
  tex test.txt   # Open file test.txt in Tex
  ...
  ```
  
  ### User Controls
  Key |  Usage
  :----:|:-------:
  **NAVIGATION** |**-------------------------------------------**
  `ARROW_UP`   | Move cursor up
  `ARROW_DOWN` | Move curosr down
  `ARROR_RIGHT`| Move cursor right
  `ARROW_LEFT` | Move cursoor left
  `PAGE_UP`    | Jump cursor to BOF
  `PAGE_DOWN`  | Jump cursor to EOF
  `HOME`       | Jump cursor to start of line
  `END`        | Jump cursor to end of line
  **TEXT MANIPULATION** |**-------------------------------------------**
  `DEL`        | Delete character right of curor
  `BACKSPACE`  | Delete character left of cursor
  `CTRL-H`     | Delete character left of cursor
  `ENTER`      | Insert a new line
  **EDITOR CONTROLS** |**-------------------------------------------**
  `CTRL-S`     | Save the file on disk
  `CTRL-F`     | Find a string in the file
  `CTRL-Q`     | Quit the editor
  - - -

### Future Tasks:
- [x] Implement a search feature
- [x] Implement syntax highlighting
- [x] Filetype detection
- [x] Language based syntax highlighting
- [ ] Shift select words
- [ ] Copy & Paste
- [ ] Auto indent newlines to same level as previous one
- - -

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
- - -

## Versioning
This project uses the Semantic Versioning idealogy, a short summary can be found below, with more information found on the [website](https://semver.org/spec/v2.0.0.html)
   ### Summary
   Given a version number MAJOR.MINOR.PATCH, increment the:
1. MARJOR version when you make incompatible API changes,
2. MINOR version when you add functionality in a backwards-compatible manner, and 
3. PATCH version when you make backwards-compatible bug fixes.

Additional labels for pre-release and build metadata are available as extensions to the MAJOR.MINOR.PATCH format.
- - -

## License
[GPL v3.0](https://choosealicense.com/licenses/gpl-3.0/)
- - -
