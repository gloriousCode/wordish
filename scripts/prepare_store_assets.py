#!/usr/bin/env python3
"""Prepare verified store screenshots, a native 720×320 banner and app icons.

Requires Pillow. Run after capture_screenshots.py from this checkout.
The screenshot pixels are copied unchanged; banner and icons use original
typography drawn at their final sizes with the repository's fonts. Nothing is resized.
"""

import hashlib
import json
from pathlib import Path
import shutil

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "store/1.6.2"
DIMENSIONS = {"aplite": (144, 168), "basalt": (144, 168), "chalk": (180, 180),
              "diorite": (144, 168), "emery": (200, 228), "flint": (144, 168),
              "gabbro": (260, 260)}
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
    manifest = {"name": "Wordish", "version": "1.6.2", "screenshots": []}
    bundle_hash = None
    for platform, dimensions in DIMENSIONS.items():
        capture_manifest = ROOT / f"build/screenshots/release-1.6.2/{platform}_manifest.json"
        source = json.loads(capture_manifest.read_text())
        metadata = source["bundle_metadata"]
        if metadata["longName"] != "Wordish" or metadata["versionLabel"] != "1.6.2":
            raise ValueError("Capture does not describe Wordish 1.6.2")
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
            filename = f"build/screenshots/release-1.6.2/{platform}_{original}.png"
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
                "preset": shot.get("preset"),
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
    manifest["icons"] = []
    for size in (80, 144):
        icon = Image.new("RGB", (size, size), "#101114")
        icon_draw = ImageDraw.Draw(icon)
        font = ImageFont.truetype(str(bold), round(size * 0.72))
        icon_draw.text((size // 2, round(size * 0.08)), "W", font=font,
                       fill="#F7F4ED", anchor="mt")
        radius = max(2, round(size * 0.038))
        for fraction, colour in ((0.31, "#FF5B55"), (0.5, "#FFDA45"), (0.69, "#48D968")):
            x, y = round(size * fraction), round(size * 0.83)
            icon_draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=colour)
        icon_path = DESTINATION / f"wordish-icon-{size}.png"
        icon.save(icon_path, optimize=True)
        manifest["icons"].append({
            "file": icon_path.name, "width": size, "height": size,
            "sha256": hashlib.sha256(icon_path.read_bytes()).hexdigest(),
            "method": f"Original typography drawn natively at {size}x{size}; no resizing",
        })
    for image in [*DESTINATION.glob("*.png"), *DESTINATION.glob("*/*.png")]:
        if image.stat().st_size > 4_400_000:
            raise ValueError(f"Store image exceeds 4.4 MB: {image}")
    (DESTINATION / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Prepared {len(manifest['screenshots'])} unchanged screenshots, banner and two native icons")


if __name__ == "__main__":
    main()
