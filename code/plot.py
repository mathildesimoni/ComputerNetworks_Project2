log_file = open("../analysis/CWND.csv", 'r')
times = []
cwnds = []

for l in log_file:
    line = log_file.readline().strip().split(",")
    tmp_time = line[0].split(":")[1:]
    if len(tmp_time) == 0: # endo of file
    	break
    time = int(tmp_time[0])*60*1000000 + int(tmp_time[1])*1000000 + int(tmp_time[2])
    # print(time)
    cwnd = float(line[1])
    # print(cwnd)
    times.append(time)
    cwnds.append(cwnd)


# print(times)
# print(cwnds)

adapt_times = []
for i in range(len(times)):
    tmp = times[i] - times[0]
    adapt_times.append(tmp)
# print(adapt_times)

# import numpy as np
import matplotlib.pyplot as plt

fig = plt.figure(figsize=(15,10), facecolor='w')
ax = plt.gca()

plt.plot(adapt_times, cwnds, lw=2, color='r')

plt.ylabel("CWND")
plt.xlabel("Time (us)")
# plt.xlim([0,300])
plt.grid(True, which="both")
plt.savefig("../analysis/plot_cwnd.pdf",dpi=1000,bbox_inches='tight')
# plt.show()
