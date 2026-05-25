from PIL import Image, ImageChops

def color_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

DARK_BG = (30, 30, 30) # Dark grey

def trim_and_center_dark(img, target_size=50):
    # Convert to RGB if not already
    img = img.convert('RGB')
    
    # Find bounding box of content (not white)
    bg_white = Image.new('RGB', img.size, (255, 255, 255))
    diff = ImageChops.difference(img, bg_white)
    bbox = diff.getbbox()
    if not bbox:
        return Image.new("RGB", (target_size, target_size), DARK_BG)
    
    # Crop to content
    cropped = img.crop(bbox)
    
    # Resize keeping aspect ratio
    w, h = cropped.size
    ratio = min(target_size / w, target_size / h)
    new_w = int(w * ratio)
    new_h = int(h * ratio)
    resized = cropped.resize((new_w, new_h), Image.LANCZOS)
    
    # Create new DARK background and paste centered
    # We also need to replace the remaining white in 'resized' with DARK_BG
    # A simple way is to use a mask or just iterate pixels
    
    final = Image.new("RGB", (target_size, target_size), DARK_BG)
    
    # Iterate through resized image and replace "mostly white" with DARK_BG
    # and "mostly non-white" with its color.
    # To avoid artifacts, we can just paste it and then replace white in the final image
    offset = ((target_size - new_w) // 2, (target_size - new_h) // 2)
    final.paste(resized, offset)
    
    pixels = final.load()
    for y in range(target_size):
        for x in range(target_size):
            r, g, b = pixels[x, y]
            # If it's very bright (near white), make it dark bg
            if r > 240 and g > 240 and b > 240:
                pixels[x, y] = DARK_BG
                
    return final

img = Image.open('C:/Users/cyroa/OneDrive/Documents/project/M5_PDM_PLOT/ChatGPT.png').convert('RGB')
w, h = img.size

cols = 5
rows = 2
dw = w // cols
dh = h // rows

icons_data = []

for r in range(rows):
    for c in range(cols):
        left = c * dw
        top = r * dh
        right = left + dw
        bottom = top + dh
        
        region = img.crop((left, top, right, bottom))
        
        # Crop the box part
        inner_padding_w = dw // 12
        inner_padding_h = dh // 12
        icon_box = region.crop((inner_padding_w, inner_padding_h, dw - inner_padding_w, dh - dh // 4))
        
        # Clean up and resize for dark theme
        refined = trim_and_center_dark(icon_box, 50)
        
        data = []
        for y in range(50):
            for x in range(50):
                pixel = refined.getpixel((x, y))
                data.append(color_to_rgb565(*pixel))
        icons_data.append(data)

with open('icons.h', 'w') as f:
    f.write('const uint16_t icons[10][2500] = {\n')
    for icon in icons_data:
        f.write('  {')
        f.write(','.join(hex(d) for d in icon))
        f.write('},\n')
    f.write('};\n')

print("Dark theme icons.h generated from ChatGPT.png")
