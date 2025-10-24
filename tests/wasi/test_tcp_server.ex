defmodule TestTcpServer do
  @moduledoc """
  Simple TCP server test for AtomVM WASI
  Tests basic TCP server functionality (Phase 2)

  This creates a simple echo server that:
  1. Listens on port 8081
  2. Accepts one connection
  3. Receives data
  4. Echoes it back
  5. Closes the connection

  Test with: echo "Hello Server" | nc localhost 8081

  Returns: :ok on success, {:error, reason} on failure
  """

  def start do
    port = 8081

    # Start listening
    with {:ok, listen_socket} <- :gen_tcp.listen(port, [:binary, active: false, reuseaddr: true]),
         # Accept one connection
         {:ok, client_socket} <- :gen_tcp.accept(listen_socket, 10000),
         # Receive data
         {:ok, data} <- :gen_tcp.recv(client_socket, 0, 5000),
         # Echo it back
         :ok <- :gen_tcp.send(client_socket, data),
         # Close client socket
         :ok <- :gen_tcp.close(client_socket),
         # Close listening socket
         :ok <- :gen_tcp.close(listen_socket) do
      :ok
    else
      {:error, reason} -> {:error, reason}
    end
  end
end
