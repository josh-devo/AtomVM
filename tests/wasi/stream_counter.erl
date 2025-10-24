-module(stream_counter).
-export([start/0]).

%% Counts bytes and chunks as data streams through
%% Demonstrates real-time processing with statistics

start() ->
    io:put_chars(standard_io, <<">>> STREAM COUNTER ACTIVE <<<\n">>),
    count_loop(0, 0).

count_loop(ChunkCount, ByteCount) ->
    case io:get_chars(standard_io, 1024) of
        eof ->
            Summary = io_lib:format("~n>>> STREAM COMPLETE: ~p chunks, ~p bytes <<<~n", 
                                    [ChunkCount, ByteCount]),
            io:put_chars(standard_io, list_to_binary(Summary)),
            ok;
        {ok, Data} ->
            % Pass through the data unchanged
            io:put_chars(standard_io, Data),
            % Update counters
            NewBytes = ByteCount + byte_size(Data),
            count_loop(ChunkCount + 1, NewBytes);
        {error, Reason} ->
            {error, Reason}
    end.
