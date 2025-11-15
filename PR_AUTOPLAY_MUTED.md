# PR: Autoplay & Muted support (demo)

This small PR branch documents the change that enables `autoplay=1` and `muted=1` query parameters for the web demo.

Changes included in this branch:

- Added runtime handling in `index.html` to read `autoplay` and `muted` from the URL.
- If `muted=1`, the page mutes any `<audio>` elements and sets `window._battleship_demo_muted = true`.
- If `autoplay=1`, the demo now starts automatically using the same startup logic as the Start button.
- README updated to document the query parameters.

This branch is intended to be reviewed and merged into `main` if everything looks good.
