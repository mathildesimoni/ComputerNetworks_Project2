import matplotlib.pyplot as plt

log_file = open("../analysis/CWND.csv", 'r')
times = []
cwnds = []

# get content from CWND.csv file
content = log_file.readlines()
for l in content:
	line = l.strip().split(",")
	tmp_time = line[0].split(":")[1:]
	if len(tmp_time) == 0: # endo of file
		break
	time = int(tmp_time[0])*60*1000000 + int(tmp_time[1])*1000000 + int(tmp_time[2])
	cwnd = float(line[1])
	times.append(time)
	cwnds.append(cwnd)

# move the times to get a baseline at time t = 0	
adapt_times = []
for i in range(len(times)):
    tmp = times[i] - times[0]
    adapt_times.append(tmp)

# plot 1: based on real time (unit of time: us)
fig = plt.figure(figsize=(15,10), facecolor='w')
ax = plt.gca()

plt.plot(adapt_times, cwnds, lw=2, color='r')

plt.ylabel("CWND (window size)")
plt.xlabel("Time (us)")
plt.yticks(range(int(min(cwnds)), int(max(cwnds)), int(max(cwnds)/10)+1))
plt.grid(True, which="both")
plt.savefig("../analysis/plot_cwnd.pdf",dpi=1000,bbox_inches='tight')


#plot 2: based on arbitrary time (unit of time = CWND update)
xs = range(len(cwnds))
cwnds_2 = [cwnds[0]]
adapt_times_2 = [xs[0]]
minus = 0 # to update a pretty graph similar to lecture slides
for i in range(1, len(xs)):
	if cwnds[i] > cwnds[i-1]:
		cwnds_2.append(cwnds[i])
		adapt_times_2.append(xs[i] - minus)
	else:
		minus += 0.99
		cwnds_2.append(cwnds[i])
		adapt_times_2.append(xs[i]- minus)

# plot
fig = plt.figure(figsize=(15,10), facecolor='w')
ax = plt.gca()

plt.plot(adapt_times_2, cwnds_2, lw=2, color='r')

plt.ylabel("CWND (window size)")
plt.xlabel("Time (CWND update)")
plt.yticks(range(int(min(cwnds_2)), int(max(cwnds_2)), int(max(cwnds_2)/10)+1))
plt.grid(True, which="both")
plt.savefig("../analysis/plot_cwnd_2.pdf",dpi=1000,bbox_inches='tight')





