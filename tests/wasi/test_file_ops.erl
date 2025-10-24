-module(test_file_ops).
-export([start/0]).

%%% Comprehensive test for new WASI file operations:
%%% - file:make_dir/1
%%% - file:del_dir/1
%%% - file:delete/1
%%% - file:rename/2
%%% - file:list_dir/1

start() ->
    io:put_chars(standard_io, <<"=== WASI File Operations Test ===\n\n">>),

    % Run all tests
    test_make_dir(),
    test_list_dir(),
    test_write_and_delete(),
    test_rename(),
    test_del_dir(),

    io:put_chars(standard_io, <<"\n=== All tests completed ===\n">>),
    ok.

%% Test file:make_dir/1
test_make_dir() ->
    io:put_chars(standard_io, <<"[TEST] file:make_dir/1\n">>),

    % Create a test directory
    case file:make_dir("test_dir") of
        ok ->
            io:put_chars(standard_io, <<"  ✓ Created test_dir\n">>);
        {error, Reason} ->
            Msg = io_lib:format("  ✗ Failed to create test_dir: ~p~n", [Reason]),
            io:put_chars(standard_io, list_to_binary(Msg))
    end,

    % Create a nested directory
    case file:make_dir("test_dir/subdir") of
        ok ->
            io:put_chars(standard_io, <<"  ✓ Created test_dir/subdir\n">>);
        {error, Reason2} ->
            Msg2 = io_lib:format("  ✗ Failed to create test_dir/subdir: ~p~n", [Reason2]),
            io:put_chars(standard_io, list_to_binary(Msg2))
    end,

    io:put_chars(standard_io, <<"\n">>).

%% Test file:list_dir/1
test_list_dir() ->
    io:put_chars(standard_io, <<"[TEST] file:list_dir/1\n">>),

    % List current directory
    case file:list_dir(".") of
        {ok, Files} ->
            Msg = io_lib:format("  ✓ Listed current directory (~p files)~n", [length(Files)]),
            io:put_chars(standard_io, list_to_binary(Msg)),
            % Show first few files
            show_files("    ", lists:sublist(Files, 5));
        {error, enotsup} ->
            io:put_chars(standard_io, <<"  ⚠ list_dir not supported in WASI (known limitation)\n">>);
        {error, Reason} ->
            Msg = io_lib:format("  ✗ Failed to list current directory: ~p~n", [Reason]),
            io:put_chars(standard_io, list_to_binary(Msg))
    end,

    io:put_chars(standard_io, <<"\n">>).

%% Test file:write_file/2 and file:delete/1
test_write_and_delete() ->
    io:put_chars(standard_io, <<"[TEST] file:write_file/2 + file:delete/1\n">>),

    % Write a test file
    TestData = <<"This is test data for file operations\n">>,
    case file:write_file("test_dir/test.txt", TestData) of
        ok ->
            io:put_chars(standard_io, <<"  ✓ Wrote test_dir/test.txt\n">>);
        {error, Reason} ->
            Msg = io_lib:format("  ✗ Failed to write test_dir/test.txt: ~p~n", [Reason]),
            io:put_chars(standard_io, list_to_binary(Msg))
    end,

    % Verify file exists by reading it
    case file:read_file("test_dir/test.txt") of
        {ok, Data} ->
            if
                Data =:= TestData ->
                    io:put_chars(standard_io, <<"  ✓ Verified file content matches\n">>);
                true ->
                    io:put_chars(standard_io, <<"  ✗ File content mismatch\n">>)
            end;
        {error, Reason2} ->
            Msg2 = io_lib:format("  ✗ Failed to read test_dir/test.txt: ~p~n", [Reason2]),
            io:put_chars(standard_io, list_to_binary(Msg2))
    end,

    % Delete the file
    case file:delete("test_dir/test.txt") of
        ok ->
            io:put_chars(standard_io, <<"  ✓ Deleted test_dir/test.txt\n">>);
        {error, Reason3} ->
            Msg3 = io_lib:format("  ✗ Failed to delete test_dir/test.txt: ~p~n", [Reason3]),
            io:put_chars(standard_io, list_to_binary(Msg3))
    end,

    % Verify file is gone
    case file:read_file("test_dir/test.txt") of
        {error, enoent} ->
            io:put_chars(standard_io, <<"  ✓ Verified file was deleted\n">>);
        {ok, _} ->
            io:put_chars(standard_io, <<"  ✗ File still exists after delete\n">>);
        {error, OtherReason} ->
            MsgX = io_lib:format("  ? Unexpected error reading deleted file: ~p~n", [OtherReason]),
            io:put_chars(standard_io, list_to_binary(MsgX))
    end,

    io:put_chars(standard_io, <<"\n">>).

