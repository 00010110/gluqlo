#!/bin/sh
set -eu

targets=$(make -s print-install-targets | sort)
expected=$(printf '%s\n%s\n' \
	'/usr/lib/xscreensaver/gluqlo' \
	'/usr/libexec/xscreensaver/gluqlo' | sort)

if [ "$targets" != "$expected" ]; then
	printf 'unexpected install targets:\n%s\nexpected:\n%s\n' \
		"$targets" "$expected" >&2
	exit 1
fi
