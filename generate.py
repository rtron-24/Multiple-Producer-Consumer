#!/usr/bin/python
import sys, random

def generate(l, m):
    arr = []
    for it in 'PCD':
        arr.extend([it]*l*m)

    random.shuffle(arr)
    print(arr)

    with open('inpfile.txt','w') as fil:
        fil.write(''.join(arr))


if __name__ == "__main__":
    if len(sys.argv)<3:
        print('Few arguments given')
        sys.exit(0)
    l = int(sys.argv[1])
    m = int(sys.argv[2])

    generate(l, m)

