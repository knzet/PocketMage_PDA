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
    "bin": "05_binary",
    "carray": "06_c_arrays"
}


# --------------------------------------------------
# Ensure directory structure
# --------------------------------------------------
def ensure_dirs():
    for folder in FOLDERS.values():
        os.makedirs(os.path.join(OUTPUT_DIR, folder), exist_ok=True)


# --------------------------------------------------
# Download image
# --------------------------------------------------
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


# --------------------------------------------------
# PACK image into 1-bit-per-pixel .bin format
# --------------------------------------------------
def write_bin_bitmap(input_path, output_path):
    # bit_img = resized_img.convert('1', dither=Image.NONE)
    img = Image.open(input_path).convert("1", dither=Image.NONE)  # already resized, but ensure 1-bit

    pixels = img.load()
    width, height = img.size

    # MUST be width divisible by 8
    if width % 8 != 0:
        raise ValueError("Width must be divisible by 8 for bin packing")

    data = bytearray()

    for y in range(height):
        for x in range(0, width, 8):
            byte = 0
            for bit in range(8):
                px = pixels[x + bit, y]
                bitval = 1 if px < 128 else 0  # dark → 1, light → 0
                byte |= (bitval << (7 - bit))
            data.append(byte)

    # for y in range(height):
    #     for x in range(0, width, 8):
    #         byte = 0
    #         for bit in range(8):
    #             px = pixels[x + bit, y]        # 0 = black, 255 = white
    #             bitval = 0 if px == 0 else 1   # match PocketMage logic
    #             byte |= (bitval << (7 - bit))
    #         data.append(byte)

    with open(output_path, "wb") as f:
        f.write(data)

    print(f"BIN written: {output_path} ({len(data)} bytes)")


# --------------------------------------------------
# Create a C array (your old last step)
# --------------------------------------------------
def convert_icon_to_c_array(input_path, output_path, array_name):
    img = Image.open(input_path).convert("1")
    pixels = img.load()
    data = bytearray()

    width, height = img.size
    if width % 8 != 0:
        raise ValueError("Width must be divisible by 8 for C array")

    # pack 8 pixels per byte
    for y in range(height):
        for x in range(0, width, 8):
            byte = 0
            for bit in range(8):
                px = pixels[x + bit, y]
                bitval = 0 if px == 0 else 1
                byte |= (bitval << (7 - bit))
            data.append(byte)

    with open(output_path, "w") as f:
        f.write(f"// Auto-generated from {input_path}\n")
        f.write(f"const unsigned char {array_name}[{len(data)}] = {{\n    ")

        for i, b in enumerate(data):
            f.write(f"0x{b:02X}")
            if i != len(data) - 1:
                f.write(", ")
            if (i + 1) % 12 == 0:
                f.write("\n    ")

        f.write("\n};\n")

    print(f"C array written: {output_path} ({len(data)} bytes)")


# --------------------------------------------------
# Master processor per image
# --------------------------------------------------
def process_image(input_path, index):
    # 1. grayscale
    gray_img = Image.open(input_path).convert("L")
    gray_path = os.path.join(OUTPUT_DIR, FOLDERS["gray"], f"ar{index:02d}.png")
    gray_img.save(gray_path)

    # 2. resize to PocketMage tarot size
    resized_img = gray_img.resize((128, 218), Image.NEAREST)
    resized_path = os.path.join(OUTPUT_DIR, FOLDERS["resized"], f"ar{index:02d}.png")
    resized_img.save(resized_path)

    # 3. convert to 1-bit bitmap
    bit_img = resized_img.convert("1", dither=Image.NONE)
    bit_path = os.path.join(OUTPUT_DIR, FOLDERS["bit1"], f"ar{index:02d}.png")
    bit_img.save(bit_path)

    # 4. NEW: raw .bin bitmap for PocketMage
    bin_path = os.path.join(OUTPUT_DIR, FOLDERS["bin"], f"ar{index:02d}.bin")
    write_bin_bitmap(resized_path, bin_path)

    # 5. C array (moved down)
    c_path = os.path.join(OUTPUT_DIR, FOLDERS["carray"], f"ar{index:02d}.c")
    convert_icon_to_c_array(resized_path, c_path, f"ar{index:02d}")


# --------------------------------------------------
# Main
# --------------------------------------------------
if __name__ == "__main__":
    ensure_dirs()

    for i in range(22):  # ar00–ar21
        img_path = download_image(i)
        if img_path:
            process_image(img_path, i)
