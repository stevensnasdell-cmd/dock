#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Steven Snasdell
"""Dock's reference screens (docs/design/DOCK_STYLE.md): HOME, FOCUS, FX and LOOPER at the FM-1's 240 x 240, in Dock's
fonts, tokens and grid, drawn from the scamps ("Dock FM-1 Screens"). The firmware's screens are built to these.

  dock_refs.py [OUTDIR]     -> OUTDIR/<screen>.png (240 x 240) and OUTDIR/sheet.png (all four, 2x)
                               (default docs/design/dock/ref)
Needs Pillow with Raqm. Prints FIT for any text wider than the place it is given (the style guide: nothing is cut).
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / "assets" / "fonts"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "design" / "dock" / "ref"

# ---- tokens (DOCK_STYLE.md "Colour") -------------------------------------------------------------------------
BG, SURFACE, RAISED, LINE = "#0B0B0E", "#16161B", "#1E1E25", "#2A2A33"
TEXT, MID, DIM = "#F2F0EA", "#9A9AA4", "#5C5C68"
BASS, KEYS, LEAD, DRUMS = "#A98BFF", "#46D39A", "#FFB547", "#E9DFC7"
REC = "#FF5A5F"


def rgb(c):
    c = c.lstrip("#")
    return tuple(int(c[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, k):                                   # k of b over a
    a, b = rgb(a), rgb(b)
    return "#%02X%02X%02X" % tuple(round(a[i] + (b[i] - a[i]) * k) for i in range(3))


def idle(c): return mix(BG, c, .55)                 # a member that is not sounding
def tint(c): return mix(SURFACE, c, .18)            # the picked row, a knob turning (fills)


# ---- type (DOCK_STYLE.md "Type") ------------------------------------------------------------------------------
def face(file, px, wght=None):
    f = ImageFont.truetype(str(FONTS / file), px, layout_engine=ImageFont.Layout.RAQM)
    if wght:
        try:
            f.set_variation_by_axes([wght])
        except OSError:
            pass
    return f


SG = "SpaceGrotesk[wght].ttf"
LB = face(SG, 16, 500)        # labels, tags, chips
TX = face(SG, 20, 500)        # names, rows, header
TXB = face(SG, 20, 700)       # the badge letter, the screen's title
VL = face(SG, 28, 600)        # values in words, a name being browsed
HR = face(SG, 48, 600)        # (big names: rarely fits; ty_fit drops to VL)
NUM_VL = face("Unbounded[wght].ttf", 28, 600)       # big numbers: values, the bar counter
NUM_HR = face("Unbounded[wght].ttf", 48, 600)
WORD = face("Unbounded[wght].ttf", 18, 700)         # the DOCK wordmark, a Line-up's name
MONO = face("SpaceMono-Regular.ttf", 16)            # data: channels, voices, BPM, rates
MONOB = face("SpaceMono-Bold.ttf", 16)

fits = []


class Screen:
    def __init__(self, name):
        self.name, self.im = name, Image.new("RGB", (240, 240), BG)
        self.d = ImageDraw.Draw(self.im)

    def text(self, xy, s, f, fill, w=None, anchor="ls"):
        """text at its baseline (anchor ls), or right-aligned (rs) / centred (ms); w: the width it may take"""
        tw = self.d.textlength(s, font=f)
        if w is not None and tw > w:
            fits.append(f"FIT {self.name}: '{s}' is {tw:.0f} px, has {w}")
        self.d.text(xy, s, font=f, fill=fill, anchor=anchor)
        return tw

    def card(self, box, fill=SURFACE, r=8, outline=None):
        self.d.rounded_rectangle(box, r, fill=fill, outline=outline, width=1)

    def badge(self, x, y, letter, c, size=24):
        self.card((x, y, x + size, y + size), c, 6)
        self.text((x + size / 2, y + size / 2 + 7), letter, TXB, BG, anchor="ms")

    def save(self, d):
        self.im.save(d / f"{self.name}.png")
        return self.im


def header(s, title, right, right_c=TEXT):
    s.text((8, 22), title, WORD if title == "DOCK" else TXB, TEXT)


def home():
    s = Screen("home")
    w = s.text((8, 22), "DOCK", WORD, TEXT)
    s.text((8 + w + 8, 22), "Yacht", TX, MID, w=80)
    s.d.ellipse((168, 11, 176, 19), fill=TEXT)          # clock: external (filled), internal (ring)
    s.text((232, 22), "112", MONOB, TEXT, anchor="rs")
    # the four knobs, always the same four
    for k, (lab, v) in enumerate([("Tone", .62), ("Bite", .35), ("Shape", .48), ("Space", .70)]):
        x = 2 + 60 * k
        s.card((x, 32, x + 57, 80))
        s.text((x + 4, 52), lab, LB, TEXT, w=49)
        s.d.rounded_rectangle((x + 5, 66, x + 52, 69), 2, fill=LINE)
        s.d.rounded_rectangle((x + 5, 66, x + 5 + round(47 * v), 69), 2, fill=TEXT)
    # the band, a row each: badge, sound, what it is doing
    rows = [("B", "Funk Bass II", BASS, [3, 6, 9, 6, 4], False, "LOOP"),
            ("K", "Suitcase Rhodes", KEYS, [5, 8, 10, 9, 7], True, None),
            ("L", "Silk Lead", LEAD, None, False, None),
            ("D", "Laid-back", DRUMS, [9, 2, 6, 2, 9], False, None)]
    for r, (b, name, c, bars, picked, chip) in enumerate(rows):
        y = 84 + 32 * r
        s.card((2, y, 238, y + 30), tint(c) if picked else SURFACE, 7)
        s.badge(5, y + 3, b, c if bars else idle(c))
        s.text((36, y + 22), name, TX, TEXT if bars or picked else MID, w=136 if chip else 166)
        if chip:
            s.card((176, y + 7, 200, y + 23), c, 4)
            s.text((188, y + 20), "4", MONOB, BG, anchor="ms")
        for i in range(5):                               # the meter: five bars; dots when silent
            x = 206 + 6 * i
            if bars:
                h = bars[i] * 2
                s.d.rounded_rectangle((x, y + 24 - h, x + 3, y + 24), 1, fill=c)
            else:
                s.d.ellipse((x, y + 20, x + 3, y + 23), fill=LINE)
    # the eight band voices, coloured by owner (the drummer's lane is not among them)
    s.text((8, 233), "VOICES", LB, MID, w=64)
    for i, c in enumerate([BASS, KEYS, KEYS, KEYS, KEYS, None, None, None]):
        x = 82 + 19 * i
        s.card((x, 220, x + 15, 235), c or SURFACE, 4, None if c else LINE)
    return s


def focus():
    s = Screen("focus")
    s.badge(6, 3, "K", KEYS)
    s.text((38, 22), "KEYS", TXB, TEXT, w=80)
    s.text((232, 22), "4/6", MONO, MID, anchor="rs")
    for k, (lab, v, turning) in enumerate([("Tone", "58", False), ("Bark", "72", True), ("Decay", "44", False),
                                           ("Space", "63", False)]):
        x = 2 + 60 * k
        s.card((x, 32, x + 57, 80), KEYS if turning else SURFACE)
        s.text((x + 4, 50), lab, LB, BG if turning else MID, w=49)
        s.text((x + 4, 75), v, NUM_VL, BG if turning else TEXT, w=49)
    # the picture: what the turning knob shapes (BARK: the pickup's drive against velocity), never covered
    s.card((2, 84, 238, 208))
    s.text((10, 102), "BARK", LB, KEYS, w=80)
    pts = [(12, 196), (60, 190), (110, 178), (150, 158), (190, 128), (228, 104)]
    s.d.polygon(pts + [(228, 198), (12, 198)], fill=mix(SURFACE, KEYS, .16))
    s.d.line(pts, fill=KEYS, width=3, joint="curve")
    s.d.ellipse((186, 124, 194, 132), fill=TEXT)          # where this note's velocity sits
    s.text((228, 102), "velocity", LB, MID, anchor="rs", w=90)
    # the sound bar
    s.text((8, 232), "Suitcase Rhodes", TX, TEXT, w=170)
    s.text((232, 232), "EP", LB, MID, anchor="rs")
    return s


def fx():
    s = Screen("fx")
    s.text((8, 22), "FX", TXB, TEXT)
    s.text((232, 22), "1/8", MONO, MID, anchor="rs")
    for k, (lab, v) in enumerate([("Depth", "80"), ("Mix", "100"), ("Rate", "1/8"), ("Gate", "60")]):
        x = 2 + 60 * k
        s.card((x, 32, x + 57, 80))
        s.text((x + 5, 50), lab, LB, MID, w=48)
        s.text((x + 5, 74), v, MONOB, TEXT, w=48)
    # who goes through: the black keys
    for k, (lab, c, on) in enumerate([("BASS", BASS, True), ("KEYS", KEYS, False), ("LEAD", LEAD, True)]):
        x = 2 + 80 * k
        s.card((x, 84, x + 76, 110), c if on else SURFACE, 7, None if on else LINE)
        s.text((x + 38, 103), lab, LB, BG if on else DIM, anchor="ms", w=66)
    # the effects: the white keys, one held
    fxs = [("C", "STUT", "1/8"), ("D", "STUT", "1/16"), ("E", "REV", "1 bt"), ("F", "STOP", "1/2"),
           ("G", "FRZ", "hold"), ("A", "SWP", "LP"), ("B", "VERB", "all"), ("C", "ECHO", "3/16")]
    for i, (key, lab, v) in enumerate(fxs):
        x, y = 2 + 60 * (i % 4), 114 + 63 * (i // 4)
        held = i == 0
        s.card((x, y, x + 57, y + 59), TEXT if held else SURFACE)
        s.text((x + 7, y + 18), key, MONO, BG if held else DIM)
        s.text((x + 7, y + 37), lab, LB, BG if held else TEXT, w=45)
        s.text((x + 7, y + 54), v, MONO, BG if held else MID, w=45)
    return s


def looper():
    s = Screen("looper")
    s.d.ellipse((8, 9, 20, 21), fill=REC)
    s.text((28, 22), "REC", TXB, TEXT)
    s.badge(80, 3, "B", BASS)
    s.text((232, 22), "BARS", LB, MID, anchor="rs")
    w = s.text((8, 76), "2", NUM_HR, TEXT)
    s.text((8 + w + 6, 76), "/4", NUM_VL, DIM)
    s.text((232, 52), "LENGTH", LB, MID, anchor="rs")
    s.text((232, 76), "4", NUM_VL, TEXT, anchor="rs")
    for i in range(16):                                  # the bars' beats: done, now, to come (the downbeats lighter)
        x = 2 + 15 * i
        c = BASS if i < 6 else (RAISED if i % 4 else LINE)
        s.d.rounded_rectangle((x, 88, x + 12, 96), 3, fill=c)
    s.card((2, 102, 238, 208))
    for (x0, x1, row, c) in [(10, 40, 4, BASS), (44, 64, 4, BASS), (70, 86, 2, BASS), (90, 112, 3, BASS),
                             (116, 124, 4, BASS), (150, 176, 4, RAISED), (182, 200, 2, RAISED)]:
        y = 112 + 18 * row
        s.d.rounded_rectangle((x0, y, x1, y + 8), 3, fill=c)
    s.d.line((126, 106, 126, 204), fill=TEXT, width=2)  # the playhead
    s.text((8, 232), "closes on bar 5", TX, MID, w=224)
    return s


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    ims = [f().save(OUT) for f in (home, focus, fx, looper)]
    sheet = Image.new("RGB", (4 * 480 + 5 * 16, 480 + 32), "#202024")
    for i, im in enumerate(ims):
        sheet.paste(im.resize((480, 480), Image.NEAREST), (16 + i * 496, 16))
    sheet.save(OUT / "sheet.png")
    for f in fits:
        print(f)
    print(f"dock refs: {OUT} ({len(ims)} screens, {len(fits)} FIT findings)")
    return 1 if fits else 0


if __name__ == "__main__":
    sys.exit(main())
