-module(stream_process).
-export([start/0]).

%% Processes streaming data line by line
%% Adds a prefix and line number to each line

start() ->
    io:format("WASM Stream Processor started~n", []),
    process_lines(1).

process_lines(LineNum) ->
    case io:get_line(standard_io, "") of
        eof ->
            io:format("End of stream. Processed ~p lines.~n", [LineNum - 1]),
            ok;
        {ok, Line} ->
            % Process: add line number prefix
            Processed = io_lib:format("[Line ~p] ~s", [LineNum, Line]),
            io:put_chars(standard_io, Processed),
            process_lines(LineNum + 1);
        {error, Reason} ->
            io:format("Error reading: ~p~n", [Reason]),
            {error, Reason}
    end.
