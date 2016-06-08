#!/bin/sh

post()
{
    echo "--> $1"
    echo -n "<-- "
    curl -d "$1" -s "localhost:8080/rpc"
    echo ""
}

# Check if curl is available
if ! which curl > /dev/null; then
    echo "ERROR; Curl not found! Please install Curl before launch."
    exit 1
fi

# Do tests

# rpc call with positional parameters:
post '{"jsonrpc": "2.0", "method": "subtract", "params": [42, 23], "id": 1}'
post '{"jsonrpc": "2.0", "method": "subtract", "params": [23, 42], "id": 2}'

# rpc call with named parameters:
post '{"jsonrpc": "2.0", "method": "subtract", "params": {"subtrahend": 23, "minuend": 42}, "id": 3}'
post '{"jsonrpc": "2.0", "method": "subtract", "params": {"minuend": 42, "subtrahend": 23}, "id": 4}'

# a Notification:
post '{"jsonrpc": "2.0", "method": "update", "params": [1,2,3,4,5]}'
post '{"jsonrpc": "2.0", "method": "foobar"}'

# rpc call of non-existent method:
post '{"jsonrpc": "2.0", "method": "foobar", "id": "1"}'

# rpc call with invalid JSON:
post '{"jsonrpc": "2.0", "method": "foobar, "params": "bar", "baz]'

# rpc call with invalid Request object:
post '{"jsonrpc": "2.0", "method": 1, "params": "bar"}'

# rpc call with invalid Params object:
post '{"jsonrpc": "2.0", "method": "foobar", "params": "bar", "id": "1"}'
post '{"jsonrpc": "2.0", "method": "foobar", "params": "bar"}'

# rpc call Batch, invalid JSON:
post '[
  {"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"},
  {"jsonrpc": "2.0", "method"
]'

# rpc call with an empty Array:
post '[]'

# rpc call with an invalid Batch (but not empty):
post '[1]'

# rpc call with invalid Batch:
post '[1,2,3]'

# rpc call Batch:
post '[
        {"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"},
        {"jsonrpc": "2.0", "method": "notify_hello", "params": [7]},
        {"jsonrpc": "2.0", "method": "subtract", "params": [42,23], "id": "2"},
        {"foo": "boo"},
        {"jsonrpc": "2.0", "method": "foo.get", "params": {"name": "myself"}, "id": "5"},
        {"jsonrpc": "2.0", "method": "get_data", "id": "9"}
    ]'



