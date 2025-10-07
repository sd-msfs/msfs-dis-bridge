import pandas as pd
import matplotlib.pyplot as plt
import os
import time

log_file = "../build/bin/Release/dis_log.csv"
last_mod_time = 0

def generate_plots():
    try:
        df = pd.read_csv(log_file, names=[
            "timestamp", "entityID", "locX", "locY", "locZ",
            "psi", "theta", "phi", "velX", "velY", "velZ"
        ])
    except Exception as e:
        print("Error reading log:", e)
        return

    if df.empty:
        print("No data yet...")
        return

    # Compute relative time in seconds
    df["time"] = (df["timestamp"] - df["timestamp"].iloc[0]) / 1000.0

    # Save altitude plot
    plt.figure()
    plt.plot(df["time"], df["locZ"])
    plt.title("Altitude vs Time")
    plt.xlabel("Time (s)")
    plt.ylabel("Altitude (Z)")
    plt.grid(True)
    plt.savefig("altitude_vs_time.png")
    plt.close()

    # Save heading plot
    plt.figure()
    plt.plot(df["time"], df["psi"])
    plt.title("Heading (Psi) vs Time")
    plt.xlabel("Time (s)")
    plt.ylabel("Heading (degrees)")
    plt.grid(True)
    plt.savefig("heading_vs_time.png")
    plt.close()

    # Save 3D trajectory
    from mpl_toolkits.mplot3d import Axes3D
    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')
    ax.plot(df["locX"], df["locY"], df["locZ"])
    ax.set_title("3D Flight Trajectory")
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    plt.savefig("trajectory_3d.png")
    plt.close()

    print("Updated plots at", time.strftime("%H:%M:%S"))

print(f"Watching {log_file} for updates...")
while True:
    if os.path.exists(log_file):
        mod_time = os.path.getmtime(log_file)
        if mod_time != last_mod_time:
            last_mod_time = mod_time
            generate_plots()
    else:
        print("Waiting for log file...")
    time.sleep(2)