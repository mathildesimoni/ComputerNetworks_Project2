# ComputerNetworks_Project2

This is the repository for project 2 in Computer Networks: TCP Congestion Protocol


### Instructions to Run Code

1. Place the file to send in the folder "data_send/"
2. Open a terminal window in the "code/" folder, and run `make`
3. Open 2 terminal windows in the "obj/" folder
4. In the first window, run the receiver: `./rdt_receiver <port_number> <received_file_name>`
5. In the second window, run the sender: `./rdt_sender <source_IP> <port_number> <source_file_name>` (or MAHIMAHI)
6. The file received should be now available in the folder "data_recv/"
7. A CSV file *CWND.csv* was also added in the "analysis/" folder. It shows how the CWND varies over time
8. In the "code/" folder, run `python3 plot.py`
9. Two graphs were added to the "analysis/" folder, showing the evolution of CWND over time (milliseconds for *plot_cwnd.pdf* and CWND updates for *plot_cwnd_2.pdf*)