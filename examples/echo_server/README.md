# An Echo Server Implemented Using SocketManager

Implementing Echo using SocketManager is a bit tricky to
make the memory model correct.

## The Memory Model Before Echo
Dropping `Sender` will close the `Write` side of the socket connection,
and drop its reference to `Connection`.
`ConnCallback` will drop its internal reference to `Connection` when
the connection is closed.
Thus `Connection` will free any reference to `Notifier`
or `MsgReceiver`.
Note to drop all related resources, no reference to the
returned `Waker` should be kept.

```mermaid
flowchart TD
    M(SocketManager) -->|strong ref| CB(ConnCallback)
    CB -->|strong ref, drop on close| CON(Connection)
    CON -->|strong ref| NF(Notifier)
    CON -->|strong ref| RCV(MsgReceiver)
    SEND(Sender) -->|strong ref| CON
```

## The Memory Model Of Echo

`Sender` is now referenced by `MsgReceiver` for sending
back the message, and cannot be dropped easily,
thus a cycle of reference is created.

New links are marked in red.

```mermaid
flowchart TD
    NF ==>|strong ref| WK(Waker ref=1)
    RCV ==>|strong ref| SEND
    RCV ==>|strong ref| NF
    CB ==> |strong ref| RCV
    M(SocketManager) -->|strong ref| CB(ConnCallback)
    CB -->|strong ref, drop on close| CON(Connection ref=2)
    CON -->|strong ref| NF(Notifier ref=2)
    CON -->|strong ref| RCV(MsgReceiver ref=2)
    SEND(Sender ref=1) -->|strong ref| CON
    WK -.-> TOKIO(tokio task)
    linkStyle 0,1,2,3 color:red;

```

The echo connection can be closed only be the remote,
thus we receive a `on_close` event from `ConnCallback`,
which will drop its reference to `Connection`.
Creating the following:

```mermaid
flowchart TD
    NF ==>|strong ref| WK(Waker ref=1)
    RCV ==>|strong ref| SEND
    RCV ==>|strong ref| NF
    CB(ConnCallback) ==> |strong ref| RCV
    CON -->|strong ref| NF(Notifier ref=2)
    CON -->|strong ref| RCV(MsgReceiver ref=2)
    SEND(Sender ref=1) -->|strong ref| CON(Connection ref=1)
    WK -.-> TOKIO(tokio task)
    linkStyle 0,1,2,3 color:red;

```

Now, using the reference of `MsgReceiver` in `ConnCallback`,
we manually call erase the reference to `Sender` and `Notifer`
in `MsgReceiver`, and then drop the reference of `MsgReceiver`:

```mermaid
flowchart TD
    NF ==>|strong ref| WK(Waker ref=1)
    CON -->|strong ref| NF(Notifier ref=1)
    CON -->|strong ref| RCV(MsgReceiver ref=1)
    SEND(Sender ref=0) -->|strong ref| CON(Connection ref=1)
    WK -.-> TOKIO(tokio task)
    linkStyle 0,1 color:red;
```

Now everything will be cleared up.
