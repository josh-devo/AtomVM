defmodule WasiTcpTest do
  use ExUnit.Case

  @moduletag :wasi_tcp

  setup do
    # Start echo server
    port = 8080
    {:ok, server} = :gen_tcp.listen(port, [:binary, {:active, false}, {:reuseaddr, true}])

    # Spawn acceptor
    acceptor = spawn(fn -> accept_loop(server) end)

    on_exit(fn ->
      Process.exit(acceptor, :kill)
      :gen_tcp.close(server)
    end)

    {:ok, %{port: port}}
  end

  test "TCP client connects, sends, receives", %{port: port} do
    wasm_path = Path.expand("../../../build-wasi/src/platforms/wasi/AtomVM.wasm", __DIR__)
    test_beam = Path.expand("test_tcp_socket.beam", __DIR__)

    assert File.exists?(wasm_path), "AtomVM.wasm not found at #{wasm_path}"
    assert File.exists?(test_beam), "test_tcp_socket.beam not found"

    wasm_bytes = File.read!(wasm_path)

    {:ok, instance} = Wasmex.start_link(%{
      bytes: wasm_bytes,
      wasi: %Wasmex.Wasi.WasiP2Options{
        allow_tcp: true,
        allow_sockets: true,
        inherit_network: true,
        preopen_dirs: [{Path.dirname(test_beam), "."}]
      }
    })

    # Call AtomVM main with test module
    {:ok, [result]} = Wasmex.call_function(instance, "main", [
      Wasmex.from_string("test_tcp_socket.beam")
    ])

    # Should return 0 for success
    assert result == 0, "Test failed with exit code #{result}"
  end

  defp accept_loop(server) do
    case :gen_tcp.accept(server, 1000) do
      {:ok, client} ->
        spawn(fn -> handle_client(client) end)
        accept_loop(server)

      {:error, :timeout} ->
        accept_loop(server)

      {:error, reason} ->
        IO.puts("Server accept failed: #{inspect(reason)}")
    end
  end

  defp handle_client(socket) do
    case :gen_tcp.recv(socket, 0, 5000) do
      {:ok, data} ->
        :gen_tcp.send(socket, data)
        :gen_tcp.close(socket)

      {:error, reason} ->
        IO.puts("Server recv failed: #{inspect(reason)}")
        :gen_tcp.close(socket)
    end
  end
end
