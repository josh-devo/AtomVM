-module(stream_cat).
-export([start/0]).

%% Simple cat program - reads from stdin and writes to stdout
%% Demonstrates streaming I/O in AtomVM WASI
%%
%% Usage: cat input.txt | wasmtime AtomVM.wasm estdlib.avm stream_cat.beam | gzip > output.gz

start() ->
    stream_loop().

stream_loop() ->
    case io:get_chars(standard_io, 4096) of
        eof ->
            % End of input
            ok;
        {ok, Data} ->
            % Write data to stdout
            case io:put_chars(standard_io, Data) of
                ok ->
                    % Continue reading
                    stream_loop();
                Error ->
                    Error
            end;
        {error, Reason} ->
            {error, Reason}
    end.
