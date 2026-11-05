#!/usr/bin/env python3
"""Read-only preflight for future device configuration; never opens a port."""
import argparse
import json
import sys

KEY_COUNT = 12
LAYER_COUNT = 4

def inspect(backup):
    if not isinstance(backup, dict):
        raise ValueError("Expected a JSON object")
    config = backup.get("config", backup)
    if not isinstance(config, dict) or not isinstance(config.get("layers"), list) or len(config["layers"]) != LAYER_COUNT:
        raise ValueError("Expected exactly four layers")
    issues = []
    accepted = 0
    for layer_no, layer in enumerate(config["layers"], 1):
        if not isinstance(layer, dict) or not isinstance(layer.get("keys"), list) or len(layer["keys"]) != KEY_COUNT:
            raise ValueError(f"Layer {layer_no}: expected 12 key descriptions")
        behaviors = layer.get("keyBehaviors")
        if behaviors is None:
            behaviors = [{} for _ in range(KEY_COUNT)]
        if not isinstance(behaviors, list) or len(behaviors) != KEY_COUNT:
            raise ValueError(f"Layer {layer_no}: invalid key behavior array")
        for i, (label, behavior) in enumerate(zip(layer["keys"], behaviors)):
            if not isinstance(label, str) or not isinstance(behavior, dict):
                raise ValueError(f"Layer {layer_no}, KEY {i}: invalid entry")
            for field, description in (("primaryKey", label), ("secondaryKey", behavior.get("secondAction", ""))):
                if not isinstance(description, str):
                    raise ValueError(f"Layer {layer_no}, KEY {i}: invalid description")
                assignment = behavior.get(field)
                if not description and assignment is None:
                    continue
                where = f"Layer {layer_no}, KEY {i}, {field}"
                if assignment is None:
                    issues.append(f"{where}: description has no structured HID assignment")
                    continue
                if not isinstance(assignment, dict) or assignment.get("label") != description:
                    issues.append(f"{where}: assignment does not match visible description")
                    continue
                code, modifiers = assignment.get("keyCode"), assignment.get("modifiers")
                if type(code) is not int or type(modifiers) is not int or not (4 <= code <= 0xA4 or 0xE0 <= code <= 0xE7) or not 0 <= modifiers <= 255:
                    issues.append(f"{where}: unsupported HID key or modifiers")
                    continue
                accepted += 1
    return {"schema": 1, "structured_keyboard_actions": accepted, "unresolved": issues,
            "device_write_supported": False,
            "note": "Read-only preflight; this is not a complete device configuration."}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("backup", help="Studio backup JSON or config JSON")
    args = parser.parse_args()
    try:
        with open(args.backup, encoding="utf-8") as f:
            result = inspect(json.load(f))
    except (ValueError, OSError, json.JSONDecodeError) as e:
        print(f"Invalid input: {e}", file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 1 if result["unresolved"] else 0

if __name__ == "__main__":
    raise SystemExit(main())
