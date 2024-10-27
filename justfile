set shell := ["zsh", "-uc"]

default:build

build:
  cmake -S . -B build
  cmake --build build

test:
  cmake --build build --target test

speed:
  cmake --build build --target speed

tidy: 
  cmake --build build --target tidy

format: 
  cmake --build build --target format

check point:
  cmake --build build --target check{{point}}

line-of-code:
  python ./scripts/lines-of-code

clean:
  rm -rf ./build
