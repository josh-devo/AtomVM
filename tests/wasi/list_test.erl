%
% Test list operations
%
-module(list_test).
-export([start/0]).

start() ->
    L = [1, 2, 3],
    erlang:display({list, L}),
    erlang:display({hd, hd(L), 1}),
    erlang:display({tl, tl(L), [2, 3]}),
    erlang:display(list_tests_passed),
    ok.
