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

defmodule JsonSchemaValidator do
  @moduledoc """
  JSON Schema validator for AtomVM WASI applications.

  This module validates Elixir data structures (maps, lists, binaries, etc.) against
  JSON Schema definitions. It is designed for use in WASI-hosted AtomVM applications
  where JSON decoding/encoding is handled by host functions.

  ## Host Integration Guide

  When embedding this validator in a WASI host application, the typical flow is:

  1. **Host receives JSON data** (e.g., from HTTP request, file, WebSocket)
  2. **Host decodes JSON** to Elixir-compatible data structures using a host function
  3. **AtomVM calls JsonSchemaValidator.validate/2** with the decoded data and schema
  4. **Validator returns** `{:ok, data}` or `{:error, errors}`
  5. **Host handles result** (accept valid data, reject invalid data)

  ### Data Structure Mapping

  The host's JSON decoder must map JSON types to Elixir types as follows:

  | JSON Type | Elixir Type | Example |
  |-----------|-------------|---------|
  | `string`  | `binary()`  | `"hello"` |
  | `number`  | `integer() \\| float()` | `42`, `3.14` |
  | `boolean` | `true \\| false` | `true` |
  | `null`    | `nil` | `nil` |
  | `array`   | `list()` | `[1, 2, 3]` |
  | `object`  | `map()` with string keys | `%{"name" => "Alice"}` |

  **Important**: Object keys must be strings, not atoms. This validator expects
  `%{"key" => value}`, not `%{key: value}`.

  ### Example Host Function (Rust/C/Zig)

  Your WASI host should provide a function like:

  ```rust
  // Pseudo-code for a host function
  fn validate_json(json_string: &str, schema: &str) -> Result<(), Vec<ValidationError>> {
      // 1. Decode JSON string to Elixir term (via AtomVM NIF/port)
      let data = json_decode(json_string)?;
      let schema_term = json_decode(schema)?;

      // 2. Call into AtomVM: JsonSchemaValidator.validate(data, schema_term)
      let result = call_elixir_function("JsonSchemaValidator", "validate", [data, schema_term]);

      // 3. Handle result
      match result {
          {:ok, _} => Ok(()),
          {:error, errors} => Err(convert_errors(errors))
      }
  }
  ```

  ## Supported JSON Schema Features

  This validator implements a subset of JSON Schema (draft-07 compatible):

  ### Type Validation
  - `"type": "string"` - validates binaries
  - `"type": "number"` - validates integers and floats
  - `"type": "integer"` - validates integers only
  - `"type": "boolean"` - validates true/false
  - `"type": "array"` - validates lists
  - `"type": "object"` - validates maps
  - `"type": "null"` - validates nil

  ### String Constraints
  - `"minLength"` - minimum string byte length
  - `"maxLength"` - maximum string byte length

  ### Number Constraints
  - `"minimum"` - minimum value (inclusive)
  - `"maximum"` - maximum value (inclusive)

  ### Array Constraints
  - `"minItems"` - minimum array length
  - `"maxItems"` - maximum array length
  - `"items"` - schema for array elements (validates all items)

  ### Object Constraints
  - `"required"` - list of required property names
  - `"properties"` - schemas for named properties
  - `"additionalProperties"` - set to `false` to disallow extra properties

  ## Usage Examples

  ### Basic Type Validation

  ```elixir
  # Validate a string
  schema = %{"type" => "string", "minLength" => 1, "maxLength" => 100}
  JsonSchemaValidator.validate("hello", schema)
  # => {:ok, "hello"}

  JsonSchemaValidator.validate("", schema)
  # => {:error, [{[], "min length 1, got 0"}]}

  # Validate a number
  schema = %{"type" => "integer", "minimum" => 0, "maximum" => 150}
  JsonSchemaValidator.validate(25, schema)
  # => {:ok, 25}

  JsonSchemaValidator.validate(200, schema)
  # => {:error, [{[], "maximum 150, got 200"}]}
  ```

  ### Object Validation

  ```elixir
  user_schema = %{
    "type" => "object",
    "required" => ["name", "email"],
    "properties" => %{
      "name" => %{"type" => "string", "minLength" => 1},
      "email" => %{"type" => "string"},
      "age" => %{"type" => "integer", "minimum" => 0}
    },
    "additionalProperties" => false
  }

  # Valid user
  user = %{"name" => "Alice", "email" => "alice@example.com", "age" => 30}
  JsonSchemaValidator.validate(user, user_schema)
  # => {:ok, %{"name" => "Alice", ...}}

  # Missing required field
  user = %{"name" => "Bob"}
  JsonSchemaValidator.validate(user, user_schema)
  # => {:error, [{[], "missing required: email"}]}

  # Extra field not allowed
  user = %{"name" => "Charlie", "email" => "c@ex.com", "extra" => "field"}
  JsonSchemaValidator.validate(user, user_schema)
  # => {:error, [{[], "additional property not allowed: extra"}]}
  ```

  ### Array Validation

  ```elixir
  numbers_schema = %{
    "type" => "array",
    "minItems" => 1,
    "maxItems" => 10,
    "items" => %{"type" => "integer", "minimum" => 0}
  }

  JsonSchemaValidator.validate([1, 2, 3], numbers_schema)
  # => {:ok, [1, 2, 3]}

  JsonSchemaValidator.validate([1, -5, 3], numbers_schema)
  # => {:error, [{["[1]"], "minimum 0, got -5"}]}
  ```

  ### Nested Object Validation

  ```elixir
  person_schema = %{
    "type" => "object",
    "required" => ["name", "address"],
    "properties" => %{
      "name" => %{"type" => "string"},
      "address" => %{
        "type" => "object",
        "required" => ["street", "city"],
        "properties" => %{
          "street" => %{"type" => "string"},
          "city" => %{"type" => "string"},
          "zipcode" => %{"type" => "string"}
        }
      }
    }
  }

  person = %{
    "name" => "Alice",
    "address" => %{
      "street" => "123 Main St",
      "city" => "Springfield"
    }
  }

  JsonSchemaValidator.validate(person, person_schema)
  # => {:ok, %{"name" => "Alice", ...}}

  # Missing nested required field
  person = %{"name" => "Bob", "address" => %{"street" => "456 Oak"}}
  JsonSchemaValidator.validate(person, person_schema)
  # => {:error, [{[".address"], "missing required: city"}]}
  ```

  ## Error Format

  Validation errors are returned as `{:error, errors}` where `errors` is a list of
  `{path, message}` tuples:

  - `path` - List of path segments indicating where the error occurred
    - `[]` - Root level error
    - `[".property"]` - Object property error
    - `["[0]"]` - Array element error
    - `[".address", ".city"]` - Nested property error
  - `message` - Human-readable error description

  Example:
  ```elixir
  {:error, [
    {[".address", ".zipcode"], "expected string"},
    {[".age"], "maximum 150, got 200"}
  ]}
  ```

  ## Good Test Case Patterns

  When testing this validator in your application, consider these categories:

  ### 1. Happy Path Tests
  - Valid data matching schema exactly
  - Valid data with optional fields
  - Valid nested structures
  - Valid arrays with all elements passing

  ### 2. Type Mismatch Tests
  - Wrong primitive type (string when number expected)
  - Wrong complex type (array when object expected)
  - Null when non-null expected

  ### 3. Constraint Violation Tests
  - Strings too short/long
  - Numbers too small/large
  - Arrays too few/many items
  - Missing required object properties
  - Extra object properties when `additionalProperties: false`

  ### 4. Nested Validation Tests
  - Nested objects with invalid deep properties
  - Arrays of objects with some invalid elements
  - Multiple simultaneous errors at different paths

  ### 5. Edge Cases
  - Empty strings with minLength
  - Zero values with minimum constraints
  - Empty arrays with minItems
  - Empty objects with required fields
  - Single-element arrays
  - Deeply nested structures (3+ levels)

  ### 6. Schema Edge Cases
  - Schema with no constraints (type only)
  - Schema with all constraints
  - Schema with optional properties
  - Schema allowing additional properties

  Example comprehensive test:
  ```elixir
  def test_comprehensive_validation do
    schema = %{
      "type" => "object",
      "required" => ["id", "items"],
      "properties" => %{
        "id" => %{"type" => "integer", "minimum" => 1},
        "items" => %{
          "type" => "array",
          "minItems" => 1,
          "items" => %{
            "type" => "object",
            "required" => ["name", "price"],
            "properties" => %{
              "name" => %{"type" => "string", "minLength" => 1},
              "price" => %{"type" => "number", "minimum" => 0}
            }
          }
        }
      }
    }

    # Test valid complex structure
    data = %{
      "id" => 42,
      "items" => [
        %{"name" => "Widget", "price" => 9.99},
        %{"name" => "Gadget", "price" => 15.50}
      ]
    }
    assert {:ok, ^data} = JsonSchemaValidator.validate(data, schema)

    # Test invalid nested item
    data = %{
      "id" => 42,
      "items" => [
        %{"name" => "Widget", "price" => 9.99},
        %{"name" => "", "price" => -5}  # Invalid: empty name, negative price
      ]
    }
    assert {:error, errors} = JsonSchemaValidator.validate(data, schema)
    assert length(errors) == 2
  end
  ```

  ## Limitations

  This validator does NOT currently support:
  - `$ref` (schema references)
  - `anyOf`, `oneOf`, `allOf` combinators
  - `pattern` (regex matching for strings)
  - `format` (email, uri, date-time, etc.)
  - `enum` (enumerated values)
  - `const` (constant values)
  - `dependencies` (property dependencies)
  - `patternProperties` (pattern-based property matching)

  For full JSON Schema support, consider using a complete validator library when
  available for AtomVM, or implement additional features as needed.
  """

  @doc """
  Validates data against a JSON schema.
  Returns {:ok, data} if valid, {:error, errors} if invalid.
  """
  def validate(data, schema) do
    case validate_value(data, schema, []) do
      [] -> {:ok, data}
      errors -> {:error, errors}
    end
  end

  ## Private validation functions

  defp validate_value(value, %{"type" => type} = schema, path) do
    case validate_type(value, type, path) do
      [] -> validate_constraints(value, schema, path)
      errors -> errors
    end
  end

  defp validate_value(_value, _schema, path) do
    [{path, "schema must specify a type"}]
  end

  # Type validation
  defp validate_type(value, "string", _path) when is_binary(value), do: []
  defp validate_type(_value, "string", path), do: [{path, "expected string"}]

  defp validate_type(value, "number", _path) when is_number(value), do: []
  defp validate_type(_value, "number", path), do: [{path, "expected number"}]

  defp validate_type(value, "integer", _path) when is_integer(value), do: []
  defp validate_type(_value, "integer", path), do: [{path, "expected integer"}]

  defp validate_type(value, "boolean", _path) when is_boolean(value), do: []
  defp validate_type(_value, "boolean", path), do: [{path, "expected boolean"}]

  defp validate_type(value, "array", _path) when is_list(value), do: []
  defp validate_type(_value, "array", path), do: [{path, "expected array"}]

  defp validate_type(value, "object", _path) when is_map(value), do: []
  defp validate_type(_value, "object", path), do: [{path, "expected object"}]

  defp validate_type(nil, "null", _path), do: []
  defp validate_type(_value, "null", path), do: [{path, "expected null"}]

  defp validate_type(_value, unknown, path), do: [{path, "unknown type: #{unknown}"}]

  # Helper to validate array items recursively (Enum.with_index not available)
  defp validate_array_items([], _schema, _path, _index, errors), do: errors

  defp validate_array_items([item | rest], schema, path, index, errors) do
    item_path = path ++ ["[#{index}]"]
    item_errors = validate_value(item, schema, item_path)
    validate_array_items(rest, schema, path, index + 1, item_errors ++ errors)
  end

  # Constraint validation for strings
  defp validate_constraints(value, schema, path) when is_binary(value) do
    errors = []

    errors =
      case Map.get(schema, "minLength") do
        nil -> errors
        min_len ->
          len = byte_size(value)
          if len >= min_len, do: errors, else: [{path, "min length #{min_len}, got #{len}"} | errors]
      end

    errors =
      case Map.get(schema, "maxLength") do
        nil -> errors
        max_len ->
          len = byte_size(value)
          if len <= max_len, do: errors, else: [{path, "max length #{max_len}, got #{len}"} | errors]
      end

    errors
  end

  # Constraint validation for numbers
  defp validate_constraints(value, schema, path) when is_number(value) do
    errors = []

    errors =
      case Map.get(schema, "minimum") do
        nil -> errors
        min -> if value >= min, do: errors, else: [{path, "minimum #{min}, got #{value}"} | errors]
      end

    errors =
      case Map.get(schema, "maximum") do
        nil -> errors
        max -> if value <= max, do: errors, else: [{path, "maximum #{max}, got #{value}"} | errors]
      end

    errors
  end

  # Constraint validation for arrays
  defp validate_constraints(value, schema, path) when is_list(value) do
    len = length(value)
    errors = []

    errors =
      case Map.get(schema, "minItems") do
        nil -> errors
        min -> if len >= min, do: errors, else: [{path, "min items #{min}, got #{len}"} | errors]
      end

    errors =
      case Map.get(schema, "maxItems") do
        nil -> errors
        max -> if len <= max, do: errors, else: [{path, "max items #{max}, got #{len}"} | errors]
      end

    # Validate items if schema provided
    errors =
      case Map.get(schema, "items") do
        nil ->
          errors

        item_schema ->
          validate_array_items(value, item_schema, path, 0, errors)
      end

    errors
  end

  # Constraint validation for objects
  defp validate_constraints(value, schema, path) when is_map(value) do
    errors = []

    # Check required properties
    errors =
      case Map.get(schema, "required") do
        nil ->
          errors

        required when is_list(required) ->
          missing = required -- Map.keys(value)

          if missing == [] do
            errors
          else
            missing_str = Enum.join(missing, ", ")
            [{path, "missing required: #{missing_str}"} | errors]
          end

        _ ->
          errors
      end

    # Validate properties
    errors =
      case Map.get(schema, "properties") do
        nil ->
          errors

        properties when is_map(properties) ->
          Map.keys(value)
          |> Enum.reduce(errors, fn key, acc ->
            case Map.get(properties, key) do
              nil ->
                # Check if additional properties are allowed
                if Map.get(schema, "additionalProperties") == false do
                  [{path, "additional property not allowed: #{key}"} | acc]
                else
                  acc
                end

              prop_schema ->
                prop_path = path ++ [".#{key}"]
                validate_value(Map.get(value, key), prop_schema, prop_path) ++ acc
            end
          end)
      end

    errors
  end

  # No constraints for other types
  defp validate_constraints(_value, _schema, _path), do: []
