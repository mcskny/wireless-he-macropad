"""Exercise the same pure Swift calculations used by the native application."""
from pathlib import Path
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / 'Native.swift'

class SensorLogicTest(unittest.TestCase):
    def test_normalization_compatibility_and_trigger(self):
        source = SOURCE.read_text()
        definitions = source[source.index('struct LightConfig:'):source.index('struct Point: Identifiable')]
        swift = '''import SwiftUI\nimport Foundation\n''' + definitions + r'''
func near(_ value:Double?,_ expected:Double)->Bool { guard let value=value else{return false};return abs(value-expected)<0.00001 }
precondition(near(SensorMath.percent(raw:1900,free:1900,pressed:700,exponent:1),0))
precondition(near(SensorMath.percent(raw:700,free:1900,pressed:700,exponent:1),100))
precondition(near(SensorMath.percent(raw:1300,free:1900,pressed:700,exponent:1),50))
precondition(near(SensorMath.percent(raw:1300,free:1900,pressed:700,exponent:2),25))
precondition(near(SensorMath.percent(raw:1300,free:700,pressed:1900,exponent:1),50))
precondition(SensorMath.percent(raw:100,free:100,pressed:95,exponent:1) == nil)
precondition(SensorMath.percent(raw:100,free:100,pressed:0,exponent:0) == nil)
let original=Config();let json=try JSONEncoder().encode(original)
var renamed=LayerConfig(name:"My racing layer");renamed.presetID="Assetto Corsa"
let renamedCopy=try JSONDecoder().decode(LayerConfig.self,from:JSONEncoder().encode(renamed))
precondition(renamedCopy.taskProfile == "Assetto Corsa")
let legacy=try JSONDecoder().decode(LayerConfig.self,from:Data("{\"name\":\"Assetto Corsa\",\"keys\":[],\"left\":\"\",\"right\":\"\",\"push\":\"\",\"light\":{\"effect\":\"Sabit\",\"color\":\"#A6F0CA\",\"brightness\":60,\"speed\":50}}".utf8))
precondition(legacy.taskProfile == "Assetto Corsa")
var object=try JSONSerialization.jsonObject(with:json) as! [String:Any];object.removeValue(forKey:"responseCurves")
let old=try JSONSerialization.data(withJSONObject:object);let decoded=try JSONDecoder().decode(Config.self,from:old)
precondition(decoded.responseCurves == nil && decoded.calibration.count == 12)
var updated=decoded;updated.responseCurves=Array(repeating:1.0,count:12);updated.responseCurves![0]=1.5
let roundtrip=try JSONDecoder().decode(Config.self,from:JSONEncoder().encode(updated))
precondition(roundtrip.responseCurves?.first == 1.5 && roundtrip.calibration == decoded.calibration)
var h=HallConfig();var normal=LocalTrigger()
precondition(!normal.update(49,h));precondition(normal.update(51,h));precondition(normal.update(47,h));precondition(!normal.update(44,h))
h.mode="Rapid Trigger";h.min=5;h.max=95;h.press=5;h.release=5
var rt=LocalTrigger();precondition(!rt.update(0,h));precondition(rt.update(20,h));precondition(rt.update(50,h));precondition(!rt.update(44,h));precondition(!rt.update(40,h));precondition(rt.update(46,h));precondition(!rt.update(2,h));precondition(rt.update(98,h))
var socd=LocalSOCD();var pair=[true,false];socd.resolve(&pair,previous:[false,false],a:0,b:1);precondition(pair == [true,false])
pair=[true,true];socd.resolve(&pair,previous:[true,false],a:0,b:1);precondition(pair == [false,true])
pair=[true,false];socd.resolve(&pair,previous:[true,true],a:0,b:1);precondition(pair == [true,false])
var tie=LocalSOCD();pair=[true,true];tie.resolve(&pair,previous:[false,false],a:0,b:1);precondition(pair == [false,false])
'''
        with tempfile.TemporaryDirectory() as folder:
            script = Path(folder) / 'test.swift'
            script.write_text(swift)
            subprocess.run(['swift', str(script)], check=True)

if __name__ == '__main__':
    unittest.main()
