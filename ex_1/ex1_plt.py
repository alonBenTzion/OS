import matplotlib.pyplot as plt
import numpy as np

# Set the run times for each function on each platform (in seconds)
local_times = [0.594, 4.386, 483.51]
vm_times = [0.511,12.564, 695.256]
container_times = [0.27, 2.029, 480.017]

# Set the names of the functions and platforms
functions = ['Arithmetic_op', 'Function_call', 'Sys_call']
platforms = ['Local', 'VM', 'Container']

# Create a numpy array of the run times for each function on each platform
times = np.array([local_times, vm_times, container_times])

# Set the width of the bars
bar_width = 0.25

# Set the x locations of the bars for each platform
r1 = np.arange(len(functions))
r2 = [x + bar_width for x in r1]
r3 = [x + 2*bar_width for x in r1]

fig, ax = plt.subplots()
ax.set_yscale('log')
ax.bar(r1, times[0], color='r', width=bar_width, edgecolor='white', label=platforms[0])
ax.bar(r2, times[1], color='g', width=bar_width, edgecolor='white', label=platforms[1])
ax.bar(r3, times[2], color='b', width=bar_width, edgecolor='white', label=platforms[2])

# Add labels and legend
plt.xlabel('Functions')
plt.ylabel('Time (s)')
plt.xticks([r + bar_width for r in range(len(functions))], functions)
plt.title('Function Run Times(nano_s) on Different Platforms')
plt.legend()

# Save the plot
plt.savefig("/Users/roy/Desktop/ex1_plot.png")
