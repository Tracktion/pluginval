#!/usr/bin/env bash
# Runs "pluginval test" on every config found (recursively) under
# INPUT_CONFIG_DIR, runs them all, and fails if any failed. See action.yml for
# the inputs.

set -u
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../common/common.sh"

resolve_pluginval

config_dir="${INPUT_CONFIG_DIR:-}"

if [ ! -d "$config_dir" ]; then
    echo "::error::config-dir is not a directory: $config_dir"
    exit 1
fi

report_dir="${INPUT_REPORT_DIR:-$RUNNER_TEMP/pluginval-reports/acceptance}"
mkdir -p "$report_dir"

options=()
[ -n "${INPUT_PLUGIN:-}" ] && options+=(--plugin "$INPUT_PLUGIN")
[ "${INPUT_RECORD_MISSING:-false}" = "true" ] && options+=(--record-missing)
[ -n "${INPUT_TIMEOUT_MS:-}" ] && options+=(--timeout-ms "$INPUT_TIMEOUT_MS")

passed=0
failed=0
index=0
rows=""

# Every *.json is a config, except the <reference>.wav.json manifests that
# pluginval writes next to each reference.
IFS=$'\n'
configs=( $(find "$config_dir" -type f -name '*.json' ! -name '*.wav.json' | sort) )
unset IFS

for config in ${configs[@]+"${configs[@]}"}; do
    index=$((index + 1))
    name="${config#"$config_dir"/}"
    report="$report_dir/$index-$(sanitise "$(basename "$config" .json)").xml"

    echo "::group::pluginval test: $name"

    # pluginval only runs the first entry of an array config, so reject them
    # rather than silently skipping tests.
    if [ "$(tr -d '[:space:]' < "$config" | head -c 1)" = "[" ]; then
        echo "Array configs aren't supported: use one config per file."
        code=1
    else
        run_pluginval test ${options[@]+"${options[@]}"} --junit-xml "$report" "$config"
        code=$?
    fi

    echo "::endgroup::"

    if [ $code -eq 0 ]; then
        passed=$((passed + 1))
        rows="$rows| \`$name\` | :white_check_mark: passed |"$'\n'
    else
        failed=$((failed + 1))
        rows="$rows| \`$name\` | :x: failed |"$'\n'
        echo "::error::pluginval acceptance test failed: $name"
    fi
done

if [ $index -eq 0 ]; then
    echo "::error::No configs (*.json) found under $config_dir"
    exit 1
fi

# Gather the diff WAVs written next to failing references, so they can be
# uploaded along with the reports.
find "$config_dir" -type f -name '*-diff.wav' | while IFS= read -r diff; do
    mkdir -p "$report_dir/diffs"
    cp "$diff" "$report_dir/diffs/$(sanitise "${diff#"$config_dir"/}")"
done

{
    echo "### pluginval acceptance tests"
    echo
    echo "| Config | Result |"
    echo "| --- | --- |"
    printf '%s' "$rows"
} >> "$GITHUB_STEP_SUMMARY"

{
    echo "passed=$passed"
    echo "failed=$failed"
    echo "report-dir=$report_dir"
} >> "$GITHUB_OUTPUT"

echo "pluginval acceptance tests: $passed passed, $failed failed"
[ $failed -eq 0 ]
