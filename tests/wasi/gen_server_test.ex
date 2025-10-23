#
# This file is part of AtomVM.
#
# Copyright 2025 Josh Adams
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
#

defmodule SimpleCounter do
  @moduledoc """
  Simple counter using :gen_server directly (without GenServer behaviour).
  """

  # Client API
  def start_link(initial_value) do
    :gen_server.start_link(__MODULE__, initial_value, [])
  end

  def increment(pid) do
    :gen_server.call(pid, :increment)
  end

  def get_count(pid) do
    :gen_server.call(pid, :get)
  end

  def add(pid, value) do
    :gen_server.cast(pid, {:add, value})
  end

  def stop(pid) do
    :gen_server.stop(pid)
  end

  # Server callbacks (required by gen_server)
  def init(initial_value) do
    IO.puts("SimpleCounter init with: ")
    IO.puts(initial_value)
    {:ok, initial_value}
  end

  def handle_call(:increment, _from, count) do
    new_count = count + 1
    {:reply, new_count, new_count}
  end

  def handle_call(:get, _from, count) do
    {:reply, count, count}
  end

  def handle_cast({:add, value}, count) do
    new_count = count + value
    IO.puts("Cast add: ")
    IO.puts(value)
    {:noreply, new_count}
  end

  def terminate(_reason, count) do
    IO.puts("SimpleCounter terminate at count: ")
    IO.puts(count)
    :ok
  end
end

defmodule Elixir.GenServerTest do
  def start() do
    IO.puts("Testing gen_server behavior...")

    # Start counter at 10
    {:ok, pid} = SimpleCounter.start_link(10)
    IO.puts("Started SimpleCounter")

    # Increment (synchronous call)
    result = SimpleCounter.increment(pid)
    IO.puts("After increment: ")
    IO.puts(result)

    # Another increment
    result2 = SimpleCounter.increment(pid)
    IO.puts("After second increment: ")
    IO.puts(result2)

    # Async add
    SimpleCounter.add(pid, 5)
    :timer.sleep(100)

    # Get final count
    final = SimpleCounter.get_count(pid)
    IO.puts("Final count: ")
    IO.puts(final)

    # Stop the server
    SimpleCounter.stop(pid)

    IO.puts("gen_server test completed!")
    final
  end
end
