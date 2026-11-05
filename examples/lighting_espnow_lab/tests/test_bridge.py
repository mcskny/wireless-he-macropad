import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('send_backup',Path(__file__).resolve().parents[1]/'send_backup.py')
bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)

def backup(light): return {'config':{'layers':[{'light':light} for _ in range(4)]}}

class BridgeTests(unittest.TestCase):
    def test_wave_and_depth(self):
        b=backup({'effect':'Basılan yerden dalga','rainbowWave':True,'depthSensitive':True,'color':'#123456','brightness':60,'speed':50})
        self.assertEqual(bridge.studio_command(b,0,2),'set 2 ripple 18 52 86 60 50 3')
    def test_sweep_and_rainbow(self):
        b=backup({'effect':'Kayan ışık','rainbowSweep':True,'sweepColorStyle':'tur'})
        self.assertTrue(bridge.studio_command(b,0).endswith(' 5'))
    def test_legacy(self):
        b=backup({'effect':'Derinliğe duyarlı','depthSensitive':None})
        self.assertTrue(bridge.studio_command(b,0).endswith(' 2'))
    def test_invalid_color(self):
        with self.assertRaises(ValueError): bridge.studio_command(backup({'color':'#xyz'}),0)
if __name__=='__main__': unittest.main()
