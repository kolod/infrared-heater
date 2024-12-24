#! /usr/bin/python3

from typing import List
from pathlib import Path
from datetime import datetime, timezone
from struct import unpack
from re import sub

try:
    Import("env") # type: ignore
except NameError as e:
    root = Path(__file__).parent
    env = {}
    env['PROJECT_DIR']         = root
    env['PROJECT_INCLUDE_DIR'] = root / 'include'
    env['PROJECT_SRC_DIR']     = root / 'src'

try:
    from PIL import Image, ImageFont, ImageDraw                     # type: ignore
except ModuleNotFoundError:
    env.Execute("$PYTHONEXE -m pip install pillow")                 # type: ignore
    from PIL import Image, ImageFont, ImageDraw                     # type: ignore

try:
    from scipy.optimize import minimize, OptimizeResult, Bounds     # type: ignore
except ModuleNotFoundError:
    env.Execute("$PYTHONEXE -m pip install scipy")                  # type: ignore
    from scipy.optimize import minimize, OptimizeResult, Bounds     # type: ignore


def camel_to_snake(name):
    s1 = sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    s2 = sub('([a-z0-9])([A-Z])', r'\1_\2', s1)
    return s2.lower()


class GenerateFonts(object):

    def __init__(self) -> None:
        self.include          = Path(env['PROJECT_INCLUDE_DIR'])    # type: ignore
        self.sources          = Path(env['PROJECT_SRC_DIR'])        # type: ignore
        self.root             = Path(env['PROJECT_DIR'])            # type: ignore
        self.header_path      = self.include.joinpath('fonts.h')
        source_header_path    = self.include.joinpath('fonts.in.h')
        self.fonts            = self.root / 'fonts'
        self.max_gliph_size    = 0

        self.offset  = 0.03125

        LETTERS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
        letters = 'abcdefghijklmnopqrstuvwxyz'
        nubers = '0123456789'
        simbols = '~!@#$%^&*()_+-/\\|,.[]{}"\'<=>'
        degree = '°'

        header = source_header_path.read_text(encoding='utf-8', errors='ignore')
        with self.header_path.open('wt', encoding='utf-8', errors='ignore') as out:
            out.write('// This file is auto-generated, do not edit it by hand.\n')
            out.write(f'// Generated {datetime.now(tz=timezone.utc)}\n\n')
            out.write(header)

        # FiraCode:64
        self.generate_monospaced_font(
            name='FiraCode',
            font_path=self.fonts / 'FiraCode-Regular.ttf', 
            height=64,
            chars=LETTERS + letters + nubers + simbols + degree,
        )

        with self.header_path.open('a+', encoding='utf-8', errors='ignore') as out:
            out.write(f'\n#define FONT_MAX_GLIPH_SIZE {self.max_gliph_size}\n')


    def generate_monospaced_font(self, name:str, font_path: Path, height: int, chars: str, face:int = 0):
        snake_name  = camel_to_snake(name)
        image_path  = self.include.joinpath(snake_name).with_suffix('.png')
        source_path = self.sources.joinpath(snake_name).with_suffix('.c')
        font_size   = self.optimize_font_size(font_path, height, chars, face)
        font_width, baseline = self.font_width_and_baceline(font_path, height, font_size, chars, face)

        # Create font image
        img   = Image.new('L', (font_width, len(chars) * height), color=0)
        draw  = ImageDraw.Draw(img)
        font  = ImageFont.truetype(str(font_path), size=font_size, index=face, encoding="unic")

        for i, char in enumerate(chars):
            pos = (font_width / 2, baseline + i * height)
            draw.text(pos, char, font=font, anchor='ms', fill=255)

        img.save(image_path)

        # Create font *.cpp file
        header = '\n'.join([
            '// This file is auto-generated, do not edit it by hand.',
            f'// Generated {datetime.now(tz=timezone.utc)} with parameters:',
            f'//   Font name: {name}',
            f'//   Font size: {font_size}',
            f'//   Font face: {face}',
            f'//   Char height: {height}',
            f'//   Char width: {font_width}',
            f'//   Char baseline: {baseline}',
            f'//   Chars: {", ".join(chars)}',
            '', '#include "fonts.h"',
            '', ''
        ])

        with source_path.open('wt', encoding='utf-8', errors='ignore') as out:
            out.write(header)

            pixmap = img.load()
            rows = []
            for y in range(img.height):
                row = []
                for x in range(img.width):
                    row.append(f'0x{pixmap[x,y]:02X}')
                rows.append('    ' + ', '.join(row))
            
            out.write('const uint8_t ' + snake_name + '_data[] = { \n' + ',\n'.join(rows) + '\n};\n\n')
        
            next = 'NULL'
            for i, ch in reversed(list(enumerate(chars))):
                wch = list(unpack('<I', ch.encode('utf-32le')))[0]
                offset = i * font_width * height
                name = f'{snake_name}_gliph_{i:03d}'
                out.write(f'const gliph_t {name} = {{ .key = 0x{wch:04x}, .data = &{snake_name}_data[0x{offset:08x}], .next = {next} }}; /* {ch} */\n' )
                next = f'&{name}'

            out.write(f'\nconst font_t {snake_name} = {{ .height = {height}, .width = {font_width}, .baseline = {baseline}, .gliph = {next} }};\n')

        # Appand data to `fonts.h`
        with self.header_path.open('at+', encoding='utf-8', errors='ignore') as out:
            out.write(f'\nextern const uint8_t {snake_name}_data[];\n')
            out.write(f'extern const font_t {snake_name};\n')

        self.max_gliph_size = max(self.max_gliph_size, font_width * height)



    def optimize_font_size(self, font_path: Path, height: int, chars: str, face:int):

        def error(x: float, font: str, height: int, chars: str, face:int):
            size = x[0]
            box_min_top    = 9999999999
            box_max_bottom = 0
            offset         = int(height * self.offset)
            ifont          = ImageFont.truetype(font, size=size, index=face)
            for char in chars:
                _, top, _, bottom = ifont.getbbox(str(char), anchor='ms')
                box_min_top    = min(box_min_top, top)
                box_max_bottom = max(box_max_bottom, bottom)

            result = abs((box_max_bottom - box_min_top) + offset*2 - height) - 0.001*size
            return result

        result: OptimizeResult = minimize(
            fun=error, 
            x0=1.0, 
            args=(str(font_path), height, chars, face), 
            bounds=Bounds(1.0, 200.0),
            method='powell',
            tol=0.000000000000000001
        )

        if result.success:
            return result.x[0]

        raise ValueError("Can't find optimal font size.")


    def font_width_and_baceline(self, font_path: Path, height: int, size: float, chars: str, face:int):
        offset         = int(height * self.offset)
        box_min_left   = 9999999999
        box_min_top    = 9999999999
        box_max_right  = 0
        box_max_bottom = 0

        font           = ImageFont.truetype(str(font_path), size=size, index=face)
        for char in chars:
            left, top, right, bottom = font.getbbox(str(char), anchor='ms')
            box_min_left   = min(box_min_left, left)
            box_min_top    = min(box_min_top, top)
            box_max_right  = max(box_max_right, right)
            box_max_bottom = max(box_max_bottom, bottom)

        width  = int(abs(box_max_right - box_min_left) + offset)
        baceline = int(abs(box_min_top) + offset)

        return width, baceline


GenerateFonts()
