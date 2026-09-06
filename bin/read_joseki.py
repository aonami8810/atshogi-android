import struct
import os

def check_joseki():
    filename = "static_joseki.bin"
    if not os.path.exists(filename):
        print(f"Error: {filename} not found.")
        return

    file_size = os.path.getsize(filename)
    print(f"Checking {filename} (Size: {file_size} bytes)")

    with open(filename, "rb") as f:
        data = f.read(10368) # sizeof(LocalTensorContextSoA)

    if len(data) < 10368:
        print("Error: File is smaller than expected LocalTensorContextSoA size (10368 bytes).")
        return

    # pot: 16 * 81 floats = 1296 floats
    pot_data = data[:1296 * 4]
    potentials = struct.unpack(f"{1296}f", pot_data)
    
    # potentials is a flat list of 16 * 81 floats.
    # potentials[lane][idx] = potentials[lane * 81 + idx]
    for lane in range(4):
        pots = [potentials[lane * 81 + i] for i in range(6)]
        print(f"Lane {lane}: pot[0..5]=[{pots[0]:.6f}, {pots[1]:.6f}, {pots[2]:.6f}, {pots[3]:.6f}, {pots[4]:.6f}, {pots[5]:.6f}]")

if __name__ == "__main__":
    check_joseki()
