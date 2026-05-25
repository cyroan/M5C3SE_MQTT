from PIL import Image
import os

def color_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

target_dir = r'C:\Users\cyroa\OneDrive\Documents\project\M5_PDM_PLOT'
icons_data = []

for i in range(10):
    filename = os.path.join(target_dir, f"icon_{i}.png")
    if not os.path.exists(filename):
        print(f"Warning: {filename} not found. Using black icon.")
        icons_data.append([0] * 2500)
        continue
        
    img = Image.open(filename).convert('RGB')
    img = img.resize((50, 50), Image.LANCZOS)
    
    data = []
    for y in range(50):
        for x in range(50):
            pixel = img.getpixel((x, y))
            data.append(color_to_rgb565(*pixel))
    icons_data.append(data)

header_path = os.path.join(target_dir, 'icons.h')
with open(header_path, 'w') as f:
    f.write('const uint16_t icons[10][2500] = {\n')
    for icon in icons_data:
        f.write('  {')
        f.write(','.join(hex(d) for d in icon))
        f.write('},\n')
    f.write('};\n')

print(f"Successfully re-imported icons into {header_path}")
