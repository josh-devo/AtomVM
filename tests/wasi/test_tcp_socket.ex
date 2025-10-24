defmodule TestTcpSocket do
  @moduledoc """
  Simple TCP socket test for AtomVM WASI
  Tests basic TCP client functionality (Phase 1)
  Requires echo server running on 127.0.0.1:8080
  Start server with: socat TCP-LISTEN:8080,reuseaddr,fork EXEC:cat

  Returns: :ok on success, {:error, reason} on failure
  """

  def start do
    host = ~c"127.0.0.1"
    port = 8080

    with {:ok, socket} <- :gen_tcp.connect(host, port, [:binary, active: false], 5000),
         message = "Hello from AtomVM WASI (Elixir)",
         :ok <- :gen_tcp.send(socket, message),
         {:ok, data} <- :gen_tcp.recv(socket, 0, 5000),
         :ok <- :gen_tcp.close(socket) do
      if data == message do
        :ok
      else
        {:error, :echo_mismatch}
      end
    else
      {:error, reason} -> {:error, reason}
    end
  end
end