end

defmodule Elixir.JsonValidator do
  @moduledoc """
  Test harness for JsonValidator.
  """

  def start do
    IO.puts("Testing JSON Schema Validator...")

    # Test 1: Valid user object
    user_data = %{
      "name" => "Alice",
      "age" => 30,
      "email" => "alice@example.com"
    }

    user_schema = %{
      "type" => "object",
      "required" => ["name", "age"],
      "properties" => %{
        "name" => %{"type" => "string", "minLength" => 1},
        "age" => %{"type" => "integer", "minimum" => 0, "maximum" => 150},
        "email" => %{"type" => "string"}
      }
    }

    case JsonSchemaValidator.validate(user_data, user_schema) do
      {:ok, _} -> IO.puts("Test 1: User validation PASSED")
      {:error, errors} -> IO.puts("Test 1 FAILED: #{inspect(errors)}")
    end

    # Test 2: Valid array of numbers
    numbers = [1, 2, 3, 4, 5]

    numbers_schema = %{
      "type" => "array",
      "minItems" => 1,
      "maxItems" => 10,
      "items" => %{"type" => "integer", "minimum" => 0}
    }

    case JsonSchemaValidator.validate(numbers, numbers_schema) do
      {:ok, _} -> IO.puts("Test 2: Array validation PASSED")
      {:error, errors} -> IO.puts("Test 2 FAILED: #{inspect(errors)}")
    end

    # Test 3: Invalid user (age too high)
    invalid_user = %{
      "name" => "Bob",
      "age" => 200
    }

    case JsonSchemaValidator.validate(invalid_user, user_schema) do
      {:ok, _} -> IO.puts("Test 3 FAILED: Should have rejected invalid age")
      {:error, _} -> IO.puts("Test 3: Rejection of invalid data PASSED")
    end

    # Test 4: Nested object validation
    person_with_address = %{
      "name" => "Charlie",
      "age" => 25,
      "address" => %{
        "street" => "123 Main St",
        "city" => "Springfield",
        "zipcode" => "12345"
      }
    }

    person_schema = %{
      "type" => "object",
      "required" => ["name", "address"],
      "properties" => %{
        "name" => %{"type" => "string"},
        "age" => %{"type" => "integer"},
        "address" => %{
          "type" => "object",
          "required" => ["street", "city"],
          "properties" => %{
            "street" => %{"type" => "string"},
            "city" => %{"type" => "string"},
            "zipcode" => %{"type" => "string"}
          }
        }
      }
    }

    case JsonSchemaValidator.validate(person_with_address, person_schema) do
      {:ok, _} -> IO.puts("Test 4: Nested validation PASSED")
      {:error, errors} -> IO.puts("Test 4 FAILED: #{inspect(errors)}")
    end

    IO.puts("JSON Schema Validator tests completed!")
    :ok
  end
end
