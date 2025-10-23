%
% Test basic arithmetic operations
%
-module(math_test).
-export([start/0]).

start() ->
    erlang:display({add, 2 + 3, 5}),
    erlang:display({mult, 4 * 5, 20}),
    erlang:display({sub, 10 - 3, 7}),
    erlang:display(math_tests_passed),
    ok.
