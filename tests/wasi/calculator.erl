-module(calculator).
-export([start/0, add/2, multiply/2, factorial/1]).

start() ->
    erlang:display({hello, from_wasi}),
    
    % Test basic arithmetic
    Sum = add(10, 32),
    erlang:display({add_result, Sum}),
    
    % Test multiplication
    Product = multiply(6, 7),
    erlang:display({multiply_result, Product}),
    
    % Test factorial
    Fact5 = factorial(5),
    erlang:display({factorial_5, Fact5}),
    
    % Test with spawned process
    Pid = spawn(fun() -> worker_loop() end),
    Pid ! {self(), compute, add, 100, 200},
    receive
        {result, Result} -> erlang:display({worker_result, Result})
    after 1000 ->
        erlang:display(timeout)
    end,
    
    ok.

add(A, B) -> A + B.

multiply(A, B) -> A * B.

factorial(0) -> 1;
factorial(N) when N > 0 -> N * factorial(N - 1).

worker_loop() ->
    receive
        {From, compute, add, A, B} ->
            From ! {result, A + B},
            worker_loop();
        {From, compute, multiply, A, B} ->
            From ! {result, A * B},
            worker_loop();
        stop ->
            ok
    end.
