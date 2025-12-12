import os
import requests
from PIL import Image

BASE_URL = "https://www.sacred-texts.com/tarot/pkt/img/ar{:02d}.jpg"

OUTPUT_DIR = "output"
FOLDERS = {
    "orig": "01_original",
    "gray": "02_gray",
    "resized": "03_resized",
    "bit1": "04_1bit",
    "bin": "05_binary"
}

# Ensure directory structure exists
def ensure_dirs():
    for folder in FOLDERS.values():
        os.makedirs(os.path.join(OUTPUT_DIR, folder), exist_ok=True)

def download_image(index):
    url = BASE_URL.format(index)
    resp = requests.get(url)
    if resp.status_code != 200:
        print(f"Failed to download {url}")
        return None

    path = os.path.join(OUTPUT_DIR, FOLDERS["orig"], f"ar{index:02d}.jpg")
    with open(path, "wb") as f:
        f.write(resp.content)
    print(f"Downloaded {url}")
    return path

def convert_icon_to_c_array(input_path, output_path, array_name):
    from PIL import Image

    img = Image.open(input_path)

    # Resize exactly as requested
    img = img.resize((128, 218), Image.NEAREST)

    # Convert to 1-bit
    img = img.convert("1")

    pixels = img.load()
    data = bytearray()

    # IMPORTANT:
    # The original function ONLY reads x = 0..39 (first 40 px)
    for y in range(218):
        for x in range(0, 40, 8):
            byte = 0
            for bit in range(8):
                px = pixels[x + bit, y]
                bitval = 0 if px == 0 else 1
                byte |= (bitval << (7 - bit))
            data.append(byte)

    # Convert bytearray to C array source code
    with open(output_path, "w") as f:
        f.write(f"// Auto-generated from {input_path}\n")
        f.write(f"const unsigned char {array_name}[{len(data)}] = {{\n    ")

        for i, b in enumerate(data):
            f.write(f"0x{b:02X}")
            if i != len(data) - 1:
                f.write(", ")
            if (i + 1) % 12 == 0:   # pretty formatting
                f.write("\n    ")

        f.write("\n};\n")

    print(f"C array written: {output_path} ({len(data)} bytes)")



def process_image(input_path, index):
    # 1 — Convert to grayscale
    gray_img = Image.open(input_path).convert("L")
    gray_path = os.path.join(OUTPUT_DIR, FOLDERS["gray"], f"ar{index:02d}.png")
    gray_img.save(gray_path)

    # 2 — Resize to 128x218
    resized_img = gray_img.resize((128, 218), Image.NEAREST)
    resized_path = os.path.join(OUTPUT_DIR, FOLDERS["resized"], f"ar{index:02d}.png")
    resized_img.save(resized_path)

    # 3 — Convert to 1-bit
    bit_img = resized_img.convert("1")
    bit_path = os.path.join(OUTPUT_DIR, FOLDERS["bit1"], f"ar{index:02d}.png")
    bit_img.save(bit_path)

    # 4 — Generate C byte array
    c_path = os.path.join(OUTPUT_DIR, FOLDERS["bin"], f"ar{index:02d}.c")
    convert_icon_to_c_array(resized_path, c_path, f"ar{index:02d}")


if __name__ == "__main__":
    ensure_dirs()

    for i in range(22):  # ar00–ar21
        img_path = download_image(i)
        if img_path:
            process_image(img_path, i)
