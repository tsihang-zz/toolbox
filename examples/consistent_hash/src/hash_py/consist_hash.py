#! /bin/python2

from hashlib import md5
from struct import unpack_from
from bisect import bisect_left

ITEMS = 1000000
NODES = 100

node_stat = [0 for i in range(NODES)]
ring = []
hash2node = {}

def _hash(value):
    k = md5(str(value)).digest()
    ha = unpack_from(">I", k)[0]
    return ha

for n in range(NODES):
    h = _hash(n)
    ring.append(h)
    ring.sort()
    hash2node[h] = n

for item in range(ITEMS):
    h = _hash(item)
    n = bisect_left(ring, h) % NODES
    node_stat[hash2node[ring[n]]] += 1


_ave = ITEMS / NODES
_max = max(node_stat)
_min = min(node_stat)

print("Ave: %d" % _ave)
print("Max: %d\t(%0.2f%%)" % (_max, (_max - _ave) * 100.0 / _ave))
print("Min: %d\t(%0.2f%%)" % (_min, (_ave - _min) * 100.0 / _ave))


# Test Node Change

print "When a new node added ..."

new_ring = []
change = 0
NEW_NODES = 101

for n in range(NEW_NODES):
    new_ring.append(_hash(n))
    new_ring.sort()

for item in range(ITEMS):
    h = _hash(item)
    n = bisect_left(ring, h) % NODES
    new_n = bisect_left(new_ring, h) % NEW_NODES
    if new_n != n:
        change += 1

print("Change: %d\t(%0.2f%%)" % (change, change * 100.0 / ITEMS))
