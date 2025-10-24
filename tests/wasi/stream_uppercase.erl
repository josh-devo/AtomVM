-module(stream_uppercase).
-export([start/0]).

%% Converts streaming data to uppercase in real-time
%% Demonstrates processing data as it flows through

start() ->
    io:put_chars(standard_io, <<"=== UPPERCASE FILTER STARTED ===\n">>),
    process_loop().

process_loop() ->
    case io:get_chars(standard_io, 1024) of
        eof ->
            io:put_chars(standard_io, <<"\n=== END OF STREAM ===\n">>),
            ok;
        {ok, Data} ->
            % Convert to uppercase
            Upper = string:uppercase(Data),
            io:put_chars(standard_io, Upper),
            process_loop();
        {error, Reason} ->
            {error, Reason}
    end.
