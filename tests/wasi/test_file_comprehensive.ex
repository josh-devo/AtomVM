defmodule TestFileComprehensive do
  def start do
    # Test 1: Basic write and read
    test_basic_write_read()

    # Test 2: Write iolist (list of binaries/strings)
    test_iolist_write()

    # Test 3: Unicode content
    test_unicode()

    # Test 4: Multiple files simultaneously
    test_multiple_files()

    # Test 5: Read non-existent file error handling
    test_error_handling()

    # Test 6: File size variations
    test_size_variations()

    # Test 7: Special characters in content
    test_special_chars()

    # Test 8: Nested directory (if supported)
    test_nested_path()

    :ok
  end

  defp test_basic_write_read do
    file = "test_elixir_basic.txt"
    content = "Hello from Elixir on AtomVM WASI!"

    :ok = :file.write_file(file, content)
    {:ok, ^content} = :file.read_file(file)
    :ok
  end

  defp test_iolist_write do
    file = "test_elixir_iolist.txt"
    # iolist - list of strings and binaries
    iolist = ["Part 1", " ", <<"Part 2">>, " ", "Part 3"]
    expected = "Part 1 Part 2 Part 3"

    :ok = :file.write_file(file, iolist)
    {:ok, content} = :file.read_file(file)

    # Verify content matches expected
    if content == expected do
      :ok
    else
      {:error, {:content_mismatch, expected, content}}
    end
  end

  defp test_unicode do
    file = "test_elixir_unicode.txt"
    # Unicode content with various characters
    content = "Hello 世界 🌍 Привет مرحبا"

    :ok = :file.write_file(file, content)
    {:ok, ^content} = :file.read_file(file)
    :ok
  end

  defp test_multiple_files do
    files = [
      {"test_multi_1.txt", "Content 1"},
      {"test_multi_2.txt", "Content 2"},
      {"test_multi_3.txt", "Content 3"},
      {"test_multi_4.txt", "Content 4"},
      {"test_multi_5.txt", "Content 5"}
    ]

    # Write all files
    Enum.each(files, fn {file, content} ->
      :ok = :file.write_file(file, content)
    end)

    # Read and verify all files
    Enum.each(files, fn {file, expected_content} ->
      {:ok, content} = :file.read_file(file)
      if content != expected_content do
        raise "Content mismatch for #{file}"
      end
    end)

    :ok
  end

  defp test_error_handling do
    # Test reading non-existent file
    case :file.read_file("non_existent_file_xyz.txt") do
      {:error, :enoent} -> :ok
      other -> {:error, {:unexpected_result, other}}
    end

    # Test file info for non-existent file
    case :file.read_file_info("another_non_existent_file.txt") do
      {:error, :enoent} -> :ok
      other -> {:error, {:unexpected_result, other}}
    end
  end

  defp test_size_variations do
    # Test various file sizes
    sizes = [0, 1, 10, 100, 1000, 10000]

    Enum.each(sizes, fn size ->
      file = "test_size_#{size}.bin"
      content = String.duplicate("X", size)

      :ok = :file.write_file(file, content)
      {:ok, read_content} = :file.read_file(file)

      if byte_size(read_content) != size do
        raise "Size mismatch for file of size #{size}"
      end

      {:ok, file_info} = :file.read_file_info(file)
      info_size = elem(file_info, 1)

      if info_size != size do
        raise "File info size mismatch: expected #{size}, got #{info_size}"
      end
    end)

    :ok
  end

  defp test_special_chars do
    file = "test_special_chars.txt"
    # Content with newlines, tabs, special ASCII characters
    content = "Line 1\nLine 2\tTabbed\rCarriage Return\x00Null\x01SOH\xFF"

    :ok = :file.write_file(file, content)
    {:ok, ^content} = :file.read_file(file)
    :ok
  end

  defp test_nested_path do
    # Try to write to a nested path (may fail if directory doesn't exist)
    # This tests error handling for invalid paths
    file = "nonexistent_dir/test.txt"

    case :file.write_file(file, "test") do
      # If it succeeds, great! WASI might create the directory
      :ok ->
        {:ok, _} = :file.read_file(file)
        :ok

      # If it fails with enoent, that's expected (directory doesn't exist)
      {:error, :enoent} ->
        :ok

      # Any other error
      {:error, _reason} ->
        # This is acceptable - just means nested paths aren't supported yet
        :ok
    end
  end
end
