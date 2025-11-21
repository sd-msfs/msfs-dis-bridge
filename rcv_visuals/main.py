import socket
import struct
import matplotlib.pyplot as plt
import numpy as np
from collections import deque
import time
import cartopy.crs as ccrs
import cartopy.feature as cfeature

MCAST_GRP = "239.1.2.3"
MCAST_PORT = 3000
HEADER_LEN = 12
BUFFER_LEN = 100
UPDATE_INTERVAL = 0.03  # seconds (~33 Hz)

# ------------------ Packet parsing ------------------ #
def parse_entity_state(packet):
    cur = HEADER_LEN
    site, app, ent = struct.unpack_from(">HHH", packet, cur); cur += 6
    force_id = packet[cur]; cur += 1
    num_params = packet[cur]; cur += 1
    cur += 16  # skip entity types
    vx, vy, vz = struct.unpack_from(">fff", packet, cur); cur += 12
    lon, lat, alt_val = struct.unpack_from(">ddd", packet, cur); cur += 24
    psi, theta, phi = struct.unpack_from(">fff", packet, cur); cur += 12

    # C++ sends degrees already → do NOT convert
    heading = (psi % 360) - 10
    pitch   = theta
    bank    = phi

    # Airspeed: vx is already scalar (knots)
    airspeed = vx
    alt_ft = alt_val

    return {"lat": lat, "lon": lon, "alt_ft": alt_ft,
            "heading": heading, "pitch": pitch, "bank": bank,
            "airspeed": airspeed}

# ------------------ UDP setup ------------------ #
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("0.0.0.0", MCAST_PORT))
local_ip = socket.gethostbyname(socket.gethostname())
mreq = struct.pack("4s4s", socket.inet_aton(MCAST_GRP), socket.inet_aton(local_ip))
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
sock.setblocking(False)

# ------------------ Matplotlib setup ------------------ #
plt.ion()
fig = plt.figure(figsize=(14, 10))

# Subplots: heading, horizon, altitude, airspeed
ax_heading   = fig.add_subplot(3, 2, 1, projection='polar')
ax_horizon   = fig.add_subplot(3, 2, 2)
ax_alt       = fig.add_subplot(3, 2, 3)
ax_airspeed  = fig.add_subplot(3, 2, 4)

alt_buf = deque(maxlen=BUFFER_LEN)
airspeed_buf = deque(maxlen=BUFFER_LEN)

# Initialize lines
heading_line, = ax_heading.plot([], [], lw=3)
horizon_line, = ax_horizon.plot([], [], lw=3)
alt_line, = ax_alt.plot([], [], lw=2)
airspeed_line, = ax_airspeed.plot([], [], lw=2)

# Heading axis
ax_heading.set_title("Heading")
ax_heading.set_theta_zero_location("N")
ax_heading.set_theta_direction(-1)

# Horizon axis
ax_horizon.set_title("Pitch / Bank Horizon")
ax_horizon.set_xlim(-1,1)
ax_horizon.set_ylim(-1,1)
ax_horizon.set_xticks([])
ax_horizon.set_yticks([])

# Altitude axis
ax_alt.set_title("Altitude (ft)")
ax_alt.set_xlim(0, BUFFER_LEN)
ax_alt.set_ylim(0, 30000)

# Airspeed axis
ax_airspeed.set_title("Airspeed (MPH)")
ax_airspeed.set_xlim(0, BUFFER_LEN)
ax_airspeed.set_ylim(0, 800)

# Heading smoothing
heading_filtered = None
alpha = 0.3  # smoothing factor

# ------------------ Map figure (separate window) ------------------ #
map_fig = plt.figure(figsize=(8, 10))

# Global map
ax_map_global = map_fig.add_subplot(2,1,1, projection=ccrs.PlateCarree())
ax_map_global.set_global()
ax_map_global.add_feature(cfeature.LAND)
ax_map_global.add_feature(cfeature.OCEAN)
ax_map_global.add_feature(cfeature.COASTLINE)
ax_map_global.add_feature(cfeature.BORDERS, linestyle=':')
ax_map_global.set_title("Global Position")
map_scatter_global = ax_map_global.scatter([], [], c='red', s=50, transform=ccrs.PlateCarree())

# Zoomed map (+/- 5 degrees)
ax_map_zoom = map_fig.add_subplot(2,1,2, projection=ccrs.PlateCarree())
ax_map_zoom.add_feature(cfeature.LAND)
ax_map_zoom.add_feature(cfeature.OCEAN)
ax_map_zoom.add_feature(cfeature.COASTLINE)
ax_map_zoom.add_feature(cfeature.RIVERS)
ax_map_zoom.add_feature(cfeature.LAKES)
ax_map_zoom.add_feature(cfeature.BORDERS, linestyle=':')
ax_map_zoom.set_title("Zoomed Position")
map_scatter_zoom = ax_map_zoom.scatter([], [], c='blue', s=50, transform=ccrs.PlateCarree())

current = None
last_update = time.time()
print("Listening for packets. Close window to stop.")

while plt.fignum_exists(fig.number):
    latest_data = None

    # Drain all packets, keep only latest
    while True:
        try:
            data, _ = sock.recvfrom(2048)
            latest_data = data
        except BlockingIOError:
            break

    if latest_data:
        current = parse_entity_state(latest_data)
        alt_buf.append(current["alt_ft"])
        airspeed_buf.append(current["airspeed"] * 1.150779)  # knots → MPH

        if heading_filtered is None:
            heading_filtered = current["heading"]
        else:
            heading_filtered = alpha*current["heading"] + (1-alpha)*heading_filtered

        lon, lat = current["lon"], current["lat"]

    # Update plots at fixed interval
    if current and (time.time() - last_update >= UPDATE_INTERVAL):
        last_update = time.time()

        # Heading
        heading_line.set_data([np.radians(heading_filtered)]*2, [0,1])

        # Horizon
        center_y = -current["pitch"]/90
        x = np.linspace(-1,1,50)
        y = center_y * np.ones_like(x)
        rot = np.radians(current["bank"])
        xr = x*np.cos(rot) - y*np.sin(rot)
        yr = x*np.sin(rot) + y*np.cos(rot)
        horizon_line.set_data(xr, yr)

        # Altitude & airspeed
        alt_line.set_data(range(len(alt_buf)), list(alt_buf))
        airspeed_line.set_data(range(len(airspeed_buf)), list(airspeed_buf))

        # Global map
        map_scatter_global.set_offsets([[lon, lat]])

        # Zoomed map
        ax_map_zoom.set_extent([lon-5, lon+5, lat-5, lat+5], crs=ccrs.PlateCarree())
        map_scatter_zoom.set_offsets([[lon, lat]])

        # Redraw plots
        fig.canvas.draw_idle()
        map_fig.canvas.draw_idle()
        plt.pause(0.001)
    else:
        time.sleep(0.001)
