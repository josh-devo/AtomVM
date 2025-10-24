-module(test_file_io).
-export([start/0]).

start() ->
    io:format("[TEST] AtomVM WASI File I/O Tests~n"),

    TestFile = "test_data.txt",
    TestContent = <<"Hello from AtomVM WASI!">>,

    % Test write
    case file:write_file(TestFile, TestContent) of
        ok ->
            io:format("  ✓ File written successfully~n"),
            % Test read
            case file:read_file(TestFile) of
                {ok, TestContent} ->
                    io:format("  ✓ File read successfully, content matches~n"),
                    % Test file info
                    case file:read_file_info(TestFile) of
                        {ok, FileInfo} ->
                            io:format("  ✓ File info retrieved~n"),
                            io:format("    Size: ~p bytes~n", [element(2, FileInfo)]),
                            io:format("    Type: ~p~n", [element(3, FileInfo)]),
                            io:format("~nAll file I/O tests passed!~n"),
                            ok;
                        {error, Reason} ->
                            io:format("  ✗ Failed to get file info: ~p~n", [Reason]),
                            {error, {stat_failed, Reason}}
                    end;
                {ok, OtherContent} ->
                    io:format("  ✗ Content mismatch!~n"),
                    io:format("    Expected: ~p~n", [TestContent]),
                    io:format("    Got: ~p~n", [OtherContent]),
                    {error, content_mismatch};
                {error, Reason} ->
                    io:format("  ✗ Failed to read file: ~p~n", [Reason]),
                    {error, {read_failed, Reason}}
            end;
        {error, Reason} ->
            io:format("  ✗ Failed to write file: ~p~n", [Reason]),
            {error, {write_failed, Reason}}
    end.
