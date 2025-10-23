%
% Test atom operations
%
-module(atom_test).
-export([start/0]).

start() ->
    A = ok,
    B = error,
    erlang:display({atom_match, A =:= ok}),
    erlang:display({atom_nomatch, B =/= ok}),
    erlang:display(atom_tests_passed),
    ok.
