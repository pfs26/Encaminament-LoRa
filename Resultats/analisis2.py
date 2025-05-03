import re
import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime

# ----------------------
# 1. Parse log file
# ----------------------

pattern = re.compile(
    r'\[(?P<timestamp>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\].*?\[SLEEP-STATS\]'
    r'\s*Sync: (?P<sync_flag>\d+)'
    r'.*?Sleep time: (?P<sleep_time>\d+)\s*ms'
    r'\s*Sync time: (?P<sync_time>\d+)\s*ms'
    r'\s*Done time: (?P<done_time>\d+)\s*ms'
    r'\s*Delta Time: (?P<delta_time>\d+)\s*ms'
    r'\s*Sync AVG: (?P<sync_avg>\d+\.\d+)\s*ms'
    r'\s*Done AVG: (?P<done_avg>\d+\.\d+)\s*ms'
    r'\s*Sync count: (?P<sync_count>\d+)'
    r'\s*Boot count: (?P<boot_count>\d+)'
)
matches = []
with open("30sec_MACAcceptACKRandom/020525_sleep30secRANDOM.log", "r") as f:
    for line in f:
        match = pattern.search(line)
        if match:
            data = match.groupdict()
            # Convert types
            data['timestamp'] = datetime.strptime(data['timestamp'], "%Y-%m-%d %H:%M:%S")
            for key in data:
                if key != 'timestamp':
                    data[key] = float(data[key])
            matches.append(data)

# Eliminem les primeres dues entrades (1r SYNC i quan no coneixem delta) per no afectar mitjana i std
df = pd.DataFrame(matches)[2:]
# df["timestamp"] = df.index

# Rolling averages for smoother plots
df["rolling_sync_time"] = df["sync_time"].rolling(window=5).mean()
df["rolling_done_time"] = df["done_time"].rolling(window=5).mean()

df["sync_std"] = df["sync_time"].rolling(window=5).std()
df["done_std"] = df["done_time"].rolling(window=5).std()
df["delta_std"] = df["delta_time"].rolling(window=5).std()


# ----------------------
# 2. Plot everything in subplots
# ----------------------

fig, axes = plt.subplots(nrows=3, ncols=1, figsize=(14, 18), sharex=True)
fig.suptitle("LoRa Sleep & SYNC Layer - Full Analysis", fontsize=16)


# Subplot 2: SYNC flag (success/failure)
axes[0].plot(df["timestamp"], df["sync_flag"], marker='o', linestyle='', color='green')
axes[0].set_ylabel("SYNC OK")
axes[0].set_yticks([0, 1])
axes[0].grid(True)
axes[0].set_title("SYNC Success (1) / Timeout (0)")

# Subplot 1: Core timing metrics
# axes[0].plot(df["timestamp"], df["sleep_time"], label="Sleep Time")
axes[1].plot(df["timestamp"], df["done_time"], label="Done Time")
axes[1].plot(df["timestamp"], df["sync_time"], label="Sync Time")
axes[1].set_ylabel("Time (ms)")
axes[1].legend()
axes[1].grid(True)
axes[1].set_title("Core Timing Metrics")

# Subplot 3: Rolling averages
axes[2].plot(df["timestamp"], df["rolling_sync_time"], label="Rolling Sync Time (5)")
axes[2].plot(df["timestamp"], df["rolling_done_time"], label="Rolling Done Time (5)")
axes[2].set_ylabel("Time (ms)")
axes[2].legend()
axes[2].grid(True)
axes[2].set_title("Rolling Averages (5-cycle window)")

# Subplot 4: Logged moving averages
# axes[3].plot(df["timestamp"], df["sync_avg"], label="Logged Sync AVG")
# axes[3].plot(df["timestamp"], df["done_avg"], label="Logged Done AVG")
# axes[3].set_ylabel("Time (ms)")
# axes[3].legend()
# axes[3].grid(True)
# axes[3].set_title("Device-Logged Moving Averages")

# # Subplot 5: Sync & Boot counts
# axes[4].plot(df["timestamp"], df["sync_count"], label="Sync Count")
# axes[4].plot(df["timestamp"], df["boot_count"], label="Boot Count")
# axes[4].set_ylabel("Count")
# axes[4].legend()
# axes[4].grid(True)
# axes[4].set_title("Sync & Boot Count Evolution")

# Subplot 6: Delta Time Stability
# axes[3].plot(df["timestamp"], df["delta_time"], color='orange', label="Delta Time")
# axes[3].set_xlabel("Cycle #")
# axes[3].set_ylabel("Delta (ms)")
# axes[3].grid(True)
# axes[3].legend()
# axes[3].set_title("Delta Time Over Cycles")

# axes[4].plot(df["timestamp"], df["sync_std"], label="Sync Time STD")
# axes[4].plot(df["timestamp"], df["done_std"], label="Done Time STD")
# axes[4].plot(df["timestamp"], df["delta_std"], label="Delta Time STD")
# axes[4].set_ylabel("Std Dev (ms)")
# axes[4].legend()
# axes[4].grid(True)
# axes[4].set_title("Rolling Std Dev (5-cycle window)")

# Layout adjustments
plt.tight_layout(rect=[0, 0, 1, 0.97])  # Leave space for suptitle
plt.show()

# df[["sync_time", "done_time", "delta_time"]].hist(bins=50, figsize=(12, 6), layout=(1, 3))
# plt.suptitle("Timing Distributions")
# plt.tight_layout()
# plt.show()

# plt.figure(figsize=(10, 6))
# plt.boxplot([df["sync_time"], df["done_time"], df["delta_time"]], labels=["Sync", "Done", "Delta"])
# plt.title("Boxplot of Timing Metrics")
# plt.ylabel("Milliseconds")
# plt.grid(True)
# plt.show()

# df["sync_success_rate"] = df["sync_flag"].rolling(window=10).mean()
# plt.figure(figsize=(10, 4))
# plt.plot(df["timestamp"], df["sync_success_rate"], color="green")
# plt.title("Rolling SYNC Success Rate (Window=10)")
# plt.ylabel("Success Ratio")
# plt.xlabel("Cycle")
# plt.grid(True)
# plt.ylim(0, 1.05)
# plt.show()

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
plt.show()
