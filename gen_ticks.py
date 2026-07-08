import random

with open("ticks.csv", "w") as f:
    price = 18500.00
    for _ in range(100000):
        price += random.choice([-0.25, 0.0, 0.25])
        volume = random.randint(1, 50)
        aggressor = random.choice([1, -1])
        f.write(f"{price:.2f},{volume},{aggressor}\n")
print("[+] ticks.csv создан!")