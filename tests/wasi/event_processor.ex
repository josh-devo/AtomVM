defmodule EventProcessor do
  use GenServer
  
  @moduledoc """
  GenServer that processes events from stdin and emits responses to stdout
  Demonstrates real-time bidirectional communication with the host
  """
  
  ## Client API
  
  def start_link(_opts \\ []) do
    GenServer.start_link(__MODULE__, %{}, name: __MODULE__)
  end
  
  def process_event(event) do
    GenServer.call(__MODULE__, {:process, event})
  end
  
  def get_stats() do
    GenServer.call(__MODULE__, :stats)
  end
  
  ## Server Callbacks
  
  @impl true
  def init(_) do
    state = %{
      events_processed: 0,
      total_bytes: 0,
      start_time: :erlang.monotonic_time(:millisecond)
    }
    
    IO.puts(">>> EventProcessor GenServer started <<<")
    {:ok, state}
  end
  
  @impl true
  def handle_call({:process, event}, _from, state) do
    # Process the event
    event_size = byte_size(event)
    new_state = %{
      state | 
      events_processed: state.events_processed + 1,
      total_bytes: state.total_bytes + event_size
    }
    
    # Emit response event immediately
    uptime = :erlang.monotonic_time(:millisecond) - state.start_time
    response = %{
      type: "event_processed",
      event_num: new_state.events_processed,
      bytes: event_size,
      uptime_ms: uptime,
      data: String.trim(event)
    }
    
    emit_event(response)
    
    {:reply, :ok, new_state}
  end
  
  @impl true
  def handle_call(:stats, _from, state) do
    uptime = :erlang.monotonic_time(:millisecond) - state.start_time
    
    stats = %{
      type: "stats",
      events_processed: state.events_processed,
      total_bytes: state.total_bytes,
      uptime_ms: uptime
    }
    
    {:reply, stats, state}
  end
  
  ## Private Functions
  
  defp emit_event(event_map) do
    # Format as JSON-like output
    output = "[EVENT] #{inspect(event_map)}\n"
    IO.write(:stdio, output)
  end
end

defmodule InteractiveDemo do
  @moduledoc """
  Main module that reads events from stdin and sends to GenServer
  """
  
  def start do
    IO.puts("=== INTERACTIVE EVENT PROCESSOR ===")
    IO.puts("Waiting for events on stdin...")
    IO.puts("Send events (one per line), or 'STATS' for statistics, or 'QUIT' to exit")
    IO.puts("===================================\n")
    
    # Start the GenServer
    {:ok, _pid} = EventProcessor.start_link()
    
    # Start reading loop
    read_loop()
  end
  
  defp read_loop() do
    case IO.gets(:stdio, "") do
      :eof ->
        IO.puts("\n>>> Received EOF, shutting down <<<")
        stats = EventProcessor.get_stats()
        EventProcessor.emit_event(stats)
        :ok
        
      {:error, reason} ->
        IO.puts("Error reading: #{inspect(reason)}")
        {:error, reason}
        
      data when is_binary(data) ->
        line = String.trim(data)
        
        cond do
          line == "" ->
            # Skip empty lines
            read_loop()
            
          line == "QUIT" ->
            IO.puts("\n>>> QUIT command received <<<")
            stats = EventProcessor.get_stats()
            EventProcessor.emit_event(stats)
            :ok
            
          line == "STATS" ->
            stats = EventProcessor.get_stats()
            EventProcessor.emit_event(stats)
            read_loop()
            
          true ->
            # Process the event
            EventProcessor.process_event(line)
            read_loop()
        end
    end
  end
  
  # Helper to emit events (for stats)
  defp emit_event(event_map) do
    output = "[EVENT] #{inspect(event_map)}\n"
    IO.write(:stdio, output)
  end
end
