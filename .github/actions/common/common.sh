#!/usr/bin/env bash
# Shared helpers for the pluginval composite actions (sourced, not run).
# Kept compatible with bash 3.2, which is what macOS runners ship.

COMMON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Defaults so the scripts can also be run outside of GitHub Actions.
if [ -z "${RUNNER_OS:-}" ]; then
    if [ "$(uname -s)" = "Darwin" ]; then RUNNER_OS=macOS
    elif [ "$(uname -s)" = "Linux" ]; then RUNNER_OS=Linux
    else RUNNER_OS=Windows
    fi
fi

: "${RUNNER_TEMP:=$(mktemp -d)}"
: "${GITHUB_STEP_SUMMARY:=/dev/null}"
: "${GITHUB_OUTPUT:=/dev/null}"

# Sets PLUGINVAL_BIN to the pluginval binary: INPUT_PLUGINVAL_PATH if given,
# otherwise the INPUT_PLUGINVAL_VERSION release ("latest" or a tag like v2.0.0-beta1)
# downloaded for this runner's OS.
resolve_pluginval()
{
    if [ -n "${INPUT_PLUGINVAL_PATH:-}" ]; then
        PLUGINVAL_BIN="$INPUT_PLUGINVAL_PATH"
    else
        local version="${INPUT_PLUGINVAL_VERSION:-latest}"
        local dir="$RUNNER_TEMP/pluginval-$version"
        local url="https://github.com/Tracktion/pluginval/releases/download/$version/pluginval_$RUNNER_OS.zip"

        if [ "$version" = "latest" ]; then
            url="https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_$RUNNER_OS.zip"
        fi

        case "$RUNNER_OS" in
            macOS)   PLUGINVAL_BIN="$dir/pluginval.app/Contents/MacOS/pluginval" ;;
            Windows) PLUGINVAL_BIN="$dir/pluginval.exe" ;;
            *)       PLUGINVAL_BIN="$dir/pluginval" ;;
        esac

        if [ ! -f "$PLUGINVAL_BIN" ]; then
            echo "Downloading $url"
            mkdir -p "$dir"

            if ! curl -fsSL --retry 3 "$url" -o "$dir/pluginval.zip"; then
                echo "::error::Couldn't download pluginval $version for $RUNNER_OS ($url)"
                exit 1
            fi

            unzip -q -o "$dir/pluginval.zip" -d "$dir"
            chmod +x "$PLUGINVAL_BIN"
        fi
    fi

    if [ ! -f "$PLUGINVAL_BIN" ]; then
        echo "::error::pluginval binary not found: $PLUGINVAL_BIN"
        exit 1
    fi
}

# Runs pluginval with the given arguments. On Linux without a display it is
# wrapped in xvfb-run, as plugins generally need an X server to load.
run_pluginval()
{
    if [ "$RUNNER_OS" = "Linux" ] && [ -z "${DISPLAY:-}" ] && command -v xvfb-run > /dev/null; then
        xvfb-run --auto-servernum "$PLUGINVAL_BIN" "$@"
    else
        "$PLUGINVAL_BIN" "$@"
    fi
}

# Prints a file-name-safe version of the argument.
sanitise()
{
    printf '%s' "$1" | tr -c 'A-Za-z0-9._-' '_'
}

# Reads a JUnit XML report written by pluginval and sets:
#   REPORT_TESTS    a summary-table cell, e.g. "17" or "2 of 17 failed"
#   REPORT_NAMES    "name: message" per failed test, "; " separated (for the annotation)
#   REPORT_DETAILS  a markdown list item per failed test (for the job summary)
# All three are empty if there is no report.
read_report()
{
    REPORT_TESTS=""
    REPORT_NAMES=""
    REPORT_DETAILS=""

    [ -f "$1" ] || return 0

    local kind first second
    while IFS=$'\t' read -r kind first second; do
        if [ "$kind" = "T" ]; then
            if [ "$second" -gt 0 ]; then REPORT_TESTS="$second of $first failed"; else REPORT_TESTS="$first"; fi
        elif [ "$kind" = "F" ]; then
            REPORT_NAMES="${REPORT_NAMES:+$REPORT_NAMES; }$first: $second"
            REPORT_DETAILS="$REPORT_DETAILS  - **${first//|/\\|}**: ${second//|/\\|}"$'\n'
        fi
    done < <(awk -f "$COMMON_DIR/junit.awk" "$1")
}

# Prints an ::error annotation, escaping the message as workflow commands need.
annotate_error()
{
    local message="${2//%/%25}"
    echo "::error title=$1::$message"
}
