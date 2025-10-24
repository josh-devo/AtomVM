-module(simple_test).
-export([start/0]).

start() ->
    io:put_chars(standard_io, <<"Testing file:make_dir/1...\n">>),

    Result = file:make_dir("test_dir"),

    Msg = io_lib:format("Result: ~p~n", [Result]),
    io:put_chars(standard_io, list_to_binary(Msg)),

    ok.
