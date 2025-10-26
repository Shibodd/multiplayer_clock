import pandas as pd
import matplotlib.pyplot as plt

import numpy as np

df = pd.read_csv("logs_0/offsets.csv", names=["system", "steady", "offset"])

val = np.gradient(df["system"], df["steady"])

x = (df["steady"] - df["steady"][0]) / 1e9
y = (df["system"] - df["system"][0]) / 1e9

plt.plot(x, y - x)
plt.show()