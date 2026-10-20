# Theme Asset References

These fixed one-bit images and glyph metrics are independent decoder references for the V1.0 theme resources in `user/inc/theme_assets.h`. They were generated with Pillow 10.4.0, FreeType 2.12.1, BASIC text layout and Matplotlib 3.9.2's bundled DejaVu fonts.

Ordinary builds and CI do not regenerate them. Font rasterization and layout libraries can differ across platforms even with identical Python package versions. `make theme-assets` writes candidate references to `build/theme-assets/`; review visual changes explicitly before updating these references or golden frame hashes.

Font licenses are in `docs/FONT-LICENSE.md`, `docs/LICENSE-DEJAVU.txt` and `docs/Unifont-OFL-1.1.txt`. No standalone outline font is distributed here.
