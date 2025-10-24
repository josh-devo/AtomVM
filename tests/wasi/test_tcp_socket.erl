-module(test_tcp_socket).
-export([start/0]).

%% Simple TCP socket test for WASI
%% Tests basic TCP client functionality (Phase 1)
%% Requires echo server running on 127.0.0.1:8080
%% Start server with: socat TCP-LISTEN:8080,reuseaddr,fork EXEC:cat
%%
%% Returns: ok on success, {error, Reason} on failure

start() ->
    Host = "127.0.0.1",
    Port = 8080,

    case gen_tcp:connect(Host, Port, [binary, {active, false}], 5000) of
        {ok, Socket} ->
            Message = <<"Hello from AtomVM WASI">>,

            case gen_tcp:send(Socket, Message) of
                ok ->
                    case gen_tcp:recv(Socket, 0, 5000) of
                        {ok, Data} ->
                            gen_tcp:close(Socket),
                            if
                                Data =:= Message ->
                                    ok;
                                true ->
                                    {error, echo_mismatch}
                            end;
                        {error, RecvError} ->
                            gen_tcp:close(Socket),
                            {error, RecvError}
                    end;
                {error, SendError} ->
                    gen_tcp:close(Socket),
                    {error, SendError}
            end;
        {error, ConnectError} ->
            {error, ConnectError}
    end.
