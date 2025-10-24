-module(test_tcp_minimal).
-export([start/0]).

%% Minimal TCP test - just connect, send, recv, close
%% No io:format after connection to avoid WASI stdout issues

start() ->
    case gen_tcp:connect("127.0.0.1", 8080, [binary, {active, false}], 5000) of
        {ok, Socket} ->
            % Connection succeeded - now try send
            Message = <<"Hello from AtomVM WASI">>,
            case gen_tcp:send(Socket, Message) of
                ok ->
                    % Send succeeded - now try recv
                    case gen_tcp:recv(Socket, 0, 5000) of
                        {ok, Data} ->
                            % Recv succeeded - verify echo
                            gen_tcp:close(Socket),
                            if
                                Data =:= Message ->
                                    ok;  % Success
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
