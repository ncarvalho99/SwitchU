import os
from PIL import Image, ImageDraw

def create_guest_avatar(filename, skin_color, hair_color, hair_style, eye_style, mouth_style, blush=True, glasses=False, glasses_color=(40,40,40)):
    # Render at 4x (512x512) for supersampling / smooth antialiasing
    scale = 4
    canvas_size = (128 * scale, 128 * scale)
    img = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    cx, cy = 64 * scale, 68 * scale
    face_w, face_h = 76 * scale, 80 * scale
    
    # 1. Back hair (for styles that extend behind face)
    if hair_style in ["pigtails", "bob", "wavy", "long"]:
        if hair_style == "pigtails":
            draw.ellipse([cx - 48*scale, cy - 20*scale, cx - 24*scale, cy + 16*scale], fill=hair_color)
            draw.ellipse([cx + 24*scale, cy - 20*scale, cx + 48*scale, cy + 16*scale], fill=hair_color)
            # Ribbons
            draw.ellipse([cx - 38*scale, cy - 16*scale, cx - 30*scale, cy - 8*scale], fill=(240, 60, 100))
            draw.ellipse([cx + 30*scale, cy - 16*scale, cx + 38*scale, cy - 8*scale], fill=(240, 60, 100))
        elif hair_style == "bob":
            draw.rounded_rectangle([cx - 44*scale, cy - 36*scale, cx + 44*scale, cy + 32*scale], radius=24*scale, fill=hair_color)
        elif hair_style == "wavy":
            draw.ellipse([cx - 46*scale, cy - 26*scale, cx - 18*scale, cy + 34*scale], fill=hair_color)
            draw.ellipse([cx + 18*scale, cy - 26*scale, cx + 46*scale, cy + 34*scale], fill=hair_color)
        elif hair_style == "long":
            draw.rounded_rectangle([cx - 42*scale, cy - 36*scale, cx + 42*scale, cy + 44*scale], radius=20*scale, fill=hair_color)

    # 2. Ears
    ear_w, ear_h = 16 * scale, 22 * scale
    draw.ellipse([cx - face_w//2 - 6*scale, cy - 4*scale, cx - face_w//2 + 10*scale, cy + 18*scale], fill=skin_color)
    draw.ellipse([cx + face_w//2 - 10*scale, cy - 4*scale, cx + face_w//2 + 6*scale, cy + 18*scale], fill=skin_color)

    # 3. Head / Face base
    face_rect = [cx - face_w//2, cy - face_h//2, cx + face_w//2, cy + face_h//2]
    draw.ellipse(face_rect, fill=skin_color)
    
    # 4. Blush
    if blush:
        blush_color = (255, 120, 120, 100)
        draw.ellipse([cx - 28*scale, cy + 8*scale, cx - 14*scale, cy + 18*scale], fill=blush_color)
        draw.ellipse([cx + 14*scale, cy + 8*scale, cx + 28*scale, cy + 18*scale], fill=blush_color)

    # 5. Eyebrows
    brow_y = cy - 15 * scale
    brow_color = hair_color if hair_style != "cap" else (40, 30, 25)
    draw.line([(cx - 25*scale, brow_y + 2*scale), (cx - 12*scale, brow_y - 2*scale)], fill=brow_color, width=3*scale)
    draw.line([(cx + 12*scale, brow_y - 2*scale), (cx + 25*scale, brow_y + 2*scale)], fill=brow_color, width=3*scale)

    # 6. Eyes
    eye_y = cy - 4 * scale
    if eye_style == "dots":
        # Classic Mii shiny dot eyes
        draw.ellipse([cx - 23*scale, eye_y - 6*scale, cx - 13*scale, eye_y + 6*scale], fill=(25, 25, 25))
        draw.ellipse([cx + 13*scale, eye_y - 6*scale, cx + 23*scale, eye_y + 6*scale], fill=(25, 25, 25))
        # Eye catchlights
        draw.ellipse([cx - 21*scale, eye_y - 4*scale, cx - 17*scale, eye_y], fill=(255, 255, 255))
        draw.ellipse([cx + 15*scale, eye_y - 4*scale, cx + 19*scale, eye_y], fill=(255, 255, 255))
    elif eye_style == "happy":
        # Crescent smiling eyes
        draw.arc([cx - 25*scale, eye_y - 8*scale, cx - 11*scale, eye_y + 6*scale], start=200, end=340, fill=(25, 25, 25), width=4*scale)
        draw.arc([cx + 11*scale, eye_y - 8*scale, cx + 25*scale, eye_y + 6*scale], start=200, end=340, fill=(25, 25, 25), width=4*scale)
    elif eye_style == "wink":
        # Left eye open dot, right eye wink crescent
        draw.ellipse([cx - 23*scale, eye_y - 6*scale, cx - 13*scale, eye_y + 6*scale], fill=(25, 25, 25))
        draw.ellipse([cx - 21*scale, eye_y - 4*scale, cx - 17*scale, eye_y], fill=(255, 255, 255))
        draw.arc([cx + 11*scale, eye_y - 8*scale, cx + 25*scale, eye_y + 6*scale], start=200, end=340, fill=(25, 25, 25), width=4*scale)
    elif eye_style == "round":
        # Big anime round eyes
        draw.ellipse([cx - 25*scale, eye_y - 9*scale, cx - 11*scale, eye_y + 7*scale], fill=(255, 255, 255), outline=(35, 35, 35), width=2*scale)
        draw.ellipse([cx + 11*scale, eye_y - 9*scale, cx + 25*scale, eye_y + 7*scale], fill=(255, 255, 255), outline=(35, 35, 35), width=2*scale)
        draw.ellipse([cx - 21*scale, eye_y - 6*scale, cx - 14*scale, eye_y + 4*scale], fill=(35, 35, 35))
        draw.ellipse([cx + 14*scale, eye_y - 6*scale, cx + 21*scale, eye_y + 4*scale], fill=(35, 35, 35))
        draw.ellipse([cx - 19*scale, eye_y - 4*scale, cx - 16*scale, eye_y - scale], fill=(255, 255, 255))
        draw.ellipse([cx + 16*scale, eye_y - 4*scale, cx + 19*scale, eye_y - scale], fill=(255, 255, 255))

    # Glasses
    if glasses:
        draw.rounded_rectangle([cx - 28*scale, eye_y - 10*scale, cx - 8*scale, eye_y + 10*scale], radius=5*scale, outline=glasses_color, width=3*scale)
        draw.rounded_rectangle([cx + 8*scale, eye_y - 10*scale, cx + 28*scale, eye_y + 10*scale], radius=5*scale, outline=glasses_color, width=3*scale)
        draw.line([(cx - 8*scale, eye_y), (cx + 8*scale, eye_y)], fill=glasses_color, width=3*scale)

    # 7. Nose
    draw.ellipse([cx - 3*scale, cy + 6*scale, cx + 3*scale, cy + 11*scale], fill=(160, 110, 80))

    # 8. Mouth
    mouth_y = cy + 18 * scale
    if mouth_style == "smile":
        draw.arc([cx - 13*scale, mouth_y - 8*scale, cx + 13*scale, mouth_y + 6*scale], start=20, end=160, fill=(35, 25, 25), width=3*scale)
    elif mouth_style == "grin":
        draw.chord([cx - 15*scale, mouth_y - 6*scale, cx + 15*scale, mouth_y + 12*scale], start=0, end=180, fill=(215, 60, 60))
        draw.chord([cx - 15*scale, mouth_y - 6*scale, cx + 15*scale, mouth_y + 2*scale], start=0, end=180, fill=(255, 255, 255))
        draw.arc([cx - 15*scale, mouth_y - 6*scale, cx + 15*scale, mouth_y + 12*scale], start=0, end=180, fill=(35, 25, 25), width=2*scale)
    elif mouth_style == "open_smile":
        draw.chord([cx - 11*scale, mouth_y - 4*scale, cx + 11*scale, mouth_y + 10*scale], start=0, end=180, fill=(200, 50, 50))
        draw.arc([cx - 11*scale, mouth_y - 4*scale, cx + 11*scale, mouth_y + 10*scale], start=0, end=180, fill=(35, 25, 25), width=2*scale)

    # 9. Front Hair
    hair_top = cy - 46 * scale
    if hair_style == "crop":
        # Smooth modern crop
        draw.ellipse([cx - 42*scale, hair_top, cx + 42*scale, cy - 8*scale], fill=hair_color)
        draw.polygon([(cx - 42*scale, cy - 20*scale), (cx - 20*scale, cy - 14*scale), (cx, cy - 24*scale), (cx + 25*scale, cy - 15*scale), (cx + 42*scale, cy - 20*scale)], fill=hair_color)
    elif hair_style == "spiky":
        # Cool anime spiky hair
        draw.ellipse([cx - 44*scale, hair_top, cx + 44*scale, cy - 8*scale], fill=hair_color)
        spikes = [
            (cx - 42*scale, cy - 18*scale), (cx - 36*scale, cy - 36*scale),
            (cx - 22*scale, cy - 24*scale), (cx - 12*scale, cy - 42*scale),
            (cx, cy - 24*scale), (cx + 14*scale, cy - 44*scale),
            (cx + 24*scale, cy - 25*scale), (cx + 38*scale, cy - 35*scale),
            (cx + 42*scale, cy - 18*scale)
        ]
        draw.polygon(spikes, fill=hair_color)
    elif hair_style == "side_part":
        # Classic elegant side part
        draw.ellipse([cx - 42*scale, hair_top, cx + 42*scale, cy - 8*scale], fill=hair_color)
        draw.polygon([(cx - 42*scale, cy - 18*scale), (cx - 10*scale, cy - 10*scale), (cx + 42*scale, cy - 22*scale)], fill=hair_color)
    elif hair_style == "bangs":
        # Cute straight bangs
        draw.ellipse([cx - 42*scale, hair_top, cx + 42*scale, cy - 6*scale], fill=hair_color)
        draw.rounded_rectangle([cx - 36*scale, cy - 34*scale, cx + 36*scale, cy - 14*scale], radius=6*scale, fill=hair_color)
    elif hair_style == "afro":
        # Stylish full afro
        draw.ellipse([cx - 52*scale, cy - 58*scale, cx + 52*scale, cy + 4*scale], fill=hair_color)
        # Re-cut forehead curve
        draw.chord([cx - 38*scale, cy - 42*scale, cx + 38*scale, cy + 30*scale], start=0, end=180, fill=skin_color)
    elif hair_style == "bob":
        draw.ellipse([cx - 44*scale, hair_top, cx + 44*scale, cy - 8*scale], fill=hair_color)
        draw.polygon([(cx - 42*scale, cy - 18*scale), (cx - 15*scale, cy - 12*scale), (cx + 10*scale, cy - 15*scale), (cx + 42*scale, cy - 18*scale)], fill=hair_color)
    elif hair_style == "pigtails":
        draw.ellipse([cx - 42*scale, hair_top, cx + 42*scale, cy - 8*scale], fill=hair_color)
        draw.rounded_rectangle([cx - 34*scale, cy - 32*scale, cx + 34*scale, cy - 14*scale], radius=6*scale, fill=hair_color)
    elif hair_style == "wavy":
        draw.ellipse([cx - 44*scale, hair_top, cx + 44*scale, cy - 8*scale], fill=hair_color)
        draw.polygon([(cx - 42*scale, cy - 18*scale), (cx - 20*scale, cy - 10*scale), (cx + 20*scale, cy - 10*scale), (cx + 42*scale, cy - 18*scale)], fill=hair_color)
    elif hair_style == "cap":
        # Nintendo Red Cap with brim and white emblem
        draw.ellipse([cx - 44*scale, cy - 54*scale, cx + 44*scale, cy - 10*scale], fill=(225, 35, 35))
        draw.rounded_rectangle([cx - 46*scale, cy - 24*scale, cx + 46*scale, cy - 10*scale], radius=6*scale, fill=(195, 25, 25))
        draw.ellipse([cx - 12*scale, cy - 44*scale, cx + 12*scale, cy - 22*scale], fill=(255, 255, 255))
        draw.polygon([(cx - 4*scale, cy - 40*scale), (cx + 4*scale, cy - 40*scale), (cx, cy - 26*scale)], fill=(225, 35, 35))

    # Downscale smoothly to 128x128 with Lanczos antialiasing
    final_img = img.resize((128, 128), Image.Resampling.LANCZOS)
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    final_img.save(filename, "PNG")

def main():
    dest_dir = "romfs/avatars"
    os.makedirs(dest_dir, exist_ok=True)
    
    guests = [
        # 1. Classic Guest A (Red)
        ("guest_01.png", (255, 224, 189), (40, 30, 25), "crop", "dots", "smile", True, False),
        # 2. Classic Guest B (Blue)
        ("guest_02.png", (255, 209, 170), (70, 40, 20), "side_part", "happy", "grin", True, False),
        # 3. Blonde Girl (Pink)
        ("guest_03.png", (255, 230, 200), (235, 190, 70), "pigtails", "round", "open_smile", True, False),
        # 4. Tan Guy with Spiky Hair (Green)
        ("guest_04.png", (224, 172, 105), (30, 25, 25), "spiky", "dots", "grin", False, False),
        # 5. Cool Girl with Bob Cut & Glasses (Cyan)
        ("guest_05.png", (255, 220, 180), (60, 35, 20), "bob", "dots", "smile", True, True, (40, 40, 40)),
        # 6. Deep Skin with Afro (Yellow)
        ("guest_06.png", (141, 85, 36), (20, 15, 15), "afro", "happy", "grin", False, False),
        # 7. Redhead Boy with Bangs (Orange)
        ("guest_07.png", (255, 225, 190), (190, 70, 30), "bangs", "wink", "smile", True, False),
        # 8. Dark Skin Girl with Wavy Hair (Purple)
        ("guest_08.png", (97, 61, 32), (25, 20, 20), "wavy", "round", "open_smile", True, False),
        # 9. Guy with Cap (Red)
        ("guest_09.png", (245, 195, 150), (50, 35, 25), "cap", "dots", "grin", False, False),
        # 10. Calm Guy with Glasses (Brown)
        ("guest_10.png", (255, 215, 175), (45, 30, 20), "side_part", "dots", "smile", False, True, (180, 40, 40)),
        # 11. Silver Hair Elder / Master (White)
        ("guest_11.png", (245, 210, 180), (200, 200, 205), "crop", "happy", "smile", False, False),
        # 12. Cheerful Girl with Wink (Lime)
        ("guest_12.png", (255, 225, 195), (90, 50, 25), "pigtails", "wink", "grin", True, False),
        # 13. Olive Skin Guy with Spiky Black Hair (Black)
        ("guest_13.png", (184, 115, 51), (25, 20, 20), "spiky", "round", "smile", False, False),
        # 14. Sweet Blonde with Bob (Pink)
        ("guest_14.png", (255, 230, 205), (240, 200, 80), "bob", "happy", "open_smile", True, False),
        # 15. Deep Skin with Glasses (Blue)
        ("guest_15.png", (70, 41, 20), (20, 15, 15), "crop", "dots", "smile", False, True, (30, 120, 200)),
        # 16. Wavy Brown Hair Hero (Orange)
        ("guest_16.png", (255, 218, 185), (80, 45, 25), "wavy", "dots", "grin", True, False),
    ]

    for g in guests:
        name = g[0]
        path = os.path.join(dest_dir, name)
        skin = g[1]
        hair = g[2]
        h_style = g[3]
        e_style = g[4]
        m_style = g[5]
        blush = g[6]
        glasses = g[7]
        g_col = g[8] if len(g) > 8 else (40, 40, 40)
        create_guest_avatar(path, skin, hair, h_style, e_style, m_style, blush, glasses, g_col)
        print(f"Generated {path}")

if __name__ == "__main__":
    main()
