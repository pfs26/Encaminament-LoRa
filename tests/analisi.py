import re
import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime

# Nom del fitxer amb el log
# Si genera error de "frozen codecs", eliminar les primeres files generades per picocom
NOM_LOG = "test5Minuts/listener.log"

# Aplicar regex per buscar sleep stats al log
pattern = re.compile(
    r'\[(?P<timestamp>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{6})\].*?\[SLEEP-STATS\]'
    r'\s*Sync: (?P<sync_flag>\d+)'
    r'\s*Sleep time: (?P<sleep_time>\d+)\s*ms'
    r'\s*Sync time: (?P<sync_time>\d+)\s*ms'
    r'\s*Done time: (?P<done_time>\d+)\s*ms'
    r'\s*Delta Time: (?P<delta_time>\d+)\s*ms'
    r'\s*Clk correction: (?P<clk_correction>\d+)\s*ms'
)

matches = []
with open(NOM_LOG, "r") as f:
    for line in f:
        match = pattern.search(line)
        if match:
            data = match.groupdict()
            data['timestamp'] = datetime.strptime(data['timestamp'], "%Y-%m-%d %H:%M:%S.%f")
            for key in data:
                if key != 'timestamp':
                    data[key] = float(data[key])
            matches.append(data)

# Eliminem la primera entrada (1r SYNC), que no serà valida (sempre actiu fins 1r sync)
df = pd.DataFrame(matches)[1:len(matches)//1]

df["cicle_real"] = df["timestamp"].diff().dt.total_seconds() * 1000  # en ms

on = df["sync_flag"] == 1
sync_rate = on.sum() / len(df) * 100
print(f"SYNC success rate: {on.sum() / len(df) * 100:.2f}%")

fig, axes = plt.subplots(nrows=4, ncols=1, figsize=(14, 18), sharex=True)
fig.suptitle("LoRa Sleep & SYNC Layer Analysis", fontsize=16)


timestamps = df["timestamp"]
flags = df["sync_flag"]

start_idx = 0

for i in range(1, len(df)):
    if flags.iloc[i] != flags.iloc[i - 1] or i == len(df) - 1:
        end_idx = i

        if i == len(df) - 1:
            end_idx = i + 1

        estat = flags.iloc[start_idx]
        color = 'green' if estat == 1 else 'red'

        axes[0].fill_between(
            timestamps.iloc[start_idx:end_idx],
            0,
            1,
            color=color,
            alpha=0.2,
            step='post'
        )

        axes[0].step(
            timestamps.iloc[start_idx:end_idx],
            flags.iloc[start_idx:end_idx],
            where='post',
            color=color,
            linewidth=1.5
        )

        start_idx = i

axes[0].set_ylabel("SYNC?")
axes[0].set_yticks([0, 1])
axes[0].set_yticklabels(["No", "Sí"])
axes[0].set_title(f"Sync status. Rate {sync_rate:.2f}%")
axes[0].grid(True)

axes[0].scatter(
    df["timestamp"],
    df["sync_flag"],
    c=df["sync_flag"].map({0: "red", 1: "green"}),
    s=10, 
    marker='o'
)


# sync/done time
axes[1].plot(df["timestamp"], df["done_time"], label="Done Time")
axes[1].plot(df["timestamp"], df["sync_time"], label="Sync Time")
axes[1].axhline(df["done_time"].mean(), color='red', linestyle='--', label=f"Done mean ({df['done_time'].mean():.2f} ms)")
axes[1].set_ylabel("Time (ms)")
axes[1].legend()
axes[1].grid(True)
axes[1].set_title("Time metrics")

# temps de cicle
df["cycle_time"] = df["done_time"] + df["sleep_time"]
axes[2].plot(df["timestamp"], df["cycle_time"], label="Expected Cycle")
axes[2].plot(df["timestamp"], df["cicle_real"], label="Actual Cycle")
axes[2].axhline(df["cicle_real"].mean(), color='red', linestyle='--', label=f"Actual cycle mean ({df['cicle_real'].mean():.2f} ms)")
axes[2].legend()
axes[2].grid(True)
axes[2].set_title("Cycle time")


df["cycle_error"] = df["cycle_time"] - df["cicle_real"] 
axes[3].plot(df["timestamp"], df["cycle_error"], label="Cycle Time Error")
axes[3].axhline(df["cycle_error"].mean(), color='red', linestyle='--', label=f"Mean {df['cycle_error'].mean():.2f} ms")
axes[3].legend()
axes[3].grid(True)
axes[3].set_title("Cycle time error")
plt.show()

pd.set_option('display.max_rows', None)
pd.set_option('display.max_columns', None)
print(df)



CONSUM_NORMAL = 78.3  # mA
CONSUM_SLEEP = 11.3   # mA

# Calcula el consum d'un cicle, a partir del temps de cicle i del temps done
df["consum_cicle"] = ((df["done_time"] / 1000) * CONSUM_NORMAL + (df["sleep_time"] / 1000) * CONSUM_SLEEP)/3600

# plot consum
fig, ax = plt.subplots(nrows=2, ncols=1, figsize=(14, 6))
ax[0].plot(df["timestamp"], df["consum_cicle"], label="Consum per cicle", color='blue')
ax[0].axhline(df["consum_cicle"].mean(), color='red', linestyle='--', label=f"Mean {df['consum_cicle'].mean():.2f} mA")
ax[0].set_ylabel("Consumption (mA)")
ax[0].set_title("Consum per cicle")
ax[0].legend()
ax[0].grid(True)

# Grafic consum esperat per diferents cicles
T_VALUES = [i*60 for i in range(1, 60)]  # Temps a calcular consum, en ms. 
DONE_TIME = df["done_time"].mean()  # in ms
expected_consumptions = {}
normal_consumptions = {T: T*CONSUM_NORMAL/3600 for T in T_VALUES}
reduccions = {}
for T in T_VALUES:
    expected_consumptions[T] = CONSUM_NORMAL * (DONE_TIME / 1000)/3600  + CONSUM_SLEEP* (T - DONE_TIME/1000) / 3600
    reduccions[T] = (normal_consumptions[T] - expected_consumptions[T]) / normal_consumptions[T] * 100
    
ax[1].plot([i/60 for i in reduccions.keys()], reduccions.values(), label="Reducció consum", color='orange')
ax[1].legend()
ax[1].grid(True)
ax[1].set_xlabel("Temps cicle (minuts)")
ax[1].set_ylabel("Reducció (%)")
ax[1].set_title("Reducció consum per diferents cicles")
ax[1].axvline(df["cycle_time"].mean() / 60000, color='red', linestyle='--', label='Mitjana cicle real')
ax[1].legend()
plt.show()

print(f"Average cycle consumption: {df['consum_cicle'].mean():.2f} mA")