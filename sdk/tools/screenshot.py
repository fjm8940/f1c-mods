"""
screenshot.py — capture ONLY the F1 Challenge window, not the full desktop.

Uses PrintWindow with PW_RENDERFULLCONTENT, which pulls from DWM's composited
surface — works for DirectX-rendered windows (plain BitBlt returns black for
those). Game runs WINDOWEDMODE=1 (see Config.ini), so this works without
touching focus or input at all — pure GDI capture, no SendInput, no
window-activation, safe to call anytime.

USAGE
-----
  python tools/screenshot.py                  # saves tools/shot.png
  python tools/screenshot.py out.png
"""
import ctypes
import sys
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

PW_RENDERFULLCONTENT = 0x00000002


def find_window():
    hwnd = [None]

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(h, lparam):
        length = user32.GetWindowTextLengthW(h)
        if length == 0 or not user32.IsWindowVisible(h):
            return True
        buf = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(h, buf, length + 1)
        if "F1 Challenge" in buf.value:
            hwnd[0] = h
            return False
        return True

    user32.EnumWindows(cb, 0)
    if hwnd[0] is None:
        sys.exit("F1 Challenge window not found — is the game running?")
    return hwnd[0]


def capture(hwnd, out_path):
    rect = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    w, h = rect.right - rect.left, rect.bottom - rect.top
    if w <= 0 or h <= 0:
        sys.exit(f"bad window size {w}x{h} — is it minimized?")

    hdc_window = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_window)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_window, w, h)
    old = gdi32.SelectObject(hdc_mem, hbmp)

    ok = user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT)
    if not ok:
        gdi32.SelectObject(hdc_mem, old)
        gdi32.DeleteObject(hbmp)
        gdi32.DeleteDC(hdc_mem)
        user32.ReleaseDC(hwnd, hdc_window)
        sys.exit("PrintWindow failed")

    # BITMAPINFOHEADER for 32bpp top-down DIB
    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG),
            ("biHeight", wintypes.LONG), ("biPlanes", wintypes.WORD),
            ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
            ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
            ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD),
            ("biClrImportant", wintypes.DWORD),
        ]

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = w
    bmi.biHeight = -h  # top-down
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0  # BI_RGB

    buf_size = w * h * 4
    buf = (ctypes.c_ubyte * buf_size)()
    gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), 0)

    gdi32.SelectObject(hdc_mem, old)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(hwnd, hdc_window)

    from PIL import Image
    img = Image.frombuffer("RGBA", (w, h), bytes(buf), "raw", "BGRA", 0, 1)
    img = img.convert("RGB")
    img.save(out_path)
    print(f"  saved {w}x{h} -> {out_path}")


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "tools/shot.png"
    hwnd = find_window()
    capture(hwnd, out_path)


if __name__ == "__main__":
    main()