%% Test file:rename/2
test_rename() ->
    io:put_chars(standard_io, <<"[TEST] file:rename/2\n">>),

    % Create a file to rename
    case file:write_file("test_dir/old_name.txt", <<"test content">>) of
        ok ->
            io:put_chars(standard_io, <<"  ✓ Created test_dir/old_name.txt\n">>);
        {error, Reason} ->
            Msg = io_lib:format("  ✗ Failed to create old_name.txt: ~p~n", [Reason]),
            io:put_chars(standard_io, list_to_binary(Msg))
    end,

    % Rename the file
    case file:rename("test_dir/old_name.txt", "test_dir/new_name.txt") of
        ok ->
            io:put_chars(standard_io, <<"  ✓ Renamed old_name.txt to new_name.txt\n">>);
        {error, Reason2} ->
            Msg2 = io_lib:format("  ✗ Failed to rename: ~p~n", [Reason2]),
            io:put_chars(standard_io, list_to_binary(Msg2))
    end,

    % Verify old name is gone
    case file:read_file("test_dir/old_name.txt") of
        {error, enoent} ->
            io:put_chars(standard_io, <<"  ✓ Old name no longer exists\n">>);
        {ok, _} ->
            io:put_chars(standard_io, <<"  ✗ Old name still exists\n">>);
        _ ->
            ok
    end,

    % Verify new name exists
    case file:read_file("test_dir/new_name.txt") of
        {ok, <<"test content">>} ->
            io:put_chars(standard_io, <<"  ✓ New name exists with correct content\n">>);
        {ok, _} ->
            io:put_chars(standard_io, <<"  ✗ New name has wrong content\n">>);
        {error, Reason3} ->
            Msg3 = io_lib:format("  ✗ Failed to read new name: ~p~n", [Reason3]),
            io:put_chars(standard_io, list_to_binary(Msg3))
    end,

    % Clean up
    file:delete("test_dir/new_name.txt"),

    io:put_chars(standard_io, <<"\n">>).

%% Test file:del_dir/1
test_del_dir() ->
    io:put_chars(standard_io, <<"[TEST] file:del_dir/1\n">>),

    % Delete the subdirectory (should be empty)
    case file:del_dir("test_dir/subdir") of
        ok ->
            io:put_chars(standard_io, <<"  ✓ Deleted test_dir/subdir\n">>);
        {error, Reason} ->
            Msg = io_lib:format("  ✗ Failed to delete test_dir/subdir: ~p~n", [Reason]),
            io:put_chars(standard_io, list_to_binary(Msg))
    end,

    % Delete the main test directory
    case file:del_dir("test_dir") of
        ok ->
            io:put_chars(standard_io, <<"  ✓ Deleted test_dir\n">>);
        {error, Reason2} ->
            Msg2 = io_lib:format("  ✗ Failed to delete test_dir: ~p~n", [Reason2]),
            io:put_chars(standard_io, list_to_binary(Msg2))
    end,

    % Verify directory is gone
    case file:list_dir("test_dir") of
        {error, enoent} ->
            io:put_chars(standard_io, <<"  ✓ Verified test_dir was deleted\n">>);
        {ok, _} ->
            io:put_chars(standard_io, <<"  ✗ test_dir still exists\n">>);
        {error, OtherReason} ->
            MsgX = io_lib:format("  ? Unexpected error: ~p~n", [OtherReason]),
            io:put_chars(standard_io, list_to_binary(MsgX))
    end,

    io:put_chars(standard_io, <<"\n">>).

%% Helper to show files
show_files(_, []) ->
    ok;
show_files(Prefix, [File | Rest]) ->
    Msg = io_lib:format("~s- ~s~n", [Prefix, File]),
    io:put_chars(standard_io, list_to_binary(Msg)),
    show_files(Prefix, Rest).
