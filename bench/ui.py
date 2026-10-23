"""Measures how many frames a second the browser paints while the page's
layout moves a made-up graph, from 500 nodes to 20,000, drawn with SVG, drawn
on a canvas, and not drawn at all, which is what the layout costs on its own.
Chromium opens ui/bench.html, which sg serve serves, through Playwright, and
the frames a second at each size go in bench/ui.json.

From the repository's root, after building, with sg serve running:

    ./build/sg serve animals.sg 8474 &
    uv run --with playwright==1.64.0 python bench/ui.py
"""

import json
from pathlib import Path

from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parent.parent
PAGE = "http://localhost:8474/bench.html"
SIZES = [500, 1000, 2000, 5000, 10000, 20000]
DRAWS = ["none", "canvas", "svg"]


def main():
    results = []
    with sync_playwright() as playwright:
        # With a window, Chromium paints with the GPU, as a browser on a
        # laptop does. Without one, it paints a canvas in software.
        browser = playwright.chromium.launch(headless=False)
        page = browser.new_page(viewport={"width": 1440, "height": 900})
        for nodes in SIZES:
            row = {"nodes": nodes}
            for draw in DRAWS:
                page.goto(f"{PAGE}?draw={draw}&nodes={nodes}")
                page.wait_for_function("window.result", timeout=120_000)
                fps = page.evaluate("window.result.frames_per_second")
                row[draw] = round(fps, 1)
            print(json.dumps(row), flush=True)
            results.append(row)
        browser.close()
    with open(ROOT / "bench" / "ui.json", "w") as out:
        json.dump(results, out, indent=2)


if __name__ == "__main__":
    main()
