from PIL import Image
import sys
import os

def convert_icon(input_path, output_path):
    img = Image.open(input_path)

    # Force 40x40
    img = img.resize((40, 40), Image.NEAREST)

    # Convert to 1-bit
    img = img.convert("1")

    pixels = img.load()
    data = bytearray()

    for y in range(40):
        for x in range(0, 40, 8):
            byte = 0
            for bit in range(8):
                px = pixels[x + bit, y]
                bitval = 0 if px == 0 else 1
                byte |= (bitval << (7 - bit))
            data.append(byte)

    with open(output_path, "wb") as f:
        f.write(data)

    print(f"Icon written: {output_path} ({len(data)} bytes)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python make_icon.py input.bmp output.bin")
        sys.exit(1)

    convert_icon(sys.argv[1], sys.argv[2])
