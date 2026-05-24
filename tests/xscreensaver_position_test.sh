#!/bin/sh
set -eu

xscreensaver-command -select 4 >/dev/null
sleep 6

trap 'xscreensaver-command -deactivate >/dev/null 2>&1 || true' EXIT

positions=$(
	xwininfo -root -tree |
		awk '/"Gluqlo 1.1"/ {
			for (i = 1; i <= NF; i++) {
				if ($i ~ /^[0-9]+x[0-9]+[-+][0-9]+[-+][0-9]+$/) {
					print $i
					break
				}
			}
		}' |
		sort
)

expected=$(printf '%s\n%s\n' '2560x1440+0+0' '2560x1440+2560+0' | sort)

if [ "$positions" != "$expected" ]; then
	printf 'unexpected Gluqlo windows:\n%s\nexpected:\n%s\n' "$positions" "$expected" >&2
	exit 1
fi

if command -v import >/dev/null 2>&1 && command -v convert >/dev/null 2>&1; then
	shot=$(mktemp /tmp/gluqlo-xscreensaver-XXXXXX.png)
	cc=$(mktemp /tmp/gluqlo-xscreensaver-cc-XXXXXX.txt)
	import -window root "$shot"
	convert "$shot" -threshold 5% \
		-define connected-components:verbose=true \
		-connected-components 8 null: >"$cc" 2>&1
	large_components=$(
		awk '$2 !~ /^5120x1440/ && $4 >= 300000 { count++ } END { print count + 0 }' "$cc"
	)
	rm -f "$shot" "$cc"
	if [ "$large_components" -lt 8 ]; then
		printf 'expected at least 8 large clock half-components, got %s\n' \
			"$large_components" >&2
		exit 1
	fi
fi
