"""Inspect the capture dialog in an owned beta process, without connecting a device."""
import ctypes as c
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from PIL import ImageGrab

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
os.environ['TEMP'] = os.environ['TMP'] = str(out)
u = c.WinDLL('user32')
u.SetProcessDpiAwarenessContext.argtypes = [w.HANDLE]
u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
u.GetDlgItem.argtypes = [w.HWND, c.c_int]
u.GetDlgItem.restype = w.HWND
u.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.SendMessageTimeoutW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM, w.UINT, w.UINT, c.POINTER(c.c_size_t)]
u.ShowWindow.argtypes = [w.HWND, c.c_int]
visit_type = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)

def find(name):
    found = []
    @visit_type
    def visit(hwnd, unused):
        pid, cls = w.DWORD(), c.create_unicode_buffer(128)
        u.GetWindowThreadProcessId(hwnd, c.byref(pid))
        u.GetClassNameW(hwnd, cls, 128)
        if pid.value == process.pid and cls.value == name:
            found.append(hwnd)
        return True
    u.EnumWindows(visit, 0)
    return found[0] if found else None

def send(hwnd, msg, first=0, second=0):
    value = c.c_size_t()
    assert u.SendMessageTimeoutW(hwnd, msg, first, second, 2, 1500, c.byref(value)), 'UI unresponsive'
    return value.value

def wait(name):
    for _ in range(200):
        hwnd = find(name)
        if hwnd:
            return hwnd
        assert process.poll() is None, 'early process exit'
        time.sleep(.05)
    raise AssertionError('missing window: ' + name)

def rect(hwnd):
    value = w.RECT()
    assert u.GetWindowRect(hwnd, c.byref(value))
    return [value.left, value.top, value.right, value.bottom]

with (out/'stdout.log').open('w') as stdout, (out/'stderr.log').open('w') as stderr:
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(exe), '--smoke-empty', '--smoke-seconds', '45', '--no-nr', '--no-sr', '--no-fg'], cwd=out, stdout=stdout, stderr=stderr, startupinfo=startup)
    try:
        main = wait('VeyraApp')
        send(main, 0x111, 110)
        dialog = wait('VeyraCaptureSetup')
        u.ShowWindow(dialog, 4)
        time.sleep(1)
        color, span = u.GetDlgItem(dialog, 10), u.GetDlgItem(dialog, 20)
        assert send(color, 0x146) == 4 and send(span, 0x146) == 3, 'missing color choices'
        a, b, outer = rect(color), rect(span), rect(dialog)
        assert a[2] <= b[0] and outer[0] <= a[0] and b[2] <= outer[2], 'overlapping color controls'
        ImageGrab.grab(window=dialog).save(out/'capture-color.png')
        send(dialog, 0x10)
        send(main, 0x10)
        assert process.wait(timeout=15) == 0
        (out/'result.json').write_text(json.dumps(dict(passed=True, colorRect=a, rangeRect=b, scope='Owned-process dialog geometry and options; no real-card color acceptance.'), indent=2))
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
