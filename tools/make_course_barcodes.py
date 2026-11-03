#!/usr/bin/env python3
"""Printable Code 39 test barcodes at the course size, with wide white margins.

Each A4 landscape page holds one *X* symbol: 3 mm narrow / 9 mm wide elements,
141 mm long (the size in the barcode and line specification), centred so there
is about 78 mm of white before the first bar and after the last bar. Labels
and the 100 mm scale check sit above and below the bars, outside the band the
sensor crosses. Print at 100% ("Actual size"), not "Fit to page".

Usage: python tools/make_course_barcodes.py [output.pdf]
"""
from pathlib import Path
import sys

MM = 72.0 / 25.4                     # PDF points per millimetre
PAGE_W, PAGE_H = 297 * MM, 210 * MM  # A4 landscape
NARROW_MM, WIDE_MM = 3.0, 9.0
BAR_HEIGHT_MM = 80.0
PATTERNS = {'*': 0x094, 'A': 0x109, 'B': 0x049, 'C': 0x148, 'D': 0x019,
            'Z': 0x0D0}
LETTERS = ['A', 'B', 'C', 'D', 'Z']


def element_widths(letter):
    """29 alternating widths in mm, starting and ending with a bar."""
    widths = []
    for index, char in enumerate('*' + letter + '*'):
        pattern = PATTERNS[char]
        for bit in range(9):
            widths.append(WIDE_MM if (pattern >> (8 - bit)) & 1 else NARROW_MM)
        if index < 2:
            widths.append(NARROW_MM)
    return widths


def page_stream(letter):
    widths = element_widths(letter)
    total = sum(widths)
    x = (297.0 - total) / 2.0
    y = (210.0 - BAR_HEIGHT_MM) / 2.0
    ops = ['0 g']
    for index, width in enumerate(widths):
        if index % 2 == 0:
            ops.append(f'{x * MM:.3f} {y * MM:.3f} {width * MM:.3f} '
                       f'{BAR_HEIGHT_MM * MM:.3f} re f')
        x += width
    left = (297.0 - total) / 2.0
    top = y + BAR_HEIGHT_MM
    text = [
        (12, 15, 195, f'Code 39 *{letter}*   letter {letter}'),
        (9, 15, 188, f'{total:.0f} mm long, {NARROW_MM:.0f} mm narrow / '
                     f'{WIDE_MM:.0f} mm wide. Print at 100% (Actual size).'),
        (9, 15, 182, 'Drive across the bars. Keep tape, labels and paper '
                     'edges out of the sensor path.'),
        (9, 15, 22, 'Scale check: the line below must measure 100 mm.'),
    ]
    for size, tx, ty, string in text:
        safe = string.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')
        ops.append(f'BT /F1 {size} Tf {tx * MM:.3f} {ty * MM:.3f} Td ({safe}) Tj ET')
    # 100 mm scale line with end ticks, below the bars (outside the scan band).
    ops.append('0.5 w')
    ops.append(f'{15 * MM:.3f} {15 * MM:.3f} m {115 * MM:.3f} {15 * MM:.3f} l S')
    for tick in (15, 115):
        ops.append(f'{tick * MM:.3f} {12 * MM:.3f} m {tick * MM:.3f} {18 * MM:.3f} l S')
    assert top < 182 and y > 22 and left > 70
    return '\n'.join(ops).encode('ascii')


def build_pdf(letters):
    objects = []

    def add(body):
        objects.append(body)
        return len(objects)

    font = add(b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>')
    pages_id = len(objects) + 1 + 2 * len(letters)
    page_ids = []
    for letter in letters:
        stream = page_stream(letter)
        content = add(b'<< /Length %d >>\nstream\n' % len(stream) + stream +
                      b'\nendstream')
        page_ids.append(add(
            b'<< /Type /Page /Parent %d 0 R /MediaBox [0 0 %.3f %.3f] '
            b'/Resources << /Font << /F1 %d 0 R >> >> /Contents %d 0 R >>'
            % (pages_id, PAGE_W, PAGE_H, font, content)))
    kids = b' '.join(b'%d 0 R' % p for p in page_ids)
    assert add(b'<< /Type /Pages /Kids [%s] /Count %d >>'
               % (kids, len(page_ids))) == pages_id
    catalog = add(b'<< /Type /Catalog /Pages %d 0 R >>' % pages_id)

    out = bytearray(b'%PDF-1.4\n')
    offsets = []
    for number, body in enumerate(objects, start=1):
        offsets.append(len(out))
        out += b'%d 0 obj\n' % number + body + b'\nendobj\n'
    xref = len(out)
    out += b'xref\n0 %d\n0000000000 65535 f \n' % (len(objects) + 1)
    for offset in offsets:
        out += b'%010d 00000 n \n' % offset
    out += (b'trailer\n<< /Size %d /Root %d 0 R >>\nstartxref\n%d\n%%%%EOF\n'
            % (len(objects) + 1, catalog, xref))
    return bytes(out)


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    target = Path(sys.argv[1]) if len(sys.argv) > 1 else \
        root / 'output' / 'pdf' / 'AAI10-barcodes-course-size-A4.pdf'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(build_pdf(LETTERS))
    print(f'Wrote {target} ({len(LETTERS)} pages: {", ".join(LETTERS)})')
