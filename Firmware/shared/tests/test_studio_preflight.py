import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("preflight", Path(__file__).resolve().parents[2] / "tools" / "validate_studio_config.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def empty():
    return {"layers": [{"keys": [""] * 12} for _ in range(4)]}

class PreflightTests(unittest.TestCase):
    def test_old_labels_stay_unresolved(self):
        config = empty(); config["layers"][0]["keys"][0] = "Taşı · G"
        result = module.inspect(config)
        self.assertEqual(result["structured_keyboard_actions"], 0)
        self.assertEqual(len(result["unresolved"]), 1)
        self.assertFalse(result["device_write_supported"])

    def test_keyboard_roundtrip_and_stale_label(self):
        config = empty(); layer = config["layers"][0]
        layer["keys"][4] = "⌘ H"
        layer["keyBehaviors"] = [{} for _ in range(12)]
        layer["keyBehaviors"][4]["primaryKey"] = {"label": "⌘ H", "keyCode": 11, "modifiers": 8}
        self.assertEqual(module.inspect({"config": config})["structured_keyboard_actions"], 1)
        layer["keys"][4] = "H"
        self.assertIn("does not match", module.inspect(config)["unresolved"][0])

    def test_invalid_shape(self):
        with self.assertRaises(ValueError): module.inspect({"layers": []})

if __name__ == "__main__": unittest.main()
