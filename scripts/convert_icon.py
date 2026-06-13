import os
import io
import sys
import subprocess
import tempfile

threshold = 128
USAGE = 'Usage: python scripts/convert_icon.py input.png|input.svg output_name width height'

def svg_to_png_bytes_cairosvg(svg_path, width, height):
    import cairosvg
    with open(svg_path, 'rb') as f:
        svg_data = f.read()
    return cairosvg.svg2png(bytestring=svg_data, output_width=width, output_height=height)

def svg_to_png_bytes_qlmanage(svg_path, width, height):
    # macOS fallback: qlmanage renders SVG thumbnails natively.
    # Render at 8× the target size so there's detail to work with after trimming.
    from PIL import Image, ImageOps
    with tempfile.TemporaryDirectory() as tmp:
        render_size = max(width, height) * 8
        subprocess.run(
            ['qlmanage', '-t', '-s', str(render_size), '-o', tmp, svg_path],
            check=True, capture_output=True
        )
        basename = os.path.basename(svg_path) + '.png'
        out_png = os.path.join(tmp, basename)
        img = Image.open(out_png).convert('RGBA')
        # Flatten alpha onto white background
        bg = Image.new('RGBA', img.size, (255, 255, 255, 255))
        bg.paste(img, mask=img.split()[3])
        img = bg.convert('L')
        # Trim white margins (invert so content=white, then crop, then invert back)
        inverted = ImageOps.invert(img)
        bbox = inverted.getbbox()
        if bbox:
            img = img.crop(bbox)
        buf = io.BytesIO()
        img.save(buf, format='PNG')
        return buf.getvalue()

def svg_to_png_bytes(svg_path, width, height):
    try:
        return svg_to_png_bytes_cairosvg(svg_path, width, height)
    except Exception:
        return svg_to_png_bytes_qlmanage(svg_path, width, height)

def load_image(path, width, height):
    from PIL import Image

    ext = os.path.splitext(path)[1].lower()
    if ext == '.svg':
        png_bytes = svg_to_png_bytes(path, width, height)
        img = Image.open(io.BytesIO(png_bytes))
        img = img.convert('RGBA')
        background = Image.new('RGBA', img.size, (255, 255, 255, 255))
        background.paste(img, mask=img.split()[3])
        img = background.convert('RGB')
        img = img.resize((width, height), Image.LANCZOS)
    else:
        img = Image.open(path)
        img = img.convert('RGBA')
        img = img.resize((width, height), Image.LANCZOS)
        background = Image.new('RGBA', img.size, (255, 255, 255, 255))
        background.paste(img, mask=img.split()[3])
        img = background.convert('RGB')
    # Rotate 90 degrees counterclockwise
    img = img.rotate(90, expand=True)
    return img

def image_to_c_array(img, array_name):
    img = img.convert('L')
    width, height = img.size
    pixels = list(img.getdata())
    packed = []
    for y in range(height):
        for x in range(0, width, 8):
            byte = 0
            for b in range(8):
                if x + b < width:
                    v = pixels[y * width + x + b]
                    bit = 1 if v >= threshold else 0
                    byte |= (bit << (7 - b))
            packed.append(byte)
    c = f'#pragma once\n#include <cstdint>\n\n'
    c += f'// size: {width}x{height}\n'
    c += f'static const uint8_t {array_name}[] = {{\n    '
    for i, v in enumerate(packed):
        c += f'0x{v:02X}, '
        if (i + 1) % 16 == 0:
            c += '\n    '
    c = c.rstrip(', \n') + '\n};\n'
    return c

def main():
    if any(arg in ('-h', '--help') for arg in sys.argv[1:]):
        print(USAGE)
        sys.exit(0)
    if len(sys.argv) != 5:
        print(USAGE)
        sys.exit(1)
    input_path, output_name, width, height = sys.argv[1:5]
    array_name = output_name.capitalize() + 'Icon'
    width, height = int(width), int(height)
    img = load_image(input_path, width, height)
    c_array = image_to_c_array(img, array_name)

    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    output_dir = os.path.join(project_root, 'src', 'components', 'icons')
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(output_dir, f'{output_name}.h')
    with open(output_path, 'w') as f:
        f.write(c_array)
    print(f'Wrote {output_path}')

if __name__ == '__main__':
    main()
