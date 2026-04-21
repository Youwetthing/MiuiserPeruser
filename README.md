
## IPC Message Format

All communication across the stack — C daemons, Python layer, TCP fallback — uses the same format:

- **Request:** UTF-8 string + `\n` terminator
- **Response:** UTF-8 string + `\n` terminator
- **One command per connection** — connect, send, receive, close
- **No length prefixes, no binary framing**

This applies to:
- turtlecom.sock (UNIX, primary)
- krang.sock (UNIX, internal)
- 127.0.0.1:6789 (TCP, fallback only)

If the UNIX socket is dead, powerhouse.py flips to TCP automatically.
sensei_backend.py is the TCP receiver — it speaks the same protocol.
