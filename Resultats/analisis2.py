import re
import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime

# ----------------------
# 1. Parse log file
# ----------------------
pattern = re.compile(
    r'\[(?P<timestamp>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{6})\].*?\[SLEEP-STATS\]'
    r'\s*Sync: (?P<sync_flag>\d+)'
    r'\s*Sleep time: (?P<sleep_time>\d+)\s*ms'
    r'\s*Sync time: (?P<sync_time>\d+)\s*ms'
    r'\s*Done time: (?P<done_time>\d+)\s*ms'
    r'\s*Delta Time: (?P<delta_time>\d+)\s*ms'
    r'\s*Sync AVG: (?P<sync_avg>\d+\.\d+)\s*ms'
    r'\s*Done AVG: (?P<done_avg>\d+\.\d+)\s*ms'
    r'\s*Sync count: (?P<sync_count>\d+)'
    r'\s*Boot count: (?P<boot_count>\d+)'
    r'\s*Timeout:\s*(?P<timeout>\d+)\s*ms'
)

matches = []
with open("1h_ClockCorregit/filtrat.log", "r") as f:
    for line in f:
        match = pattern.search(line)
        if match:
            data = match.groupdict()
            # Convert types
            data['timestamp'] = datetime.strptime(data['timestamp'], "%Y-%m-%d %H:%M:%S.%f")
            for key in data:
                if key != 'timestamp':
                    data[key] = float(data[key])
            matches.append(data)

# Eliminem les primeres dues entrades (1r SYNC) per no afectar mitjana i std
# NO, el primer ja no l'agafa ja que no verifica regex (hi ha valor `nan`)
df = pd.DataFrame(matches)
# df["timestamp"] = df.index

on = df["sync_flag"] == 1
sync_rate = on.sum() / len(df) * 100
print(f"SYNC success rate: {on.sum() / len(df) * 100:.2f}%")

# ----------------------
# 2. Plot everything in subplots
# ----------------------
fig, axes = plt.subplots(nrows=4, ncols=1, figsize=(14, 18), sharex=True)
fig.suptitle("LoRa Sleep & SYNC Layer - Full Analysis", fontsize=16)


# Subplot 2: SYNC flag (success/failure)
axes[0].plot(df["timestamp"], df["sync_flag"], marker='o', linestyle='', color='green')
axes[0].set_ylabel("SYNC?")
axes[0].set_yticks([0, 1])
axes[0].grid(True)
axes[0].set_title(f"Sync status. Rate {sync_rate:.2f}%")

# Subplot 1: Core timing metrics
# axes[0].plot(df["timestamp"], df["sleep_time"], label="Sleep Time")
axes[1].plot(df["timestamp"], df["done_time"], label="Done Time")
axes[1].plot(df["timestamp"], df["sync_time"], label="Sync Time")
axes[1].set_ylabel("Time (ms)")
axes[1].legend()
axes[1].grid(True)
axes[1].set_title("Core Timing Metrics")

# plot the sum of sync, done and sleep times on axes[3]
df["cycle_time"] = df["done_time"] + df["sleep_time"]
axes[2].plot(df["timestamp"], df["cycle_time"], label="Cycle Time")
# axes[3].plot(df["timestamp"], df["done_time"]+df["sleep_time"]-df["sync_time"], label="Total Time")
axes[2].axhline(df["cycle_time"].mean(), color='red', linestyle='--', label="Mean")
axes[2].legend()
axes[2].grid(True)
axes[2].set_title("Cycle time")


EXPECTED_CYCLE = 3600 * 1000 # ms
df["cycle_error"] = df["cycle_time"] - EXPECTED_CYCLE
axes[3].plot(df["timestamp"], df["cycle_error"], label="Cycle Time Error")
axes[3].axhline(df["cycle_error"].mean(), color='red', linestyle='--', label=f"Mean {df['cycle_error'].mean():.2f} ms")
axes[3].legend()
axes[3].grid(True)
axes[3].set_title("Cycle time error")
plt.show()

threshold = df["sync_time"].mean() + 2 * df["sync_time"].std()
lower_threshold = df["sync_time"].mean() - 2 * df["sync_time"].std()
anomalies = df[(df["sync_time"] > threshold) | (df["sync_time"] < lower_threshold)]
print("Anomalous SYNC times:")
print(anomalies[["timestamp", "sync_time"]])
plt.figure(figsize=(12, 6))
plt.plot(df["timestamp"], df["sync_time"], label="Sync Time", color="blue")
plt.axhline(threshold, color="red", linestyle="--", label=f"Threshold ({threshold:.2f} ms)")
plt.axhline(lower_threshold, color="red", linestyle="--", label=f"Threshold ({lower_threshold:.2f} ms)")
plt.scatter(anomalies["timestamp"], anomalies["sync_time"], color="orange", label="Anomalies")
plt.title("Sync Time with Anomaly Detection")
plt.xlabel("Cycle")
plt.ylabel("Sync Time (ms)")
plt.legend()
plt.grid(True)
plt.tight_layout()

mean_delta = df["delta_time"].mean()
# PPM = 25000 
# df["tout"] = 26.57 * 1000 # TOUT obtingut a partir de debug (imprès per consola)
# plt.plot(df["timestamp"], df["tout"], label="Tout", color="purple")
plt.figure(figsize=(12, 6))
plt.plot(df["timestamp"], df["sync_time"], label="Sync Time", color="blue")
plt.axhline(df["delta_time"].mean(), color="green", linestyle="--", label=f"Delta Time ({df['delta_time'].mean():.2f} ms)")
plt.axhline(df["timeout"].mean(), color="red", linestyle="--", label=f"Timeout ({df["timeout"].mean():.2f} ms)")
# plt.axhline(df["timeout"].mean()-8000, color="red", linestyle="--", label=f"Timeout ({df["tout"].mean()-8000:.2f} ms)")
plt.axhline(df["sync_time"].mean(), color="blue", linestyle="--", label=f"Mean Sync ({df["sync_time"].mean():.2f} ms)")
plt.axhline(df["sync_time"].max(), color="orange", linestyle="--", label=f"Max Sync ({df["sync_time"].max():.2f} ms)")
plt.axhline(df["sync_time"].min(), color="orange", linestyle="--", label=f"Max Sync ({df["sync_time"].min():.2f} ms)")
plt.legend()


df["working_time"] = df["done_time"] - df["sync_time"]
plt.figure(figsize=(12, 6))
plt.plot(df["timestamp"], df["working_time"], label="Working Time", color="blue")
plt.axhline(df["working_time"].mean(), color="green", linestyle="--", label=f"AVG ({df['working_time'].mean():.2f} ms)")
plt.legend()
plt.title("Working Time Consistency")

plt.show()

