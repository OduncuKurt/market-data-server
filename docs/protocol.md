# Market Data Server Protocol

The server uses a simple line-based TCP protocol.

Each client command must end with a newline character:

```text
\n
```

## Supported Commands

### PING

Checks whether the server is responsive.

Request:

```text
PING
```

Response:

```text
PONG
```

### SYMBOLS

Returns the supported market symbols.

Request:

```text
SYMBOLS
```

Response:

```text
SYMBOLS THYAO,ASELS,GARAN,AKBNK,SISE
```



### PRICE

Updates and returns the current simulated price of a market symbol.

Request:

```text
PRICE THYAO
```

Response:

```text
PRICE THYAO 312.74
```

Possible errors:

```text
ERROR SYMBOL_REQUIRED
ERROR UNKNOWN_SYMBOL
```
### SUBSCRIBE

Subscribes the client to a market symbol.

Request:

```text
SUBSCRIBE THYAO
```

Successful response:

```text
OK SUBSCRIBED THYAO
```

Possible errors:

```text
ERROR SYMBOL_REQUIRED
ERROR UNKNOWN_SYMBOL
ERROR ALREADY_SUBSCRIBED
```

### UNSUBSCRIBE

Removes an existing subscription.

Request:

```text
UNSUBSCRIBE THYAO
```

Successful response:

```text
OK UNSUBSCRIBED THYAO
```

Possible errors:

```text
ERROR SYMBOL_REQUIRED
ERROR NOT_SUBSCRIBED
```

### LIST

Lists the client's active subscriptions.

Request:

```text
LIST
```

Response:

```text
SUBSCRIPTIONS THYAO,ASELS
```

When no active subscription exists:

```text
SUBSCRIPTIONS EMPTY
```

### QUIT

Closes the client connection gracefully.

Request:

```text
QUIT
```

Response:

```text
BYE
```
