"""Exercise flash guards and recovery control flow against a simulated target."""
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('clock_device', ROOT/'tools/device.py')
device = importlib.util.module_from_spec(spec)
spec.loader.exec_module(device)

class Target:
    def __init__(self):
        self.flash = bytearray((ROOT.parent/'cw32l010_original_64k.bin').read_bytes())
        self.part = b'CW32L010F8U6'.ljust(16,b'\0')
        self.security = 0
        self.programmed = self.resumed = self.reset_called = False
        self.corrupt = False
        self.state = device.Target.State.RUNNING
    def get_state(self): return self.state
    def halt(self): pass
    def reset_and_halt(self, **kwargs): pass
    def write32(self, addr, value): pass
    def write_core_register(self, name, value): pass
    def resume(self): self.resumed = True
    def reset(self, **kwargs): self.reset_called = True
    def read_memory_block8(self, addr, length):
        if addr == 0x1007e0: return self.part
        assert addr == 0
        return self.flash[:length]
    def read_memory_block32(self, addr, n): return [0]*n
    def read8(self, addr): return 20
    def read16(self, addr): return 4096
    def read32(self, addr):
        if addr == 0x1007d8: return 65536
        if addr == 0x40022000: return self.security << 5
        return 1
class Session:
    def __init__(self,t): self.target=t
    def __enter__(self): return self
    def __exit__(self,*args): return False
class Programmer:
    def __init__(self,s,**opts):
        self.t=s.target
        assert opts['chip_erase']=='sector'
    def program(self,path,base_address):
        assert base_address==0
        data=Path(path).read_bytes()
        assert len(data)%512==0
        self.t.flash[:len(data)]=data
        self.t.programmed=True
        if self.t.corrupt: self.t.flash[0]^=1

class DeviceTest(unittest.TestCase):
    def run_device(self,t,*args):
        with tempfile.TemporaryDirectory() as d, \
             patch.object(device,'EVIDENCE',Path(d)), \
             patch.object(device.ConnectHelper,'session_with_chosen_probe',return_value=Session(t)), \
             patch.object(device,'FileProgrammer',Programmer), \
             patch.object(device.time,'sleep',lambda _:None), \
             patch.object(sys,'argv',['device.py',*args]), \
             contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
            device.main()
    def test_inspect_resumes_without_writes(self):
        t=Target(); self.run_device(t)
        self.assertFalse(t.programmed); self.assertTrue(t.resumed)
    def test_sleeping_inspect_resumes(self):
        t=Target(); t.state=device.Target.State.SLEEPING; self.run_device(t)
        self.assertTrue(t.resumed); self.assertFalse(t.programmed)
    def test_flash_preserves_entire_tail(self):
        t=Target(); before=bytes(t.flash)
        image=(ROOT/'build/clock/clock.bin').read_bytes()
        self.run_device(t,'--flash')
        self.assertEqual(t.flash,image+before[len(image):])
        self.assertTrue(t.reset_called)
    def test_changed_original_stops_before_write(self):
        t=Target(); t.flash[0]^=1
        with self.assertRaises(RuntimeError): self.run_device(t,'--flash')
        self.assertFalse(t.programmed); self.assertTrue(t.resumed)
    def test_protection_never_unlocked(self):
        t=Target(); t.security=1
        with self.assertRaises(RuntimeError): self.run_device(t,'--flash')
        self.assertFalse(t.programmed)
    def test_verify_failure_leaves_halted(self):
        t=Target(); t.corrupt=True
        with self.assertRaises(RuntimeError): self.run_device(t,'--flash')
        self.assertTrue(t.programmed)
        self.assertFalse(t.reset_called or t.resumed)
    def test_restore_uses_fixed_backup(self):
        t=Target(); t.flash[:512]=b'\xff'*512
        self.run_device(t,'--restore')
        self.assertEqual(t.flash,(ROOT.parent/'cw32l010_original_64k.bin').read_bytes())
        self.assertTrue(t.reset_called)
if __name__=='__main__': unittest.main()
