#!/usr/bin/env bash
set -euo pipefail
# Validate thor-probe JSON output against schema
# Usage: tools/validate_json_output.sh [build_dir]
BUILD_DIR="${1:-build}"
SCHEMA="$(cd "$(dirname "$0")/.." && pwd)/schema/thor_probe_output.schema.json"
OUTPUT="$BUILD_DIR/src/thor_probe/thor_probe"

if [ ! -x "$OUTPUT" ]; then
    echo "ERROR: thor_probe binary not found at $OUTPUT"
    exit 1
fi

JSON="$($OUTPUT --json 2>/dev/null)"
echo "$JSON" | python3 -c "
import json, sys
try:
    import jsonschema
except ImportError:
    print('ERROR: jsonschema not installed. Run: pip install jsonschema')
    sys.exit(1)
schema = json.load(open('$SCHEMA'))
data = json.loads(sys.stdin.read())
jsonschema.validate(data, schema)
print('JSON output validates against schema ✓')
"
