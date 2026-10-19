#!/usr/bin/env python3
"""Prepare verified store screenshots and a banner drawn natively at 720×320.

Requires Pillow. Run after capture_screenshots.py from this checkout.
The screenshot pixels are copied unchanged; the banner is original typography,
drawn directly at its final size with the repository's fonts. No image is resized.
"""

import hashlib
import json
from pathlib import Path
import shutil

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "store/1.6.0"
SELECTION = [
    ("01-clear-words", "01-time-in-words", "Time in words"),
    ("02-colour-rows", "02-make-it-yours", "Make it yours"),
    ("03-corner-date", "03-corner-date", "A discreet date"),
    ("04-exact-time", "04-exact-time", "Exact when needed"),
    ("05-status-proof", "05-status-indicators", "Useful details, quietly"),
]


def main():
    """Validate capture provenance, prepare the gallery and draw native artwork."""
    DESTINATION.mkdir(parents=True, exist_ok=True)
    manifest = {"name": "Wordish", "version": "1.6.0", "screenshots": []}
    bundle_hash = None
    for platform, dimensions in (("emery", (200, 228)), ("gabbro", (260, 260))):
        capture_manifest = ROOT / f"screenshots/release-1.6.0/{platform}_manifest.json"
        source = json.loads(capture_manifest.read_text())
        metadata = source["bundle_metadata"]
        if metadata["longName"] != "Wordish" or metadata["versionLabel"] != "1.6.0":
            raise ValueError("Capture does not describe Wordish 1.6.0")
        digest = hashlib.sha256((ROOT / source["bundle"]).read_bytes()).hexdigest()
        if digest != source["bundle_sha256"] or bundle_hash not in (None, digest):
            raise ValueError("Rebuild and recapture: bundle differs from screenshot provenance")
        bundle_hash = digest
        for filename, expected in source["source_sha256"].items():
            if hashlib.sha256((ROOT / filename).read_bytes()).hexdigest() != expected:
                raise ValueError(f"Recapture changed source: {filename}")
        output = DESTINATION / platform
        output.mkdir(exist_ok=True)
        for original, selected, caption in SELECTION:
            filename = f"screenshots/release-1.6.0/{platform}_{original}.png"
            shot = next(item for item in source["captures"] if item["file"] == filename)
            data = (ROOT / filename).read_bytes()
            if hashlib.sha256(data).hexdigest() != shot["sha256"]:
                raise ValueError(f"Screenshot checksum differs: {filename}")
            with Image.open(ROOT / filename) as image:
                if image.size != dimensions or image.format != "PNG":
                    raise ValueError(f"Screenshot format or dimensions differ: {filename}")
            target = output / f"{selected}.png"
            shutil.copyfile(ROOT / filename, target)
            manifest["screenshots"].append({
                "platform": platform, "file": str(target.relative_to(DESTINATION)),
                "caption": caption, "source": filename, "sha256": shot["sha256"],
                "width": dimensions[0], "height": dimensions[1],
                "captured_utc": shot["captured_utc"],
            })
        manifest.update(bundle=source["bundle"], bundle_sha256=bundle_hash,
                        uuid=metadata["uuid"], version_code=metadata["versionCode"])

    # Native-size text rasterisation only: no source artwork, resize or crop step.
    banner = Image.new("RGB", (720, 320), "#101114")
    draw = ImageDraw.Draw(banner)
    bold = ROOT / "resources/fonts/Roboto-Bold.ttf"
    light = ROOT / "resources/fonts/Roboto-Condensed.ttf"
    labels = [
        ("Wordish", bold, 88, (28, 104), "#F7F4ED"),
        ("Time in words.", light, 28, (32, 205), "#F7F4ED"),
        ("quarter", light, 70, (470, 39), "#FF5B55"),
        ("past", light, 70, (470, 117), "#FFDA45"),
        ("three", bold, 70, (470, 195), "#48D968"),
    ]
    for text, font_path, size, position, colour in labels:
        font = ImageFont.truetype(str(font_path), size)
        left, top, right, bottom = draw.textbbox(position, text, font=font, anchor="lt")
        if left < 24 or top < 24 or right > 696 or bottom > 296:
            raise ValueError(f"Banner text exceeds safe area: {text}")
        draw.text(position, text, font=font, fill=colour, anchor="lt")
    banner_path = DESTINATION / "wordish-banner.png"
    banner.save(banner_path, optimize=True)
    manifest["banner"] = {
        "file": banner_path.name, "width": 720, "height": 320,
        "sha256": hashlib.sha256(banner_path.read_bytes()).hexdigest(),
        "method": "Original typography drawn natively at 720x320; no resizing",
    }
    for image in [banner_path, *DESTINATION.glob("*/*.png")]:
        if image.stat().st_size > 4_400_000:
            raise ValueError(f"Store image exceeds 4.4 MB: {image}")
    (DESTINATION / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("Prepared 10 unchanged screenshots and one native 720x320 banner")


if __name__ == "__main__":
    main()
