# Pixel glyph dependency

The firmware uses a build-generated subset of GNU Unifont 16.0.04 under SIL Open Font License 1.1. The upstream font is also offered under GPL-2.0-or-later with the GNU font embedding exception. Unifont is the work of Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller, David Corbett, and other Unifont contributors; all upstream copyrights are retained. The subset is identified as the desktop-clock UI subset, not as a new upstream release.

Source: https://unifoundry.com/pub/unifont/unifont-16.0.04/font-builds/unifont-16.0.04.hex.gz
Project and license: https://unifoundry.com/unifont.html
SIL OFL 1.1: https://unifoundry.com/OFL-1.1.txt
Alternative license and font exception: https://unifoundry.com/LICENSE.txt

Builds fetch the exact bitmaps rather than rasterizing a host system font. Firmware artifacts include the upstream license texts. No standalone font is included as a download artifact. The 5x7 large-number patterns in the generator are original to this firmware revision and are enlarged only by integer pixels.
