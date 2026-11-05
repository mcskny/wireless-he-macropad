"""Check the native shortcut resolver used for S3 profile writes."""
from pathlib import Path
import subprocess
import tempfile
import unittest
import json
import re

SOURCE = Path(__file__).resolve().parents[1] / 'Native.swift'

class HIDTaskMappingTest(unittest.TestCase):
    def test_assetto_bindings_and_explicit_override(self):
        source = SOURCE.read_text()
        start = source.index('final class MacKeyOutput {')
        end = source.index('\nfinal class StudioModel:', start)
        swift = '''import AppKit\nimport ApplicationServices\nstruct KeyboardAssignment { var keyCode:UInt8; var modifiers:UInt8; var label:String }\n''' + source[start:end] + '''
let down=MacKeyOutput.presetKey("Vites küçült",profile:"Assetto Corsa")!
let up=MacKeyOutput.presetKey("Vites büyüt",profile:"Assetto Corsa")!
precondition(down == MacKeyOutput.fromHID(224,0))
precondition(up == MacKeyOutput.fromHID(44,0))
let custom=KeyboardAssignment(keyCode:11,modifiers:2,label:"Vites küçült")
precondition(MacKeyOutput.resolve(custom,"Vites küçült") == MacKeyOutput.fromHID(11,2))
precondition(MacKeyOutput.resolve(custom,"Başka görev") == nil)
'''
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'test.swift'
            path.write_text(swift)
            subprocess.run(['swift', str(path)], check=True)

    def test_every_preset_has_an_output_and_suggested_keys_are_unique(self):
        source = SOURCE.read_text()
        core = source[source.index('final class MacKeyOutput {'):source.index('\nfinal class StudioModel:')]
        entries = [(match.group(1), json.loads(match.group(2))) for match in re.finditer(r'\.init\(id:"([^"]+)"[^\n]*?rows:(\[[^\]]+\]),encoder:', source)]
        self.assertEqual(len(entries), 13)
        statements = []
        for profile, rows in entries:
            self.assertEqual(len(rows), 12)
            for task in rows:
                p, t = json.dumps(profile, ensure_ascii=False), json.dumps(task, ensure_ascii=False)
                statements.append(f'precondition(MacKeyOutput.resolve(nil,{t}) != nil || MacKeyOutput.media[{t}] != nil || MacKeyOutput.presetKey({t},profile:{p}) != nil, "unresolved: " + {p} + " / " + {t})')
        swift = '''import AppKit\nimport ApplicationServices\nstruct KeyboardAssignment { var keyCode:UInt8; var modifiers:UInt8; var label:String }\n''' + core + '\n' + '\n'.join(statements) + '''
for (profile, tasks) in MacKeyOutput.customPresetActions {
    var seen=Set<UInt8>()
    for task in tasks {
        guard let (key, modifiers)=MacKeyOutput.customPresetShortcut(task,profile:profile) else {fatalError("missing suggestion")}
        precondition(modifiers == 7 && seen.insert(key).inserted)
    }
}
precondition(MacKeyOutput.presetKey("Oynat / Duraklat",profile:"DaVinci Resolve") == MacKeyOutput.fromHID(44,0))
'''
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'test.swift'
            path.write_text(swift)
            subprocess.run(['swift', str(path)], check=True)
