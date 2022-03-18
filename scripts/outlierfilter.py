from curses import raw
from itertools import count
from tkinter import W
import numpy as np
import csv
def outlier_filter(datas, threshold = 2):
    datas = np.array(datas)
    z = np.abs((datas - datas.mean()) / datas.std())
    return datas[z < threshold]

def data_processing(data_set, n):
    catgories = data_set[0].shape[0]
    samples = data_set[0].shape[1]
    final = np.zeros((catgories, samples))

    for c in range(catgories):
        for s in range(samples):
            final[c][s] =                                                    \
                outlier_filter([data_set[i][c][s] for i in range(n)]).mean()
    return final

data = np.ndarray((1, 3, 101, 100), dtype=np.double)
with open("time.txt", newline='\n') as time:
    i = 0
    for row in csv.reader(time, delimiter=" "):
        data[0, 0, int(row[0]), i] = row[1]
        data[0, 1, int(row[0]), i] = row[2]
        data[0, 2, int(row[0]), i] = row[3]
        i += 1
        i %= 100

filtered = data_processing(data_set=data, n=1)
filtered = filtered.transpose()
filtered = filtered.astype(dtype=int)

with open("time_filtered.txt", 'w', newline='\n') as time:
    writer = csv.writer(time, delimiter=' ')
    for row in filtered:
        writer.writerow([row[0], row[1], row[2]])
