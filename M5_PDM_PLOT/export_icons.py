from PIL import Image, ImageDraw
import math
import os

# Colors
DARK_BG = (16, 16, 16)
ICON_GREEN = (0, 255, 0)
ICON_RED = (255, 0, 0)
ICON_YELLOW = (255, 255, 0)
ICON_FRAME = (123, 190, 239)
BLACK = (0, 0, 0)
DARKGREY = (64, 64, 64)

def draw_icon(index, size=50):
    img = Image.new('RGB', (size, size), DARK_BG)
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    
    draw.rectangle([0, 0, size-1, size-1], outline=ICON_FRAME)

    if index == 0: # MOTOR OK
        draw.rounded_rectangle([cx-12, cy-8, cx+12, cy+8], radius=3, fill=ICON_GREEN)
        draw.rectangle([cx+12, cy-4, cx+16, cy+4], fill=ICON_GREEN)
        draw.ellipse([cx+4, cy-16, cx+16, cy-4], fill=DARK_BG, outline=ICON_GREEN)
        draw.line([cx+8, cy-10, cx+10, cy-8], fill=ICON_GREEN, width=1)
        draw.line([cx+10, cy-8, cx+13, cy-12], fill=ICON_GREEN, width=1)
    elif index == 1: # MOTOR UNBAL
        draw.rounded_rectangle([cx-12, cy-8, cx+12, cy+8], radius=3, fill=ICON_RED)
        for j in [-1, 1]:
            vx = cx + (j * 18)
            draw.line([vx, cy-10, vx+2, cy-5, vx, cy, vx+2, cy+5], fill=ICON_RED, width=1)
    elif index == 2: # BEAR. FAULT
        draw.ellipse([cx-16, cy-16, cx+16, cy+16], outline=ICON_RED)
        draw.ellipse([cx-10, cy-10, cx+10, cy+10], outline=ICON_RED)
        for a in range(0, 360, 45):
            rad = math.radians(a)
            bx, by = cx + math.cos(rad)*13, cy + math.sin(rad)*13
            draw.ellipse([bx-2, by-2, bx+2, by+2], fill=ICON_RED)
        draw.line([cx+12, cy-12, cx+18, cy-18], fill=ICON_RED)
    elif index == 3: # HOUS. FAIL
        draw.rounded_rectangle([cx-12, cy-12, cx+12, cy+12], radius=4, fill=ICON_RED)
        draw.line([cx, cy-12, cx-4, cy, cx+2, cy+12], fill=BLACK)
    elif index == 4: # BASE LOOSE
        draw.rounded_rectangle([cx-12, cy-10, cx+12, cy+6], radius=3, fill=ICON_RED)
        draw.rectangle([cx-18, cy+8, cx+18, cy+12], fill=ICON_RED)
        draw.polygon([cx-15, cy+14, cx-18, cy+18, cx-12, cy+18], fill=ICON_RED)
        draw.polygon([cx+15, cy+14, cx+12, cy+18, cx+18, cy+18], fill=ICON_RED)
    elif index == 5: # AXIAL SLIP
        draw.rectangle([cx-15, cy-4, cx-5, cy+4], fill=DARKGREY)
        draw.rectangle([cx+5, cy-4, cx+15, cy+4], fill=DARKGREY)
        draw.rectangle([cx-5, cy-6, cx+5, cy+6], fill=ICON_RED)
        draw.line([cx-22, cy+10, cx-10, cy+10], fill=ICON_RED)
        draw.polygon([cx-22, cy+10, cx-18, cy+8, cx-18, cy+12], fill=ICON_RED)
        draw.line([cx+22, cy+10, cx+10, cy+10], fill=ICON_RED)
        draw.polygon([cx+22, cy+10, cx+18, cy+8, cx+18, cy+12], fill=ICON_RED)
    elif index == 6: # MISALIGN
        draw.rectangle([cx-18, cy-8, cx-3, cy+2], fill=ICON_YELLOW)
        draw.rectangle([cx+3, cy+2, cx+18, cy+12], fill=ICON_YELLOW)
        draw.line([cx-3, cy-15, cx-3, cy+15], fill=ICON_YELLOW)
    elif index == 7: # CAVIT.
        draw.ellipse([cx-18, cy-14, cx+10, cy+14], outline=ICON_YELLOW)
        draw.rectangle([cx+10, cy-8, cx+18, cy+8], fill=ICON_YELLOW)
        for b in range(5):
            bx, by = cx - 8 + (b*3), cy - 4 + (b%3*4)
            draw.ellipse([bx-1, by-1, bx+1, by+1], outline=ICON_YELLOW)
    elif index == 8: # RESONANCE
        draw.rounded_rectangle([cx-10, cy-8, cx+10, cy+8], radius=2, fill=ICON_YELLOW)
        for w in range(2):
            wy = cy + 12 + w*5
            for wx in range(cx - 15, cx + 15, 4):
                draw.line([(wx, wy), (wx+2, wy+2), (wx+4, wy)], fill=ICON_YELLOW)
    elif index == 9: # LUBRIC.
        draw.polygon([cx-15, cy+10, cx+5, cy+10, cx-5, cy-5], fill=ICON_YELLOW)
        draw.line([cx+5, cy+10, cx+15, cy], fill=ICON_YELLOW, width=2)
        draw.ellipse([cx+13, cy+6, cx+17, cy+10], fill=ICON_YELLOW)
        draw.polygon([cx+15, cy+4, cx+13, cy+8, cx+17, cy+8], fill=ICON_YELLOW)

    return img

target_dir = r'C:\Users\cyroa\OneDrive\Documents\project\M5_PDM_PLOT'
print(f"Current working directory: {os.getcwd()}")
print(f"Target directory: {target_dir}")

for i in range(10):
    filename = os.path.join(target_dir, f"icon_{i}.png")
    draw_icon(i).save(filename)
    if os.path.exists(filename):
        print(f"Successfully saved {filename}")
    else:
        print(f"Failed to save {filename}")
