from PIL import Image, ImageDraw

# Funkcja do rysowania pikseli na podstawie listy współrzędnych (x, y)
def draw_pixels(draw, pixel_list, color=0):  # color=0 to czarny w trybie '1'
    for (x, y) in pixel_list:
        draw.point((x, y), fill=color)

def create_brush():
    """Pędzel - skośny z wyraźną końcówką i trzonkiem"""
    img = Image.new('1', (32, 32), 1)  # 1 = białe tło
    draw = ImageDraw.Draw(img)
    
    # Główna końcówka pędzla (włosie)
    brush_tip = [
        (20, 8), (21, 9), (22, 10), (23, 11), (24, 12),
        (19, 9), (20, 10), (21, 11), (22, 12), (23, 13),
        (18, 10), (19, 11), (20, 12), (21, 13), (22, 14),
        (17, 11), (18, 12), (19, 13), (20, 14), (21, 15),
        (16, 12), (17, 13), (18, 14), (19, 15), (20, 16),
    ]
    
    # Skuwka (metalowa część)
    ferrule = [
        (14, 14), (15, 15), (16, 16), (17, 17),
        (13, 15), (14, 16), (15, 17), (16, 18),
        (12, 16), (13, 17), (14, 18), (15, 19),
    ]
    
    # Trzonek
    handle = [
        (10, 18), (11, 19), (12, 20), (13, 21), (14, 22), (15, 23),
        (9, 19), (10, 20), (11, 21), (12, 22), (13, 23), (14, 24),
        (8, 20), (9, 21), (10, 22), (11, 23), (12, 24), (13, 25),
    ]
    
    draw_pixels(draw, brush_tip)
    draw_pixels(draw, ferrule)
    draw_pixels(draw, handle)
    
    return img

def create_spray():
    """Spray - puszka z chmurką kropek"""
    img = Image.new('1', (32, 32), 1)
    draw = ImageDraw.Draw(img)
    
    # Korpus puszki
    for y in range(10, 26):
        draw.point((10, y), fill=0)
        draw.point((20, y), fill=0)
    for x in range(10, 21):
        draw.point((x, 10), fill=0)
        draw.point((x, 25), fill=0)
    
    # Górna część (dysza)
    for x in range(14, 18):
        for y in range(6, 10):
            draw.point((x, y), fill=0)
    
    # Chmurka sprayu (rozproszone kropki)
    spray_cloud = [
        (22, 12), (23, 11), (24, 13), (25, 10), (26, 12),
        (22, 15), (23, 16), (24, 14), (25, 17), (26, 15),
        (22, 18), (23, 19), (24, 20), (25, 18), (26, 19),
    ]
    draw_pixels(draw, spray_cloud)
    
    return img

def create_eraser():
    """Gumka - trójwymiarowy blok pod kątem"""
    img = Image.new('1', (32, 32), 1)
    draw = ImageDraw.Draw(img)
    
    # Górna krawędź
    for i in range(8):
        draw.point((6 + i, 10 + i), fill=0)
    
    # Dolna krawędź
    for i in range(8):
        draw.point((14 + i, 18 + i), fill=0)
    
    # Lewa krawędź (pionowa)
    for y in range(10, 19):
        draw.point((6, y), fill=0)
    
    # Prawa krawędź (pionowa)
    for y in range(18, 27):
        draw.point((14, y), fill=0)
    
    # Linia podziału (charakterystyczna dla gumki)
    for i in range(6):
        draw.point((8 + i, 12 + i), fill=0)
    
    # Wypełnienie górnej części (ciemniejszy odcień w czerni)
    for i in range(1, 7):
        for j in range(1, i + 1):
            draw.point((6 + i, 10 + j), fill=0)
    
    return img

