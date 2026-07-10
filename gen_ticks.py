import random
from datetime import datetime, timedelta

current_time = datetime.now()

with open("ticks.csv", "w") as f:
    price = 18500.00
    for _ in range(100000):
        current_time += timedelta(seconds=random.randint(1, 5))
        time_str = current_time.strftime("%H:%M:%S")
        price += random.choice([-0.25, 0.0, 0.25])
        volume = random.randint(1, 50)
        aggressor = random.choice([1, -1])
        f.write(f"{time_str},{price:.2f},{volume},{aggressor}\n")
print("[+] ticks.csv с таймстампами готов!")