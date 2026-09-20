"""Exercise the real app's fullscreen controls, without a media/driver dependency."""
import ctypes as c
import ctypes.wintypes as w
import json
import os
from pathlib import Path
import subprocess
import sys
import time

exe, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
u = c.WinDLL('user32', use_last_error=True)
visit_type = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetDlgCtrlID.argtypes = [w.HWND]
u.IsWindowVisible.argtypes = [w.HWND]
u.SendMessageTimeoutW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM, w.UINT, w.UINT, c.POINTER(c.c_size_t)]
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
result = {'passed': False, 'hdrDisplayTested': False}
env = os.environ.copy()
env['VEYRA_LOG_FILE'] = str(out / 'app.log')
with (out / 'stdout.txt').open('w') as stdout, (out / 'stderr.txt').open('w') as stderr:
    process = subprocess.Popen([str(exe), '--smoke-empty', '--smoke-view', 'pro', '--smoke-seconds', '30'], env=env, stdout=stdout, stderr=stderr)
    try:
        found = []
        @visit_type
        def visit(hwnd, unused):
            pid = w.DWORD()
            u.GetWindowThreadProcessId(hwnd, c.byref(pid))
            name = c.create_unicode_buffer(128)
            u.GetClassNameW(hwnd, name, 128)
            if pid.value == process.pid and name.value == 'VeyraApp':
                found.append(hwnd)
            return True
        until = time.monotonic() + 10
        while not found and time.monotonic() < until:
            u.EnumWindows(visit, 0)
            time.sleep(.1)
        assert found, 'main window missing'
        main = found[0]
        def send(msg, wp=0, lp=0):
            reply = c.c_size_t()
            assert u.SendMessageTimeoutW(main, msg, wp, lp, 2, 2000, c.byref(reply)), 'UI timeout'
        lock = []
        @visit_type
        def child(hwnd, unused):
            text = c.create_unicode_buffer(256)
            u.GetWindowTextW(hwnd, text, 256)
            if 'Ctrl+L' in text.value:
                lock.append(hwnd)
            return True
        u.EnumChildWindows(main, child, 0)
        assert len(lock) == 1, 'lock control missing'
        button = lock[0]
        assert not u.IsWindowVisible(button), 'lock visible outside fullscreen'
        send(0x100, 0x7A)  # F11
        assert u.IsWindowVisible(button), 'lock not available in fullscreen'
        send(0x111, u.GetDlgCtrlID(button))
        assert not u.IsWindowVisible(button), 'locking did not hide controls'
        # Delivered through the normal message loop so pointerActivity runs.
        for i in range(5):
            u.PostMessageW(main, 0x200, 0, (80+i) | (80 << 16))
            time.sleep(.06)
        send(0)
        assert not u.IsWindowVisible(button), 'mouse motion unlocked controls'
        send(0x111, u.GetDlgCtrlID(button))
        assert u.IsWindowVisible(button), 'unlock did not restore controls'
        send(0x111, u.GetDlgCtrlID(button))
        send(0x100, 0x1B)  # Esc must exit even while locked.
        send(0x100, 0x7A)
        assert u.IsWindowVisible(button), 'fullscreen retained stale lock after exit'
        send(0x100, 0x1B)
        send(0x10)
        assert process.wait(timeout=8) == 0
        result['passed'] = True
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        result['exitCode'] = process.returncode
        (out / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