def create_bucket():
    """Wiadro - przechylone z wylewającą się farbą"""
    img = Image.new('1', (32, 32), 1)
    draw = ImageDraw.Draw(img)
    
    # Lewa krawędź wiadra
    for y in range(10, 21):
        draw.point((10, y), fill=0)
    
    # Prawa krawędź wiadra
    for y in range(15, 26):
        draw.point((22, y), fill=0)
    
    # Górna krawędź (skośna)
    for i in range(13):
        draw.point((10 + i, 10 + i//3), fill=0)
    
    # Dolna krawędź (skośna)
    for i in range(13):
        draw.point((10 + i, 20 + i//3), fill=0)
    
    # Uchwyt
    draw.point((16, 6), fill=0)
    draw.point((17, 7), fill=0)
    draw.point((18, 8), fill=0)
    draw.point((19, 9), fill=0)
    
    # Strumień farby
    paint_stream = [
        (18, 22), (19, 23), (20, 24), (21, 25),
        (19, 22), (20, 23), (21, 24), (22, 25),
    ]
    draw_pixels(draw, paint_stream)
    
    # Kropla
    draw.point((23, 26), fill=0)
    draw.point((24, 26), fill=0)
    draw.point((23, 27), fill=0)
    draw.point((24, 27), fill=0)
    
    return img

def create_roller():
    """Roller - wałek malarski w kształcie T"""
    img = Image.new('1', (32, 32), 1)
    draw = ImageDraw.Draw(img)
    
    # Głowica wałka (pozioma)
    for x in range(8, 24):
        draw.point((x, 8), fill=0)
        draw.point((x, 12), fill=0)
    
    # Wypełnienie głowicy
    for x in range(9, 23):
        for y in range(9, 12):
            draw.point((x, y), fill=0)
    
    # Uchwyt (zakrzywiony)
    curve_points = [(20, 12), (19, 13), (18, 14), (17, 15), (16, 16)]
    for (x, y) in curve_points:
        draw.point((x, y), fill=0)
    
    # Rączka (pionowa)
    for y in range(16, 26):
        draw.point((16, y), fill=0)
        draw.point((15, y), fill=0)
    
    return img

def create_compass():
    """Cyrkiel - odwrócone V z punktem i rysikiem"""
    img = Image.new('1', (32, 32), 1)
    draw = ImageDraw.Draw(img)
    
    # Lewa noga (igła) - od środka do lewego dołu
    for i in range(20):
        x = 16 - i
        y = 8 + i
        if 0 <= x < 32 and 0 <= y < 32:
            draw.point((x, y), fill=0)
    
    # Prawa noga (z rysikiem) - od środka do prawego dołu
    for i in range(20):
        x = 16 + i
        y = 8 + i
        if 0 <= x < 32 and 0 <= y < 32:
            draw.point((x, y), fill=0)
    
    # Zaostrzenie igły (lewa noga)
    draw.point((6, 28), fill=0)
    draw.point((7, 27), fill=0)
    
    # Głowica (zawias)
    for x in range(14, 19):
        for y in range(6, 9):
            draw.point((x, y), fill=0)
    
    # Małe kółko na szczycie do trzymania
    draw.point((16, 4), fill=0)
    draw.point((15, 5), fill=0)
    draw.point((16, 5), fill=0)
    draw.point((17, 5), fill=0)
    
    return img

def main():
    """Główna funkcja generująca wszystkie ikony"""
    icons = [
        ("pedzel", create_brush()),
        ("spray", create_spray()),
        ("gumka", create_eraser()),
        ("wiadro", create_bucket()),
        ("roller", create_roller()),
        ("cyrkiel", create_compass()),
    ]
    
    for name, img in icons:
        filename = f"{name}_32x32.png"
        img.save(filename)
        print(f"Zapisano: {filename}")
    
    # Dodatkowo: stworzenie kolażu wszystkich ikon obok siebie
    collage = Image.new('1', (32 * 6, 32), 1)
    x_offset = 0
    for name, img in icons:
        collage.paste(img, (x_offset, 0))
        x_offset += 32
    
    collage.save("wszystkie_ikony_kolaz.png")
    print("Zapisano również kolaż: wszystkie_ikony_kolaz.png")

if __name__ == "__main__":
    main()