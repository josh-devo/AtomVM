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

defmodule Calculator do
  def start() do
    IO.puts("Hello from WASI Elixir!")

    # Test basic arithmetic
    sum = add(10, 32)
    IO.puts("Add result: ")
    IO.puts(sum)

    # Test multiplication
    product = multiply(6, 7)
    IO.puts("Multiply result: ")
    IO.puts(product)

    # Test factorial
    fact5 = factorial(5)
    IO.puts("Factorial 5: ")
    IO.puts(fact5)

    # Test with spawned process
    pid = spawn(fn -> worker_loop() end)
    send(pid, {self(), :compute, :add, 100, 200})

    receive do
      {:result, result} ->
        IO.puts("Worker result: ")
        IO.puts(result)
    after
      1000 -> IO.puts("timeout")
    end

    :ok
  end

  def add(a, b), do: a + b

  def multiply(a, b), do: a * b

  def factorial(0), do: 1
  def factorial(n) when n > 0, do: n * factorial(n - 1)

  defp worker_loop() do
    receive do
      {from, :compute, :add, a, b} ->
        send(from, {:result, a + b})
        worker_loop()

      {from, :compute, :multiply, a, b} ->
        send(from, {:result, a * b})
        worker_loop()

      :stop ->
        :ok
    end
  end
end
