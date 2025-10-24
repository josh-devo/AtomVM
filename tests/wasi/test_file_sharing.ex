defmodule TestFileSharing do
  @moduledoc """
  File-based IPC test for AtomVM WASI
  Demonstrates using files for inter-process communication

  This simulates two processes communicating via a shared file:
  1. Writer process writes messages to a file
  2. Reader process reads messages from the file
  3. Uses simple lock file mechanism

  Returns: :ok on success, {:error, reason} on failure
  """

  def start do
    shared_file = "shared_data.txt"
    lock_file = "shared_data.lock"

    with :ok <- test_writer_reader(shared_file, lock_file) do
      IO.puts("File sharing test passed!")
      cleanup_files([shared_file, lock_file])
      :ok
    else
      {:error, reason} = error ->
        IO.puts("File sharing test failed: #{inspect(reason)}")
        cleanup_files([shared_file, lock_file])
        error
    end
  end

  defp test_writer_reader(data_file, lock_file) do
    IO.puts("[TEST] File-based IPC simulation")

    # Clean up any leftover files
    cleanup_files([data_file, lock_file])

    # Spawn writer process
    parent = self()
    writer_pid = spawn(fn -> writer_process(data_file, lock_file, parent) end)

    # Give writer time to write
    receive do
      :writer_done ->
        IO.puts("  ✓ Writer process completed")
    after
      5000 ->
        IO.puts("  ✗ Writer process timed out")
        {:error, :writer_timeout}
    end

    # Now read the data
    case reader_process(data_file, lock_file) do
      {:ok, messages} ->
        IO.puts("  ✓ Reader process completed")
        IO.puts("  Messages read: #{inspect(messages)}")

        if length(messages) == 3 do
          :ok
        else
          IO.puts("  ✗ Expected 3 messages, got #{length(messages)}")
          {:error, :wrong_message_count}
        end
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp writer_process(data_file, lock_file, parent) do
    messages = [
      "Message 1: Hello from writer",
      "Message 2: File-based IPC works",
      "Message 3: Goodbye from writer"
    ]

    IO.puts("  [WRITER] Starting...")

    result = Enum.reduce_while(messages, :ok, fn msg, _acc ->
      case write_with_lock(data_file, lock_file, msg) do
        :ok ->
          IO.puts("  [WRITER] Wrote: #{msg}")
          # Small delay between messages
          :timer.sleep(100)
          {:cont, :ok}
        {:error, reason} ->
          IO.puts("  [WRITER] Failed to write: #{inspect(reason)}")
          {:halt, {:error, reason}}
      end
    end)

    case result do
      :ok ->
        send(parent, :writer_done)
        IO.puts("  [WRITER] Finished successfully")
      {:error, _} = error ->
        send(parent, {:writer_error, error})
    end
  end

  defp reader_process(data_file, lock_file) do
    IO.puts("  [READER] Starting...")

    # Wait for file to exist
    wait_for_file(data_file, 50, 5000)

    case read_with_lock(data_file, lock_file) do
      {:ok, content} ->
        messages = String.split(content, "\n", trim: true)
        IO.puts("  [READER] Read #{length(messages)} messages")
        {:ok, messages}
      {:error, reason} ->
        IO.puts("  [READER] Failed to read: #{inspect(reason)}")
        {:error, reason}
    end
  end

  defp write_with_lock(data_file, lock_file, message) do
    # Simple lock: create lock file
    case acquire_lock(lock_file) do
      :ok ->
        # Append message to file
        result = case File.open(data_file, [:append]) do
          {:ok, file} ->
            IO.write(file, message <> "\n")
            File.close(file)
            :ok
          {:error, reason} ->
            {:error, reason}
        end

        # Release lock
        release_lock(lock_file)
        result
      error ->
        error
    end
  end

  defp read_with_lock(data_file, lock_file) do
    case acquire_lock(lock_file) do
      :ok ->
        result = File.read(data_file)
        release_lock(lock_file)
        result
      error ->
        error
    end
  end

  defp acquire_lock(lock_file, retries \\ 10) do
    if File.exists?(lock_file) do
      if retries > 0 do
        :timer.sleep(50)
        acquire_lock(lock_file, retries - 1)
      else
        {:error, :lock_timeout}
      end
    else
      # Create lock file
      File.write(lock_file, "locked")
    end
  end

  defp release_lock(lock_file) do
    File.rm(lock_file)
    :ok
  end

  defp wait_for_file(_file, _interval, timeout) when timeout <= 0 do
    {:error, :timeout}
  end

  defp wait_for_file(file, interval, timeout) do
    if File.exists?(file) do
      :ok
    else
      :timer.sleep(interval)
      wait_for_file(file, interval, timeout - interval)
    end
  end

  defp cleanup_files(files) do
    Enum.each(files, fn file ->
      if File.exists?(file) do
        File.rm(file)
      end
    end)
  end
end
