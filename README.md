# Tex

![Travis (.com)](https://img.shields.io/travis/com/jlemanski1/Tex.svg?style=for-the-badge)
![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/jlemanski1/Tex.svg?style=for-the-badge)
![GitHub issues](https://img.shields.io/github/issues/jlemanski1/Tex.svg?style=for-the-badge)
![GitHub](https://img.shields.io/github/license/jlemanski1/Tex.svg?style=for-the-badge)
![GitHub last commit](https://img.shields.io/github/last-commit/jlemanski1/Tex.svg?style=for-the-badge)

Tex is a minimalist text editor for bash terminals, written in c, with no dependencies. The reason for this project
is to have a fun way to implement all the knowledge I've recently learned in school. Since my school projects have 
been less exciting, a 64Kb Memory allocator for a recent example. Has no relation to the typesetting system, TeX.
- - - 

## Installation
TBA

```bash
# clone fron git into current directory
git clone https://github.com/jlemanski1/Tex.git

# separate compilation
make tex.o
make tex

# all compilation
make all
...
```
- - -

## Usage

  ```bash
  ./Tex            # Open Tex to new file
  ./Tex test.txt   # Open file test.txt in Tex
  ./Tex test_1.txt # TODO: Create file if not found in current directory
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
  `PAGE_UP`    | Jump to BOF
  `PAGE_DOWN`  | Jump to EOF
  `HOME`       | Jump cursor to start of line
  `END`        | Jump cursor to end of line
  **TEXT MANIPULATION** |**-------------------------------------------**
  `DEL`        | Delete character right of curor
  `BACKSPACE`  | Delete character left of cursor
  `CTRL-H`     | Delete character left of cursor
  `ENTER`      | Insert a new line
  **EDITOR CONTROLS** |**-------------------------------------------**
  `CTRL-S`     | Save the file on disk
  `CTRL-Q`     | Quit the editor
  - - -

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
- - -

## License
[GPL v3.0](https://choosealicense.com/licenses/gpl-3.0/)
- - -
