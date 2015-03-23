# lua-better-repl

lua-better-repl is a repl for lua, which is a bit better than default one.

## Usage

    % make
    % ./bin/lua-better-repl
    lua> print("Hello, world!")
    Hello, world!
    lua> quit()
    %

Lua 5.1.4 is recommended.

## Defferences from default one

It suggests suitable variables with TAB key.

    lua> hoge = 10
    lua> hoga = 20
    lua> hog<press TAB here>
    hoga  hoge
    lua> hog

It also shows the contants or type of variables.

    lua> hoge = "Hoge"
    lua> hoge
    Hoge
    lua> foobar = {"Foo", "Bar"}
    lua> foobar
    table: 0x14cf820
    lua> 

## License

This software is released under the MIT License, see LICENSE.txt.
