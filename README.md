# cpuidle_bench

```bash
	CPU 0
idle ratio: 0.8327
- state 0
  avg: 2
  total: 1399307623
- state 1
  avg: 255
  total: 1055953234
- state 2
  avg: 203
  total: 1549042812
- state 3
  avg: 346
  total: 1521860126
- state 4
  avg: 447
  total: 1309932
- state 5
  avg: 2857
  total: 287003681151
````

This program gets information from the [cpuidle](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpuidle.html) subsystem and prints it in a human-readable way.

Output includes:

- Ratio between the time spent in idle states per unit of time and the unit of time
- Available idle states
- Average idle duration for certain state per CPU (in microseconds)
- Total time spent in certain idle state per CPU (in microseconds)

## License

This work is licensed under the [GNU General Public License version 3](COPYING) or any later version.
