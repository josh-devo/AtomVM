-module(test_file_comprehensive).
-export([start/0]).

start() ->
    % Test 1: Write and read a simple file
    test_simple_write_read(),

    % Test 2: Write and read binary data
    test_binary_data(),

    % Test 3: Overwrite existing file
    test_overwrite(),

    % Test 4: Read non-existent file
    test_read_nonexistent(),

    % Test 5: File info for different file types
    test_file_info(),

    % Test 6: Write empty file
    test_empty_file(),

    % Test 7: Large file
    test_large_file(),

    ok.

test_simple_write_read() ->
    File = "test_simple.txt",
    Content = <<"Simple test content">>,

    ok = file:write_file(File, Content),
    {ok, Content} = file:read_file(File),
    ok.

test_binary_data() ->
    File = "test_binary.bin",
    % Binary data with various byte values
    Content = <<0, 1, 2, 255, 254, 253, 128, 127>>,

    ok = file:write_file(File, Content),
    {ok, Content} = file:read_file(File),
    ok.

test_overwrite() ->
    File = "test_overwrite.txt",
    Content1 = <<"First content">>,
    Content2 = <<"Second content, much longer than the first">>,

    ok = file:write_file(File, Content1),
    {ok, Content1} = file:read_file(File),

    % Overwrite with longer content
    ok = file:write_file(File, Content2),
    {ok, Content2} = file:read_file(File),

    % Overwrite with shorter content
    ok = file:write_file(File, Content1),
    {ok, Content1} = file:read_file(File),
    ok.

test_read_nonexistent() ->
    File = "this_file_does_not_exist_12345.txt",
    {error, enoent} = file:read_file(File),
    ok.

test_file_info() ->
    File = "test_file_info.txt",
    Content = <<"Test content for file info">>,

    ok = file:write_file(File, Content),
    {ok, FileInfo} = file:read_file_info(File),

    % FileInfo should be a tuple with at least size and type
    true = is_tuple(FileInfo),
    true = tuple_size(FileInfo) >= 2,

    % Check size (element 2)
    Size = element(2, FileInfo),
    true = is_integer(Size),
    true = Size == byte_size(Content),

    % Check type (element 3) - should be 'regular' for a regular file
    Type = element(3, FileInfo),
    true = Type == regular,

    ok.

test_empty_file() ->
    File = "test_empty.txt",
    Content = <<>>,

    ok = file:write_file(File, Content),
    {ok, Content} = file:read_file(File),
    {ok, FileInfo} = file:read_file_info(File),

    Size = element(2, FileInfo),
    true = Size == 0,
    ok.

test_large_file() ->
    File = "test_large.bin",
    % Create 100KB of data
    Block = binary:copy(<<"ABCDEFGHIJ">>, 100),  % 1KB
    Content = binary:copy(Block, 100),  % 100KB

    ok = file:write_file(File, Content),
    {ok, Content} = file:read_file(File),
    {ok, FileInfo} = file:read_file_info(File),

    Size = element(2, FileInfo),
    true = Size == byte_size(Content),
    ok.
