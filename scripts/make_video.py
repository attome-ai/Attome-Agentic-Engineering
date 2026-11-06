"""P5-t3: the demo video, rendered from stored runs only (decision D13: the demo is this video, not a live run).

Frames are screenshots of the stored reports taken with headless Chrome, plus title cards drawn here; the narration is
a draft voice from Windows' built-in speech synthesizer (System.Speech), to be replaced by the owner's voice if wanted
(record one WAV per scene with the same names under runs/video/voice/ and run again: those are used instead).
Every number on screen and in the narration comes from a stored run or spec named in SCENES.

Usage: python scripts/make_video.py  ->  runs/video/attome_demo.mp4 (1920 x 1080, 30 fps) and runs/video/contact.png
Needs: Chrome, ffmpeg on PATH, Windows PowerShell.
"""
from __future__ import annotations

import json
import shutil
import subprocess
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "runs" / "video"
SHOTS, FRAMES, VOICE, SEGS = OUT / "shots", OUT / "frames", OUT / "voice", OUT / "segments"
CHROME = Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe")
W, H, BAR = 1920, 1080, 150
BG, INK, ACCENT, MUTED = (246, 245, 241), (25, 28, 30), (31, 110, 96), (110, 112, 110)
FONT = "C:/Windows/Fonts/segoeui.ttf"
BOLD = "C:/Windows/Fonts/segoeuib.ttf"

BRACKET = "runs/p2_demo_delivered/report.html"
HEATSINK = "runs/heatsink_30w_fan60_2400/report.html"
SPEC = json.loads((ROOT / "docs/examples/bracket_20kg_delivered.json").read_text())

# (name, frame, caption, narration). frame: ("card", kind) or (shot, crop box in the 1600 px wide page)
SCENES = [
    ("01_title", ("card", "title"), "",
     "Designing a part that is light and provably safe usually takes CAD, simulation and optimization, all by hand. "
     "Attome Agentic Engineering does it from a plain-language brief."),
    ("02_brief", ("card", "spec"), "The brief becomes a spec you can check",
     "I describe the part: a wall shelf bracket that holds twenty kilograms, as light as possible, printed in PETG. "
     "The agent turns that into a spec I can check: the load with its design factor, a safety factor of two on printed "
     "strength, and every value with its source. Nothing runs until I accept it."),
    ("03_compare", ("safety", (190, 400, 910, 880)), "Hand design (left) against the recommended design: red fails, green is safe",
     "Here is a bracket designed by hand, next to the one the system found. Red is where a part would fail. "
     "The hand design has a safety factor of under one half. The recommended design weighs 184 grams, "
     "with a safety factor of 2.05."),
    ("04_search", ("rec", (190, 1780, 1410, 2290)), "1,600 designs, each a real finite-element solve",
     "To get there, the optimizer ran sixteen hundred real finite-element simulations of designs composed from a typed "
     "geometry grammar, so every candidate is valid geometry. Each line here is one design."),
    ("05_trust", ("rec", (196, 1150, 912, 1560)), "Trust record: grade A, every requirement with an error bar",
     "Why trust it? Lighter designs looked safe on the first mesh, but refining the mesh around their peak stress showed "
     "they fail, so this one is judged on the refined value. Every requirement has an error bar, the force balance checks "
     "out, and the solver matches seven textbook benchmarks. Grade A means every requirement is proven, not just met."),
    ("06_replay", ("rec", (190, 2290, 1410, 2960)), "The report keeps the whole search",
     "The report keeps the whole search: every design, the trade-off between mass and safety, and a replay of how the "
     "shape changed."),
    ("07_make", ("rec", (196, 3245, 1000, 3420)), "How to make it, at stated rates",
     "It also says how to make it: printing fits, at about eleven hours and twenty-two dollars at the rates I stated."),
    ("08_heatsink", ("heat", (190, 0, 1410, 1070)), "Same pipeline, different physics: a 30 W heat sink",
     "Same pipeline, different physics: a heat sink for a thirty-watt module with a small fan. It is fifty-seven percent "
     "lighter, and the seventy-five degree limit is what stops it going lighter. Grade A."),
    ("09_refuse", ("card", "refuse"), "When a brief cannot be met, it says so",
     "And when a brief cannot be met, it says so. Asked for a PETG bracket that holds one hundred kilograms, it checks "
     "the strongest design the family allows, in about ten seconds, and reports that it carries about thirty-eight."),
    ("10_close", ("card", "close"), "",
     "Open solvers, no licences, and every number traceable to a stored run. Attome Agentic Engineering."),
]
SHOT_URLS = {"rec": BRACKET, "safety": BRACKET + "#split-safety", "heat": HEATSINK}


