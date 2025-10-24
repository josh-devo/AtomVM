defmodule TestFileIO do
  @moduledoc """
  File I/O tests for AtomVM WASI
  Tests file system operations through WASI

  This tests:
  1. Writing files
  2. Reading files
  3. File existence checks
  4. Directory operations
  5. File deletion

  Returns: :ok on success, {:error, reason} on failure
  """

  def start do
    test_dir = "test_files"
    test_file = "#{test_dir}/test.txt"
    test_content = "Hello from AtomVM WASI file system!"

    with :ok <- test_write_file(test_file, test_content),
         :ok <- test_read_file(test_file, test_content),
         :ok <- test_file_exists(test_file),
         :ok <- test_file_info(test_file),
         :ok <- test_delete_file(test_file),
         :ok <- test_directory_operations(test_dir) do
      IO.puts("All file I/O tests passed!")
      :ok
    else
      {:error, reason} = error ->
        IO.puts("File I/O test failed: #{inspect(reason)}")
        error
    end
  end

  defp test_write_file(path, content) do
    IO.puts("[TEST] Writing file: #{path}")

    # Ensure directory exists - manually extract directory name
    dir = extract_dirname(path)
    case File.mkdir_p(dir) do
      :ok -> :ok
      {:error, :eexist} -> :ok
      {:error, reason} ->
        IO.puts("  Failed to create directory: #{inspect(reason)}")
        {:error, {:mkdir_failed, reason}}
    end

    case File.write(path, content) do
      :ok ->
        IO.puts("  ✓ File written successfully")
        :ok
      {:error, reason} ->
        IO.puts("  ✗ Failed to write file: #{inspect(reason)}")
        {:error, {:write_failed, reason}}
    end
  end

  defp extract_dirname(path) do
    # Simple dirname extraction without using Path module
    parts = String.split(path, "/")
    parts
    |> Enum.reverse()
    |> Enum.drop(1)
    |> Enum.reverse()
    |> Enum.join("/")
  end

  defp test_read_file(path, expected_content) do
    IO.puts("[TEST] Reading file: #{path}")

    case File.read(path) do
      {:ok, content} ->
        if content == expected_content do
          IO.puts("  ✓ File read successfully, content matches")
          :ok
        else
          IO.puts("  ✗ Content mismatch!")
          IO.puts("    Expected: #{inspect(expected_content)}")
          IO.puts("    Got: #{inspect(content)}")
          {:error, :content_mismatch}
        end
      {:error, reason} ->
        IO.puts("  ✗ Failed to read file: #{inspect(reason)}")
        {:error, {:read_failed, reason}}
    end
  end

  defp test_file_exists(path) do
    IO.puts("[TEST] Checking file existence: #{path}")

    if File.exists?(path) do
      IO.puts("  ✓ File exists")
      :ok
    else
      IO.puts("  ✗ File does not exist")
      {:error, :file_not_found}
    end
  end

  defp test_file_info(path) do
    IO.puts("[TEST] Getting file info: #{path}")

    case File.stat(path) do
      {:ok, stat} ->
        IO.puts("  ✓ File stat retrieved")
        IO.puts("    Size: #{stat.size} bytes")
        IO.puts("    Type: #{stat.type}")
        :ok
      {:error, reason} ->
        IO.puts("  ✗ Failed to get file info: #{inspect(reason)}")
        {:error, {:stat_failed, reason}}
    end
  end

  defp test_delete_file(path) do
    IO.puts("[TEST] Deleting file: #{path}")

    case File.rm(path) do
      :ok ->
        IO.puts("  ✓ File deleted successfully")
        # Verify it's gone
        if File.exists?(path) do
          IO.puts("  ✗ File still exists after deletion!")
          {:error, :delete_failed}
        else
          :ok
        end
      {:error, reason} ->
        IO.puts("  ✗ Failed to delete file: #{inspect(reason)}")
        {:error, {:delete_failed, reason}}
    end
  end

  defp test_directory_operations(dir) do
    IO.puts("[TEST] Testing directory operations: #{dir}")

    # List directory
    case File.ls(dir) do
      {:ok, files} ->
        IO.puts("  ✓ Directory listed, contains #{length(files)} files")

        # Remove directory
        case File.rmdir(dir) do
          :ok ->
            IO.puts("  ✓ Directory removed successfully")
            :ok
          {:error, reason} ->
            IO.puts("  ✗ Failed to remove directory: #{inspect(reason)}")
            {:error, {:rmdir_failed, reason}}
        end
      {:error, reason} ->
        IO.puts("  ✗ Failed to list directory: #{inspect(reason)}")
        {:error, {:ls_failed, reason}}
    end
  end
end
