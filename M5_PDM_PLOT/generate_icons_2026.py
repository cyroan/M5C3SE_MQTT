from PIL import Image
import os
import glob
import re

def color_to_rgb565(r, g, b):
    r5 = (r * 31 + 127) // 255
    g6 = (g * 63 + 127) // 255
    b5 = (b * 31 + 127) // 255
    return (r5 << 11) | (g6 << 5) | b5

def get_num(filename):
    m = re.search(r'_(\d+)\.jpg$', filename)
    return int(m.group(1)) if m else 0

project_dir = r'C:\Users\User\Documents\project_hp2024\M5_PDM_PLOT'
icons_dir = os.path.join(project_dir, 'PNG2026')
icons_data = []

# Get all .jpg files from the icons directory and sort them numerically
jpg_files = sorted(glob.glob(os.path.join(icons_dir, '*.jpg')), key=get_num)

num_icons = len(jpg_files)
print(f"Found {num_icons} icons in {icons_dir}")

target_size = 78

for filename in jpg_files:
    print(f"Processing {os.path.basename(filename)}...")
    img = Image.open(filename).convert('RGB')
    
    if img.size != (target_size, target_size):
        img = img.resize((target_size, target_size), Image.LANCZOS)
    
    data = []
    for y in range(target_size):
        for x in range(target_size):
            r, g, b = img.getpixel((x, y))
            data.append(color_to_rgb565(r, g, b))
    icons_data.append(data)

header_path = os.path.join(project_dir, 'icons_2026.h')
with open(header_path, 'w') as f:
    f.write('#ifndef ICONS_2026_H\n#define ICONS_2026_H\n\n')
    f.write(f'const uint16_t icons_2026[{num_icons}][{target_size * target_size}] = {{\n')
    for icon in icons_data:
        f.write('  {')
        f.write(','.join(f"0x{d:04x}" for d in icon))
        f.write('},\n')
    f.write('};\n\n#endif\n')

print(f"Generated icons_2026.h with {num_icons} icons")
