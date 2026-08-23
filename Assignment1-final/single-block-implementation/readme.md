Fix1: Client_fd
Not a bug. `client_fd` is declared INSIDE the while loop, so every 
iteration creates a brand new pointer + a brand new malloc'd heap 
block. Different connections never share memory, so nothing gets 
lost or overwritten.

Fix2: Persistent Connection
Producer and Consumer now keep the connection open across multiple 
requests. Typing 'quit' (or Ctrl+D) sends a 'Q' header to the 
server, which breaks out of its loop and closes that connection 
cleanly — instead of one connection per message.

Fix3: Minimizing reads/writes (per single request/response, not 
per whole session)

Producer -> Server:
  Producer: 1 write  (type + length + message, via writev)
  Server:   2 reads  (unavoidable — message length is variable, 
            so must read fixed 5-byte header FIRST to learn the 
            length, then read that many bytes for the message body)
  Server -> Producer: 1 write (1-byte ack: success/full)
  Producer: 1 read (the ack)

Consumer -> Server:
  Consumer: 1 write (fixed 5-byte header: 'C' + zero-padded length)
  Server:   1 read  (fixed 5-byte header only — no body to read 
            for a consumer request)
  Server -> Consumer: 1 write (writev: 5-byte response header + 
            message body together, when a message exists) OR 
            1 write (5-byte "empty" header alone)
  Consumer: 1 read (fixed 5-byte response header) + 1 read (message 
            body, ONLY if header says a message follows — size 
            known only after decoding the header)

