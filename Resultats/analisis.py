import re
import matplotlib.pyplot as plt

def parse_log_file(file_path):
    sync_times = []
    done_times = []
    sleep_times = []
    sync_avgs = []
    done_avgs = []
    sync_status = []
    timestamps = []

    with open(file_path, 'r') as file:
        for line in file:
            match = re.search(r'\[SLEEP-STATS\] Sync: (\d+)\s+Sleep time: (\d+) ms\s+Sync time: (\d+) ms\s+Done time: (\d+) ms\s+.*Sync AVG: ([\d\.]+) ms\s+Done AVG: ([\d\.]+) ms', line)
            if match:
                sync_status.append(int(match.group(1)))
                sleep_times.append(int(match.group(2)))
                sync_times.append(int(match.group(3)))
                done_times.append(int(match.group(4)))
                sync_avgs.append(float(match.group(5)))
                done_avgs.append(float(match.group(6)))

                # Extract timestamp from the log line
                timestamp_match = re.search(r'\[I\]\s+(\d+)', line)
                if timestamp_match:
                    timestamps.append(int(timestamp_match.group(1)))

    return timestamps, sync_status, sleep_times, sync_times, done_times, sync_avgs, done_avgs

def plot_data(timestamps, sync_status, sleep_times, sync_times, done_times, sync_avgs, done_avgs):
    x = range(1, len(sync_times) + 1)

    plt.figure(figsize=(14, 10))

    # Plot sync status
    plt.subplot(4, 1, 1)
    plt.plot(x, sync_status, label='Sync Status (1=Synced)', marker='o', linestyle="", color='blue')
    plt.title('Sync Status Over Time')
    plt.xlabel('Entry Index')
    plt.ylabel('Sync Status')
    plt.legend()

    # Plot sync and done times
    plt.subplot(4, 1, 2)
    plt.plot(x, sync_times, label='Sync Time', marker='', color='green')
    plt.plot(x, done_times, label='Done Time', marker='', color='orange')
    plt.title('Sync and Done Times')
    plt.xlabel('Entry Index')
    plt.ylabel('Time (ms)')
    plt.legend()

    # Plot averages
    plt.subplot(4, 1, 3)
    plt.plot(x, sync_avgs, label='Average Sync Time', linestyle='--', color='green')
    plt.plot(x, done_avgs, label='Average Done Time', linestyle='--', color='orange')
    plt.title('Average Sync and Done Times')
    plt.xlabel('Entry Index')
    plt.ylabel('Time (ms)')
    plt.legend()

    # Plot sleep times
    plt.subplot(4, 1, 4)
    plt.plot(x, sleep_times, label='Sleep Time', marker='', color='purple')
    plt.title('Sleep Time Over Time')
    plt.xlabel('Entry Index')
    plt.ylabel('Time (ms)')
    plt.legend()

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    log_file_path = "30sec_MACAcceptACKRandom/020525_sleep30secRANDOM.log"  # Change this to the desired log file
    timestamps, sync_status, sleep_times, sync_times, done_times, sync_avgs, done_avgs = parse_log_file(log_file_path)
    plot_data(timestamps, sync_status, sleep_times, sync_times, done_times, sync_avgs, done_avgs)