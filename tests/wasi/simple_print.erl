-module(simple_print).
-export([start/0]).

start() ->
    io:put_chars(standard_io, <<"Hello, WASI stdout!\n">>).
