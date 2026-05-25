from PIL import Image
import os
import glob

def color_to_rgb565(r, g, b):
    # Standard RGB888 to RGB565 conversion
    # R: 5 bits, G: 6 bits, B: 5 bits
    r5 = (r * 31 + 127) // 255
    g6 = (g * 63 + 127) // 255
    b5 = (b * 31 + 127) // 255
    return (r5 << 11) | (g6 << 5) | b5

project_dir = r'C:\Users\cyroa\OneDrive\Documents\project\M5_PDM_PLOT'
icons_dir = os.path.join(project_dir, 'icons')
icons_data = []

# Get all *O.png files from the icons directory
png_files = sorted(glob.glob(os.path.join(icons_dir, '*O.png')))

num_icons = len(png_files)
print(f"Found {num_icons} icons in {icons_dir}")

# Explicitly define Blue and White in RGB565
# Blue (RGB 0, 0, 255) -> 0x001F
# White (RGB 255, 255, 255) -> 0xFFFF
BLUE_565 = 0x001F
WHITE_565 = 0xFFFF

for filename in png_files:
    print(f"Processing {os.path.basename(filename)} (Strict Blue/White mapping)...")
    img = Image.open(filename).convert('RGBA')
    
    if img.size != (50, 50):
        img = img.resize((50, 50), Image.NEAREST)
    
    # Create a white background
    white_bg = Image.new("RGBA", (50, 50), (255, 255, 255, 255))
    white_bg.paste(img, (0, 0), img)
    final_img = white_bg.convert('RGB')
    
    data = []
    for y in range(50):
        for x in range(50):
            r, g, b = final_img.getpixel((x, y))
            # If the pixel is not purely white, map it to blue
            # Using a slightly tighter threshold to avoid capturing noise as blue
            if r < 240 or g < 240 or b < 240:
                data.append(BLUE_565)
            else:
                data.append(WHITE_565)
    icons_data.append(data)

header_path = os.path.join(project_dir, 'icont.h')
with open(header_path, 'w') as f:
    f.write('#ifndef ICONT_H\n#define ICONT_H\n\n')
    f.write(f'const uint16_t icons[{num_icons}][2500] = {{\n')
    for icon in icons_data:
        f.write('  {')
        f.write(','.join(f"0x{d:04x}" for d in icon))
        f.write('},\n')
    f.write('};\n\n#endif\n')

print(f"Generated icon3.h with {num_icons} icons (Strict Blue/White)")
