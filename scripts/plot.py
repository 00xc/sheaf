#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause

import sys
from collections import defaultdict
from matplotlib import pyplot as plt

if __name__ == "__main__":
	lines = defaultdict(list)

	with open(sys.argv[1], "r") as f:
		keys = f.readline().rstrip().split(',')
		for experiment in f:
			experiment = experiment.rstrip().split(',')
			vals = {key: data for key, data in zip(keys, experiment)}

			relax = "yield" if vals["parameter_yield"] == "1" else "pause"
			cc = vals.get('parameter_cc', None)
			page_size = vals.get('parameter_page_size', None)

			name = f"{vals['parameter_impl']}-{relax}"
			if page_size:
				 name += f'-{hex(int(page_size.rstrip("UL"), 0))}'
			if cc:
				name += f'-{cc}'
			lines[name].append(vals)

	# fastest = lines["sl-yield"]
	# for (k, line) in lines.items():
	# 	x = [int(v["parameter_threads"]) for v in line]
	# 	y = [float(v["mean"])/float(f["mean"]) for (v, f) in zip(line, fastest)]
	# 	plt.errorbar(x, y, label=k)

	# plt.grid()
	# plt.legend()
	# plt.xlabel("Threads")
	# plt.ylabel("Runtime (+%best)")
	# plt.show()

	for (name, line) in lines.items():
		x = [int(v["parameter_threads"]) for v in line]
		y = [float(v["mean"]) for v in line]
		yerr = [float(v["stddev"]) for v in line]
		plt.errorbar(x, y, yerr=yerr, label=name)

	plt.grid()
	plt.legend()
	plt.xlabel("Threads")
	plt.ylabel("Runtime (mean +/- std)")
	plt.show()
