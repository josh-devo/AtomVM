-module(event_processor).
-export([start/0, server_loop/1]).

%%% Event Processing Server
%%% Demonstrates real-time bidirectional communication:
%%% - Host sends events via stdin
%%% - Server processes events and emits responses via stdout
%%% - Host can monitor responses in real-time

start() ->
    io:put_chars(standard_io, <<"=== INTERACTIVE EVENT PROCESSOR ===\n">>),
    io:put_chars(standard_io, <<"Waiting for events on stdin...\n">>),
    io:put_chars(standard_io, <<"Send events (one per line), 'STATS' for statistics, 'QUIT' to exit\n">>),
    io:put_chars(standard_io, <<"===================================\n\n">>),
    
    % Start the event processing server
    ServerPid = spawn(?MODULE, server_loop, [#{events => 0, bytes => 0, start_time => erlang:monotonic_time(millisecond)}]),
    
    % Register it for easy access
    register(event_server, ServerPid),
    
    % Start reading stdin
    read_loop(ServerPid).

%% Event processing server loop
server_loop(State) ->
    receive
        {process_event, Data, From} ->
            % Process the event
            EventNum = maps:get(events, State) + 1,
            ByteCount = maps:get(bytes, State) + byte_size(Data),
            Uptime = erlang:monotonic_time(millisecond) - maps:get(start_time, State),
            
            % Emit response event
            Response = io_lib:format("[EVENT] {type: event_processed, num: ~p, bytes: ~p, uptime_ms: ~p, data: \"~s\"}~n",
                                    [EventNum, byte_size(Data), Uptime, string:trim(binary_to_list(Data))]),
            io:put_chars(standard_io, list_to_binary(Response)),
            
            From ! ok,
            
            NewState = State#{events => EventNum, bytes => ByteCount},
            server_loop(NewState);
            
        {get_stats, From} ->
            Uptime = erlang:monotonic_time(millisecond) - maps:get(start_time, State),
            
            Stats = io_lib:format("[EVENT] {type: stats, events_processed: ~p, total_bytes: ~p, uptime_ms: ~p}~n",
                                  [maps:get(events, State), maps:get(bytes, State), Uptime]),
            io:put_chars(standard_io, list_to_binary(Stats)),
            
            From ! ok,
            server_loop(State);
            
        stop ->
            ok
    end.

%% Read stdin loop
read_loop(ServerPid) ->
    case io:get_chars(standard_io, 1024) of
        eof ->
            io:put_chars(standard_io, <<"\n>>> Received EOF, shutting down <<<\n">>),
            ServerPid ! {get_stats, self()},
            receive ok -> ok end,
            ServerPid ! stop,
            ok;
            
        {error, Reason} ->
            io:put_chars(standard_io, list_to_binary(io_lib:format("Error reading: ~p~n", [Reason]))),
            {error, Reason};
            
        {ok, Data} ->
            Line = string:trim(binary_to_list(Data)),
            
            case Line of
                "" ->
                    % Skip empty lines
                    read_loop(ServerPid);
                    
                "QUIT" ->
                    io:put_chars(standard_io, <<"\n>>> QUIT command received <<<\n">>),
                    ServerPid ! {get_stats, self()},
                    receive ok -> ok end,
                    ServerPid ! stop,
                    ok;
                    
                "STATS" ->
                    ServerPid ! {get_stats, self()},
                    receive ok -> ok end,
                    read_loop(ServerPid);
                    
                _ ->
                    % Process the event
                    ServerPid ! {process_event, Data, self()},
                    receive ok -> ok end,
                    read_loop(ServerPid)
            end
    end.
