%
% Test with erlang:display/1 output
%
-module(display_test).
-export([start/0]).

start() ->
    erlang:display(ok),
    erlang:display(test_passed),
    ok.
