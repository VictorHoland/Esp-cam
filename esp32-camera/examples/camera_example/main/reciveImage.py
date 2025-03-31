import serial

PORT = "COM3"  # Ajuste conforme necessário
BAUDRATE = 115200
OUTPUT_FILE = "imagem_recebida.jpg"

def receive_image():
    ser = serial.Serial(PORT, BAUDRATE, timeout=10)
    with open(OUTPUT_FILE, "wb") as f:
        buffer = b""
        receiving = False

        while True:
            line = ser.readline().strip()
            if line.startswith(b"IMG_START:"):
                receiving = True
                size = int(line.split(b":")[1])
                print(f"Recebendo imagem de {size} bytes...")
                continue
            elif line == b"IMG_END":
                print("Recebimento concluído!")
                break
            
            if receiving:
                f.write(line)

    ser.close()
    print(f"Imagem salva como {OUTPUT_FILE}")

if __name__ == "__main__":
    print('opa')
    receive_image()
