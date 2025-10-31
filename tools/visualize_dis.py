import pandas as pd
import matplotlib.pyplot as plt

log_file = "../build/bin/Release/dis_log.csv"

def visualize_dis():
    try:
        df = pd.read_csv(log_file, header=None, names=[
            "timestamp", "entityID", "longitude", "latitude", "altitude",
            "heading", "pitch", "bank"
        ])
    except Exception as e:
        print("Error reading CSV:", e)
        return

    if df.empty:
        print("No data found in CSV")
        return

    # Compute relative time in seconds
    df["time"] = (df["timestamp"] - df["timestamp"].iloc[0]) / 1000.0

    # Altitude vs Time
    plt.figure()
    plt.plot(df["time"], df["altitude"])
    plt.title("Altitude vs Time")
    plt.xlabel("Time (s)")
    plt.ylabel("Altitude (m)")
    plt.grid(True)
    plt.show()

    #include other graphs if needed by copying the format above but changing the variables

if __name__ == "__main__":
    visualize_dis()
