# Tex

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
  ### User Controls
  Key |  Usage
  ----|-------
  ARROW_UP   | Move cursor up
  ARROW_DOWN | Move curosr down
  ARROR_RIGHT| Move cursor right
  ARROW_LEFT | Move cursoor left
  PAGE_UP    | Jump to BOF
  PAGE_DOWN  | Jump to EOF
  HOME       | Jump cursor to start of line
  END        | Jump cursor to end of line
  CTRL-S     | Save the file on disk
  CTRL-Q     | Quit the editor

  ```bash
  ./Tex            # Open Tex to new file
  ./Tex test.txt   # Open file test.txt in Tex
  ./Tex test_1.txt # TODO: Create file if not found in current directory
  ...
  ```
  - - -

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.
- - -

## License
[GPL v3.0](https://choosealicense.com/licenses/gpl-3.0/)
- - -
