"""Check the level-zero motor against synthetic regression data, no old source."""
import csv
import math
import struct
import subprocess
import sys

with open(sys.argv[1], newline='') as f:
    expected = list(csv.DictReader(f))
actual = subprocess.run([sys.argv[2]], check=True, stdout=subprocess.PIPE).stdout
assert len(expected) == 1200 and len(actual) == 1200 * 9
for i, (row, (pitch, yaw, fire)) in enumerate(zip(expected, struct.iter_unpack('<ffB', actual))):
    assert math.isclose(pitch, float(row['pitch']), abs_tol=0.001), (i, 'pitch')
    assert math.isclose(yaw, float(row['yaw']), abs_tol=0.001), (i, 'yaw')
    assert fire == int(row['fire']), (i, 'fire')
print('PASS: 1200 synthetic level-zero pitch/yaw/fire records; angle tolerance 0.001 degree.')
