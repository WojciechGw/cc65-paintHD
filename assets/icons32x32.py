from PIL import Image, ImageDraw

from PIL import Image, ImageDraw

def create_icons():
    # Tworzymy zestaw 6 ikon 32x32, tryb '1' (1-bit)
    icons = {
        "pedzel": "M 8,24 L 24,8 M 22,6 L 26,10 L 22,14 L 18,10 Z",
        "spray": "M 10,10 H 22 V 26 H 10 Z M 14,6 H 18 V 10 H 14 Z",
        "gumka": "M 6,18 L 14,10 L 26,10 L 18,18 Z",
        "wiadro": "M 10,10 L 20,10 L 24,20 L 14,20 Z",
        "roller": "M 8,6 H 24 V 12 H 8 Z M 16,12 V 26",
        "cyrkiel": "M 16,6 L 10,26 M 16,6 L 22,26"
    }

def create_paint_icons():
    # Ustawienia: 32x32, tryb '1' (1-bitowy: czarny i biały)
    size = (32, 32)
    black = 0
    white = 1

    icons = {
        "1_pedzel.png": "brush",
        "2_spray.png": "spray",
        "3_gumka.png": "eraser",
        "4_wiadro.png": "bucket",
        "5_roller.png": "roller",
        "6_cyrkiel.png": "compass"
    }

    for filename, type in icons.items():
        # Tworzymy białe tło
        img = Image.new('1', size, white)
        draw = ImageDraw.Draw(img)

        if type == "brush":
            # Trzonek (linia ukośna)
            draw.line([ (6, 25), (16, 15)], fill=black, width=2)
            # Skuwka (metalowa część)
            draw.polygon([(15, 16), (19, 12), (22, 15), (18, 19)], fill=black)
            # Włosie (szersza końcówka)
            draw.polygon([(20, 13), (27, 6), (30, 9), (23, 16)], fill=black)

        elif type == "spray":
            # Puszka (korpus)
            draw.rectangle([10, 12, 22, 28], outline=black, fill=white)
            # Przycisk u góry
            draw.rectangle([14, 8, 18, 12], fill=black)
            # "Chmurka" sprayu (rozproszone piksele)
            dots = [(24, 7), (26, 5), (28, 8), (25, 10), (28, 12), (23, 5), (30, 6)]
            for dot in dots:
                draw.point(dot, fill=black)

        elif type == "eraser":
            # Bryła gumki (perspektywa izometryczna)
            draw.polygon([(6, 18), (18, 10), (28, 15), (16, 23)], outline=black, fill=white)
            # Boczna ścianka dla efektu 3D
            draw.line([(6, 18), (6, 22), (16, 27), (16, 23)], fill=black)
            draw.line([(16, 27), (28, 19), (28, 15)], fill=black)
            # Linia podziału (dwukolorowa gumka)
            draw.line([(12, 14), (22, 19)], fill=black)

        elif type == "bucket":
            # Wiadro (przechylone)
            draw.polygon([(8, 12), (18, 8), (24, 20), (14, 24)], outline=black, fill=white)
            # Wylewająca się farba (kropla)
            draw.polygon([(24, 20), (28, 28), (22, 26)], fill=black)
            # Rączka (pałąk)
            draw.arc([6, 4, 20, 18], start=180, end=0, fill=black)

        elif type == "roller":
            # Wałek (część malująca)
            draw.rectangle([8, 6, 24, 14], outline=black, fill=black)
            # Metalowy pręt
            draw.line([(24, 10), (28, 10), (28, 18), (16, 18), (16, 20)], fill=black)
            # Rączka (uchwyt)
            draw.rectangle([14, 20, 18, 28], fill=black)

        elif type == "compass":
            # Główny zawias
            draw.rectangle([14, 4, 18, 7], fill=black)
            # Lewa noga (igła)
            draw.line([(16, 6), (10, 26)], fill=black)
            draw.point((10, 27), fill=black)
            # Prawa noga
            draw.line([(16, 6), (22, 26)], fill=black)
            # Uchwyt na ołówek
            draw.rectangle([21, 22, 24, 26], outline=black, fill=white)
            draw.line([(22, 26), (22, 28)], fill=black)

        # Zapisywanie pliku
        img.save(filename)
        print(f"Wygenerowano: {filename}")

if __name__ == "__main__":
    create_paint_icons()