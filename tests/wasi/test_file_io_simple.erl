-module(test_file_io_simple).
-export([start/0]).

start() ->
    TestFile = "test_data.txt",
    TestContent = <<"Hello from AtomVM WASI!">>,

    % Test write
    WriteResult = file:write_file(TestFile, TestContent),

    % Test read
    ReadResult = case WriteResult of
        ok ->
            file:read_file(TestFile);
        _ ->
            {error, write_failed}
    end,

    % Test file info
    InfoResult = case ReadResult of
        {ok, TestContent} ->
            file:read_file_info(TestFile);
        _ ->
            {error, read_failed}
    end,

    % Return final result
    case InfoResult of
        {ok, _FileInfo} ->
            ok;
        Error ->
            Error
    end.