def font(size, bold=False):
    return ImageFont.truetype(BOLD if bold else FONT, size)


def shoot(name: str, url: str) -> Path:
    out = SHOTS / f"{name}.png"
    if out.exists():
        return out
    base = "file:///" + str(ROOT).replace("\\", "/").replace(" ", "%20") + "/"
    subprocess.run([str(CHROME), "--headless=new", "--hide-scrollbars", "--window-size=1600,7000", "--virtual-time-budget=20000",
                    f"--user-data-dir={OUT / 'chrome-profile'}", f"--screenshot={out}", base + url],
                   capture_output=True, timeout=180, check=False)
    if not out.exists():
        raise SystemExit(f"Chrome did not write {out}")
    return out


def caption(img: Image.Image, text: str) -> None:
    if not text:
        return
    d = ImageDraw.Draw(img)
    d.rectangle([0, H - BAR, W, H], fill=(25, 28, 30))
    d.text((80, H - BAR / 2), text, font=font(42), fill=(240, 240, 236), anchor="lm")
    d.text((W - 80, H - BAR / 2), "Attome AE", font=font(30, True), fill=(120, 190, 175), anchor="rm")


def fit(src: Image.Image) -> Image.Image:
    img = Image.new("RGB", (W, H), BG)
    box_w, box_h = W - 120, H - BAR - 80
    s = min(box_w / src.width, box_h / src.height)
    im = src.resize((int(src.width * s), int(src.height * s)), Image.LANCZOS)
    img.paste(im, ((W - im.width) // 2, (H - BAR - im.height) // 2))
    return img


def card(kind: str) -> Image.Image:
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    if kind == "title":
        d.text((W / 2, 430), "Attome Agentic Engineering", font=font(96, True), fill=INK, anchor="mm")
        d.text((W / 2, 540), "From a plain-language brief to a verified part", font=font(52), fill=ACCENT, anchor="mm")
        d.text((W / 2, 640), "AIDTC 2027", font=font(36), fill=MUTED, anchor="mm")
    elif kind == "spec":
        d.text((120, 110), "Brief", font=font(40, True), fill=MUTED)
        d.text((120, 165), f"\u201c{SPEC['brief']}\u201d", font=font(50), fill=INK)
        d.text((120, 300), "Spec the agent proposes (checked by the engine, accepted by the user)", font=font(40, True), fill=MUTED)
        lc = SPEC["load_cases"][0]
        rows = [("Load", f"{-lc['loads'][0]['force'][2]:g} N on the arm end, factors {lc['factor_range'][0]:g} to {lc['factor_range'][1]:g}", lc["source"]),
                ("Material", SPEC["material"] + ", print knock-down applied", "brief: printed in PETG")]
        for r in SPEC["requirements"]:
            rows.append((r["id"], f"{r['quantity']} {r['op']} {r['value']:g}", r["source"]))
        y = 370
        for a, b, c in rows:
            d.text((120, y), a, font=font(38, True), fill=ACCENT)
            d.text((330, y), b, font=font(38), fill=INK)
            d.text((330, y + 50), "source: " + c, font=font(28), fill=MUTED)
            y += 112
    elif kind == "refuse":
        d.text((W / 2, 250), "\u201cA PETG bracket that holds 100 kg.\u201d", font=font(62), fill=INK, anchor="mm")
        d.text((W / 2, 420), "Not feasible in this family", font=font(80, True), fill=(176, 48, 48), anchor="mm")
        d.text((W / 2, 540), "The strongest design the bounds allow reaches a safety factor of about 0.76 against the 2 required:",
               font=font(38), fill=INK, anchor="mm")
        d.text((W / 2, 600), "it carries about 38 kg. Found in about 10 s, before any optimization.", font=font(38), fill=INK, anchor="mm")
        d.text((W / 2, 720), "benchmarks/bracket_100kg_feasibility.json", font=font(30), fill=MUTED, anchor="mm")
    elif kind == "close":
        lines = [("20 kg PETG bracket", "99 g, unsafe (SF 0.48)  \u2192  184 g, SF 2.05 \u00b1 0.1%, grade A"),
                 ("30 W heat sink", "103.7 g  \u2192  44.1 g at 74.4 \u00b0C, grade A"),
                 ("100 kg bracket", "refused: the family carries about 38 kg"),
                 ("Solver checks", "7 of 7 textbook benchmarks"),
                 ("Licences", "open solvers (CalculiX, gmsh, OpenCASCADE); no paid licence")]
        d.text((W / 2, 170), "Attome Agentic Engineering", font=font(72, True), fill=INK, anchor="mm")
        y = 330
        for a, b in lines:
            d.text((200, y), a, font=font(40, True), fill=ACCENT)
            d.text((640, y), b, font=font(40), fill=INK)
            y += 95
        d.text((W / 2, 880), "Every number comes from a stored run in the repository.", font=font(36), fill=MUTED, anchor="mm")
    return img


def speak(name: str, text: str) -> Path:
    own = VOICE / f"{name}.wav"
    if own.exists():
        return own
    tts = OUT / "tts" / f"{name}.wav"
    tts.parent.mkdir(parents=True, exist_ok=True)
    txt = OUT / "tts" / f"{name}.txt"
    txt.write_text(text, encoding="utf-8")
    ps = ("Add-Type -AssemblyName System.Speech; $s = New-Object System.Speech.Synthesis.SpeechSynthesizer; $s.Rate = 0; "
          f"$s.SetOutputToWaveFile('{tts}'); $s.Speak([IO.File]::ReadAllText('{txt}')); $s.Dispose()")
    subprocess.run(["powershell", "-NoProfile", "-Command", ps], check=True, capture_output=True, timeout=120)
    return tts


def seconds(wav: Path) -> float:
    with wave.open(str(wav)) as w:
        return w.getnframes() / w.getframerate()


def main():
    for d in (SHOTS, FRAMES, SEGS):
        d.mkdir(parents=True, exist_ok=True)
    ffmpeg = shutil.which("ffmpeg") or "ffmpeg"
    segs, thumbs, total = [], [], 0.0
    for name, frame, cap, text in SCENES:
        if frame[0] == "card":
            img = card(frame[1])
        else:
            src = Image.open(shoot(frame[0], SHOT_URLS[frame[0]])).convert("RGB").crop(frame[1])
            img = fit(src)
        caption(img, cap)
        fp = FRAMES / f"{name}.png"
        img.save(fp)
        thumbs.append(img.resize((480, 270)))
        wav = speak(name, text)
        dur = seconds(wav) + 1.0
        total += dur
        seg = SEGS / f"{name}.mp4"
        vf = f"fade=t=in:st=0:d=0.4,fade=t=out:st={dur - 0.4:.2f}:d=0.4,format=yuv420p"
        subprocess.run([ffmpeg, "-y", "-loglevel", "error", "-loop", "1", "-framerate", "30", "-i", str(fp), "-i", str(wav),
                        "-af", "adelay=300|300,apad", "-t", f"{dur:.2f}", "-vf", vf, "-c:v", "libx264", "-tune", "stillimage",
                        "-preset", "medium", "-crf", "18", "-c:a", "aac", "-b:a", "160k", "-ar", "48000", "-ac", "2", str(seg)], check=True)
        segs.append(seg)
        print(f"{name}: {dur:.1f} s", flush=True)
    lst = OUT / "segments.txt"
    lst.write_text("".join(f"file '{s.as_posix()}'\n" for s in segs))
    final = OUT / "attome_demo.mp4"
    subprocess.run([ffmpeg, "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(lst), "-c", "copy", str(final)], check=True)
    sheet = Image.new("RGB", (480 * 4, 270 * ((len(thumbs) + 3) // 4)), (0, 0, 0))
    for i, t in enumerate(thumbs):
        sheet.paste(t, (480 * (i % 4), 270 * (i // 4)))
    sheet.save(OUT / "contact.png")
    print(f"total {total:.1f} s -> {final}")


if __name__ == "__main__":
    main()
